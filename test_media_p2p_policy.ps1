param(
    [int]$Port = 18841,
    [int]$TimeoutSec = 20
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$exePath = Join-Path $projectRoot "media_server.exe"
if (!(Test-Path $exePath)) {
    throw "media_server.exe bulunamadi. Once build_media.ps1 calistirin."
}

function Wait-Health {
    param([int]$Port, [int]$TimeoutSec)
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        try {
            $h = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/health") -Method Get -TimeoutSec 2
            if ($h.status -eq "healthy") { return $true }
        } catch {}
        Start-Sleep -Milliseconds 400
    }
    return $false
}

function Start-ProcessWithEnvironment {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [Parameter(Mandatory = $true)][hashtable]$Environment
    )
    $prevValues = @{}
    foreach ($kv in $Environment.GetEnumerator()) {
        $key = [string]$kv.Key
        $prevValues[$key] = [Environment]::GetEnvironmentVariable($key, "Process")
        [Environment]::SetEnvironmentVariable($key, [string]$kv.Value, "Process")
    }
    try {
        return Start-Process -FilePath $FilePath -WorkingDirectory $WorkingDirectory -WindowStyle Hidden -PassThru
    } finally {
        foreach ($kv in $prevValues.GetEnumerator()) {
            [Environment]::SetEnvironmentVariable([string]$kv.Key, $kv.Value, "Process")
        }
    }
}

function Receive-WebSocketText {
    param([System.Net.WebSockets.ClientWebSocket]$Socket, [int]$ReceiveTimeoutSec = 6)
    $buffer = New-Object byte[] 16384
    $segment = [System.ArraySegment[byte]]::new($buffer)
    $cts = [System.Threading.CancellationTokenSource]::new([TimeSpan]::FromSeconds($ReceiveTimeoutSec))
    try {
        $result = $Socket.ReceiveAsync($segment, $cts.Token).GetAwaiter().GetResult()
    } catch {
        return $null
    } finally {
        $cts.Dispose()
    }
    if ($result.Count -le 0) { return "" }
    return [System.Text.Encoding]::UTF8.GetString($buffer, 0, $result.Count)
}

function Wait-Event {
    param(
        [System.Net.WebSockets.ClientWebSocket]$Socket,
        [scriptblock]$Predicate,
        [int]$TimeoutSec = 10
    )
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        $raw = Receive-WebSocketText -Socket $Socket -ReceiveTimeoutSec 2
        if ([string]::IsNullOrWhiteSpace($raw)) { continue }
        try { $evt = $raw | ConvertFrom-Json } catch { continue }
        if (& $Predicate $evt) { return $evt }
    }
    return $null
}

function Send-WebSocketText {
    param([System.Net.WebSockets.ClientWebSocket]$Socket, [string]$Text)
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
    $segment = [System.ArraySegment[byte]]::new($bytes)
    $Socket.SendAsync($segment, [System.Net.WebSockets.WebSocketMessageType]::Text, $true, [System.Threading.CancellationToken]::None).GetAwaiter().GetResult() | Out-Null
}

$server = $null
$ws1 = $null
$ws2 = $null
$ws3 = $null
try {
    $childEnv = @{
        GIGACHAD_SERVER_PORT = "$Port"
        MEDIA_SERVER_PORT = "$Port"
        GIGACHAD_EVENT_BUS = "memory"
        MEDIA_EVENT_BUS = "memory"
        GIGACHAD_API_TOKEN = ""
        MEDIA_API_TOKEN = ""
        GIGACHAD_WS_TOKEN = ""
        MEDIA_WS_TOKEN = ""
        GIGACHAD_MEDIA_P2P_MAX_PEERS = "2"
        MEDIA_P2P_MAX_PEERS = "2"
    }

    $server = Start-ProcessWithEnvironment -FilePath $exePath -WorkingDirectory $projectRoot -Environment $childEnv
    if (!(Wait-Health -Port $Port -TimeoutSec $TimeoutSec)) { throw "Server hazir degil." }

    $room = "policy-room"
    $tenant = "tenant-policy"

    $ws1 = [System.Net.WebSockets.ClientWebSocket]::new()
    $u1 = [System.Uri]("ws://127.0.0.1:$Port/ws?room=$room&user=u1&tenant=$tenant")
    $ws1.ConnectAsync($u1, [System.Threading.CancellationToken]::None).GetAwaiter().GetResult() | Out-Null
    $c1 = Wait-Event -Socket $ws1 -Predicate { param($e) $e.type -eq "connected" } -TimeoutSec 8
    if (-not $c1) { throw "u1 connected eventi gelmedi." }
    if ($c1.mediaMode -ne "p2p") { throw "u1 mediaMode beklenen p2p, gelen=$($c1.mediaMode)" }

    $ws2 = [System.Net.WebSockets.ClientWebSocket]::new()
    $u2 = [System.Uri]("ws://127.0.0.1:$Port/ws?room=$room&user=u2&tenant=$tenant")
    $ws2.ConnectAsync($u2, [System.Threading.CancellationToken]::None).GetAwaiter().GetResult() | Out-Null
    $c2 = Wait-Event -Socket $ws2 -Predicate { param($e) $e.type -eq "connected" } -TimeoutSec 8
    if (-not $c2) { throw "u2 connected eventi gelmedi." }
    if ($c2.mediaMode -ne "p2p") { throw "u2 mediaMode beklenen p2p, gelen=$($c2.mediaMode)" }

    $ws3 = [System.Net.WebSockets.ClientWebSocket]::new()
    $u3 = [System.Uri]("ws://127.0.0.1:$Port/ws?room=$room&user=u3&tenant=$tenant")
    $ws3.ConnectAsync($u3, [System.Threading.CancellationToken]::None).GetAwaiter().GetResult() | Out-Null
    $c3 = Wait-Event -Socket $ws3 -Predicate { param($e) $e.type -eq "connected" } -TimeoutSec 8
    if (-not $c3) { throw "u3 connected eventi gelmedi." }
    if ($c3.mediaMode -ne "sfu") { throw "u3 mediaMode beklenen sfu, gelen=$($c3.mediaMode)" }

    $signalBody = '{"type":"webrtc.signal","targetUser":"u2","signalType":"offer","data":"hello-offer"}'
    Send-WebSocketText -Socket $ws1 -Text $signalBody
    $relay = Wait-Event -Socket $ws2 -Predicate { param($e) $e.type -eq "webrtc.signal" -and $e.payload.from -eq "u1" -and $e.payload.to -eq "u2" -and $e.payload.signalType -eq "offer" -and $e.payload.data -eq "hello-offer" } -TimeoutSec 8
    if (-not $relay) { throw "webrtc.signal relay u1->u2 gelmedi." }
    $ack = Wait-Event -Socket $ws1 -Predicate { param($e) $e.type -eq "webrtc.signal.ack" -and $e.targetUser -eq "u2" } -TimeoutSec 8
    if (-not $ack) { throw "webrtc.signal ack gelmedi." }

    Write-Output "PASS: media p2p/sfu policy + webrtc.signal relay calisti."
}
finally {
    foreach ($ws in @($ws1, $ws2, $ws3)) {
        if ($ws) {
            try {
                if ($ws.State -eq [System.Net.WebSockets.WebSocketState]::Open) {
                    $ws.CloseAsync([System.Net.WebSockets.WebSocketCloseStatus]::NormalClosure, "done", [System.Threading.CancellationToken]::None).GetAwaiter().GetResult()
                }
            } catch {}
            $ws.Dispose()
        }
    }
    if ($server -and !$server.HasExited) {
        Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
    }
}
