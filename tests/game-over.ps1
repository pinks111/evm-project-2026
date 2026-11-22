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
$lobbyPrefix = 'LOBBY_STATE '
try {
    $a = Open-Player 'A'
    Send-Line $a 'JOIN Artem'
    $null = Expect-Line $a 'OK JOIN'
    $null = Expect-Line $a $lobbyPrefix -Prefix

    $b = Open-Player 'B'
    Send-Line $b 'JOIN Nikita'
    $null = Expect-Line $b 'OK JOIN'
    $null = Expect-Line $a $lobbyPrefix -Prefix
    $null = Expect-Line $b $lobbyPrefix -Prefix
    Send-Line $a 'READY'
    $null = Expect-Line $a 'OK READY'
    $null = Expect-Line $a $lobbyPrefix -Prefix
    $null = Expect-Line $b $lobbyPrefix -Prefix
    Send-Line $b 'READY'
    $null = Expect-Line $b 'OK READY'
    $null = Expect-Line $a 'MATCH_START'
    $null = Expect-Line $b 'MATCH_START'
    $initialState = Read-GameState $a $b

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

    for ($hp = 9; $hp -ge 0; $hp--) {
        Send-Line $a 'ATTACK 4 4'
        $null = Expect-Line $a 'OK ATTACK'
        $state = Read-GameState $a $b
        $p2 = $state.players | Where-Object { $_.id -eq 2 }
        if ($null -eq $p2.hp -or $p2.hp -ne $hp) {
            throw "Expected player B HP $hp, got $($p2.hp)"
        }
        if ($hp -gt 0) {
            Pass-Turn $a
            $null = Read-GameState $a $b
            Pass-Turn $b
            $null = Read-GameState $a $b
        }
    }

    $null = Expect-Line $a 'GAME_OVER 1'
    $null = Expect-Line $b 'GAME_OVER 1'
    foreach ($player in $a, $b) {
        $line = Expect-Line $player $lobbyPrefix -Prefix
        $lobby = $line.Substring($lobbyPrefix.Length) | ConvertFrom-Json
        $unready = @($lobby.players | Where-Object { $_.ready -is [bool] -and $_.ready -eq $false })
        if (@($lobby.players).Count -ne 2 -or $unready.Count -ne 2) {
            throw "$($player.Name): expected both players unready, got '$line'"
        }
    }

    Send-Line $a 'ATTACK 4 4'
    $null = Expect-Line $a 'ERR_NOT_IN_MATCH'
    Send-Line $a 'READY'
    $null = Expect-Line $a 'OK READY'
    $null = Expect-Line $a $lobbyPrefix -Prefix
    $null = Expect-Line $b $lobbyPrefix -Prefix
    Send-Line $b 'READY'
    $null = Expect-Line $b 'OK READY'
    $null = Expect-Line $a 'MATCH_START'
    $null = Expect-Line $b 'MATCH_START'
    $rematchState = Read-GameState $a $b
    if (($rematchState | ConvertTo-Json -Depth 10 -Compress) -cne
        ($initialState | ConvertTo-Json -Depth 10 -Compress)) {
        throw 'Rematch did not restore the initial game state'
    }

    $b.Tcp.Dispose()
    $b = $null
    $null = Expect-Line $a 'MATCH_ABORTED'
    $null = Expect-Line $a $lobbyPrefix -Prefix
    Write-Host 'GAME_OVER tests passed'
} finally {
    if ($null -ne $b) { $b.Tcp.Dispose() }
    if ($null -ne $a) { $a.Tcp.Dispose() }
}
