param([int]$Port = 2024)

$ErrorActionPreference = 'Stop'

function Open-Player($name) {
    $tcp = [Net.Sockets.TcpClient]::new('127.0.0.1', $Port)
    $stream = $tcp.GetStream()
    $stream.ReadTimeout = 3000
    @{
        Name = $name
        Tcp = $tcp
        Stream = $stream
        Reader = [IO.StreamReader]::new($stream)
    }
}

function Send-Line($player, $line) {
    $bytes = [Text.Encoding]::UTF8.GetBytes("$line`n")
    $player.Stream.Write($bytes, 0, $bytes.Length)
}

function Expect-Line($player, $expected, [switch]$Prefix) {
    $line = $player.Reader.ReadLine()
    if ($null -eq $line) {
        throw "$($player.Name): server closed the connection"
    }
    $ok = if ($Prefix) { $line.StartsWith($expected) } else { $line -ceq $expected }
    if (!$ok) {
        throw "$($player.Name): expected '$expected', got '$line'"
    }
    $line
}

function Expect-GameState($turn, [bool]$canMove, $ax, $ay, $bx, $by) {
    $prefix = 'GAME_STATE '
    $stateA = Expect-Line $a $prefix -Prefix
    $stateB = Expect-Line $b $prefix -Prefix
    if ($stateA -cne $stateB) {
        throw "Players received different game states.`nA: $stateA`nB: $stateB"
    }

    $state = $stateA.Substring($prefix.Length) | ConvertFrom-Json
    $p1 = $state.players | Where-Object { $_.id -eq 1 }
    $p2 = $state.players | Where-Object { $_.id -eq 2 }
    if ($state.turn -ne $turn -or
        $state.can_move -isnot [bool] -or $state.can_move -ne $canMove -or
        $state.can_attack -isnot [bool] -or $state.can_attack -ne $true -or
        $p1.x -ne $ax -or $p1.y -ne $ay -or $p2.x -ne $bx -or $p2.y -ne $by) {
        throw "Unexpected game state: $stateA"
    }
}

$a = $null
$b = $null
try {
    $a = Open-Player 'A'
    Send-Line $a 'JOIN Artem'
    $null = Expect-Line $a 'OK JOIN'
    $null = Expect-Line $a 'LOBBY_STATE ' -Prefix
    Send-Line $a 'END_TURN'
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
    Expect-GameState 1 $true 0 0 4 4

    Send-Line $b 'END_TURN'
    $null = Expect-Line $b 'ERR_NOT_YOUR_TURN'
    Send-Line $a 'MOVE 1 0'
    $null = Expect-Line $a 'OK MOVE'
    Expect-GameState 1 $false 1 0 4 4

    Send-Line $a 'END_TURN'
    $null = Expect-Line $a 'OK END_TURN'
    Expect-GameState 2 $true 1 0 4 4
    Send-Line $b 'MOVE 3 4'
    $null = Expect-Line $b 'OK MOVE'
    Expect-GameState 2 $false 1 0 3 4

    Send-Line $b 'END_TURN'
    $null = Expect-Line $b 'OK END_TURN'
    Expect-GameState 1 $true 1 0 3 4
    Send-Line $a 'MOVE 2 0'
    $null = Expect-Line $a 'OK MOVE'
    Expect-GameState 1 $false 2 0 3 4

    $b.Tcp.Dispose()
    $b = $null
    $null = Expect-Line $a 'MATCH_ABORTED'
    $null = Expect-Line $a 'LOBBY_STATE ' -Prefix
    Write-Host 'END_TURN tests passed'
} finally {
    if ($null -ne $b) { $b.Tcp.Dispose() }
    if ($null -ne $a) { $a.Tcp.Dispose() }
}
