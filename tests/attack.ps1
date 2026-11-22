param([int]$Port = 2024)

$ErrorActionPreference = 'Stop'

function Open-Player($name) {
    $tcp = [Net.Sockets.TcpClient]::new('127.0.0.1', $Port)
    $stream = $tcp.GetStream()
    $stream.ReadTimeout = 3000
    @{ Name = $name; Tcp = $tcp; Stream = $stream; Reader = [IO.StreamReader]::new($stream) }
}

function Send-Line($player, $line) {
    $bytes = [Text.Encoding]::UTF8.GetBytes("$line`n")
    $player.Stream.Write($bytes, 0, $bytes.Length)
}

function Expect-Line($player, $expected, [switch]$Prefix) {
    $line = $player.Reader.ReadLine()
    $ok = if ($Prefix) {
        $null -ne $line -and $line.StartsWith($expected)
    } else {
        $line -ceq $expected
    }
    if (!$ok) {
        throw "$($player.Name): expected '$expected', got '$line'"
    }
    return $line
}

function Read-GameState($first, $second) {
    $prefix = 'GAME_STATE '
    $stateA = Expect-Line $first $prefix -Prefix
    $stateB = Expect-Line $second $prefix -Prefix
    if ($stateA -cne $stateB) {
        throw "Players received different game states.`nA: $stateA`nB: $stateB"
    }
    return ($stateA.Substring($prefix.Length) | ConvertFrom-Json)
}

function Pass-Turn($player) {
    Send-Line $player 'END_TURN'
    $null = Expect-Line $player 'OK END_TURN'
}

$a = $null
$b = $null
try {
    $a = Open-Player 'A'
    Send-Line $a 'JOIN Artem'
    $null = Expect-Line $a 'OK JOIN'
    $null = Expect-Line $a 'LOBBY_STATE ' -Prefix
    Send-Line $a 'ATTACK 4 4'
    $null = Expect-Line $a 'ERR_NOT_IN_MATCH'

    $b = Open-Player 'B'
    Send-Line $b 'JOIN Nikita'
    $null = Expect-Line $b 'OK JOIN'
    $null = Expect-Line $a 'LOBBY_STATE ' -Prefix
    $null = Expect-Line $b 'LOBBY_STATE ' -Prefix
    Send-Line $a 'READY'
    $null = Expect-Line $a 'OK READY'
    $null = Expect-Line $a 'LOBBY_STATE ' -Prefix
    $null = Expect-Line $b 'LOBBY_STATE ' -Prefix
    Send-Line $b 'READY'
    $null = Expect-Line $b 'OK READY'
    $null = Expect-Line $a 'MATCH_START'
    $null = Expect-Line $b 'MATCH_START'
    $null = Read-GameState $a $b

    Send-Line $a 'ATTACK 4 4'
    $null = Expect-Line $a 'ERR_INVALID_ATTACK'

    # Walk along the top and right edges. B passes each turn.
    foreach ($cell in '1 0', '2 0', '3 0', '4 0', '4 1', '4 2', '4 3') {
        Send-Line $a "MOVE $cell"
        $null = Expect-Line $a 'OK MOVE'
        $null = Read-GameState $a $b
        Pass-Turn $a
        $null = Read-GameState $a $b
        Pass-Turn $b
        $null = Read-GameState $a $b
    }

    # A and B are adjacent. A has both actions available.
    Send-Line $b 'ATTACK 4 3'
    $null = Expect-Line $b 'ERR_INVALID_ATTACK'
    Send-Line $a 'ATTACK x 4'
    $null = Expect-Line $a 'ERR_INVALID_ATTACK'
    Send-Line $a 'ATTACK 3 3'
    $null = Expect-Line $a 'ERR_INVALID_ATTACK'

    Send-Line $a 'ATTACK 4 4'
    $null = Expect-Line $a 'OK ATTACK'
    $state = Read-GameState $a $b
    $p1 = $state.players | Where-Object { $_.id -eq 1 }
    $p2 = $state.players | Where-Object { $_.id -eq 2 }
    if ($p1.hp -ne 10 -or $p2.hp -ne 9 -or $state.turn -ne 1 -or
        $state.can_attack -isnot [bool] -or $state.can_attack -ne $false -or
        $state.can_move -isnot [bool] -or $state.can_move -ne $true) {
        throw "Unexpected state after ATTACK: $($state | ConvertTo-Json -Compress)"
    }

    Send-Line $a 'ATTACK 4 4'
    $null = Expect-Line $a 'ERR_INVALID_ATTACK'
    Send-Line $a 'MOVE 3 3'
    $null = Expect-Line $a 'OK MOVE'
    $state = Read-GameState $a $b
    $p2 = $state.players | Where-Object { $_.id -eq 2 }
    if ($p2.hp -ne 9) {
        throw "Repeated ATTACK changed player B's HP to $($p2.hp)"
    }

    $b.Tcp.Dispose()
    $b = $null
    $null = Expect-Line $a 'MATCH_ABORTED'
    $null = Expect-Line $a 'LOBBY_STATE ' -Prefix
    Write-Host 'ATTACK tests passed'
} finally {
    if ($null -ne $b) { $b.Tcp.Dispose() }
    if ($null -ne $a) { $a.Tcp.Dispose() }
}
