param(
    [int]$Port = 18861,
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
        GIGACHAD_SFU_ENABLED = "1"
        MEDIA_SFU_ENABLED = "1"
        GIGACHAD_SFU_PROVIDER = "livekit"
        MEDIA_SFU_PROVIDER = "livekit"
        GIGACHAD_SFU_BASE_URL = "wss://sfu.local"
        MEDIA_SFU_BASE_URL = "wss://sfu.local"
        GIGACHAD_SFU_TOKEN_SECRET = "ci-sfu-secret"
        MEDIA_SFU_TOKEN_SECRET = "ci-sfu-secret"
    }

    $server = Start-ProcessWithEnvironment -FilePath $exePath -WorkingDirectory $projectRoot -Environment $childEnv
    if (!(Wait-Health -Port $Port -TimeoutSec $TimeoutSec)) { throw "Server hazir degil." }

    $room = "sfu-room"
    $tenant = "tenant-sfu"

    $ws1 = [System.Net.WebSockets.ClientWebSocket]::new()
    $ws1.ConnectAsync([System.Uri]("ws://127.0.0.1:$Port/ws?room=$room&user=u1&tenant=$tenant"), [System.Threading.CancellationToken]::None).GetAwaiter().GetResult() | Out-Null
    $null = Wait-Event -Socket $ws1 -Predicate { param($e) $e.type -eq "connected" } -TimeoutSec 8

    $ws2 = [System.Net.WebSockets.ClientWebSocket]::new()
    $ws2.ConnectAsync([System.Uri]("ws://127.0.0.1:$Port/ws?room=$room&user=u2&tenant=$tenant"), [System.Threading.CancellationToken]::None).GetAwaiter().GetResult() | Out-Null
    $null = Wait-Event -Socket $ws2 -Predicate { param($e) $e.type -eq "connected" } -TimeoutSec 8

    $ws3 = [System.Net.WebSockets.ClientWebSocket]::new()
    $ws3.ConnectAsync([System.Uri]("ws://127.0.0.1:$Port/ws?room=$room&user=u3&tenant=$tenant"), [System.Threading.CancellationToken]::None).GetAwaiter().GetResult() | Out-Null
    $c3 = Wait-Event -Socket $ws3 -Predicate { param($e) $e.type -eq "connected" } -TimeoutSec 8
    if (-not $c3) { throw "u3 connected event gelmedi." }
    if ($c3.mediaMode -ne "sfu") { throw "u3 mediaMode beklenen sfu, gelen=$($c3.mediaMode)" }

    Send-WebSocketText -Socket $ws3 -Text '{"type":"media.peer.sync"}'
    $sfuRequired = Wait-Event -Socket $ws3 -Predicate { param($e) $e.type -eq "media.sfu.required" -and $e.payload.provider -eq "livekit" -and $e.payload.enabled -eq $true -and $e.payload.baseUrl -eq "wss://sfu.local" } -TimeoutSec 8
    if (-not $sfuRequired) { throw "media.sfu.required eventi gelmedi." }
    if ([string]::IsNullOrWhiteSpace($sfuRequired.payload.joinToken)) { throw "media.sfu.required joinToken bos." }

    Send-WebSocketText -Socket $ws1 -Text '{"type":"webrtc.signal","targetUser":"u2","signalType":"offer","data":"blocked-in-sfu"}'
    $sfuError = Wait-Event -Socket $ws1 -Predicate { param($e) $e.type -eq "error" -and $e.code -eq "sfu_required" } -TimeoutSec 8
    if (-not $sfuError) { throw "webrtc.signal sfu_required error bekleniyordu." }
    $relayToU2 = Wait-Event -Socket $ws2 -Predicate { param($e) $e.type -eq "webrtc.signal" -and $e.payload.data -eq "blocked-in-sfu" } -TimeoutSec 3
    if ($relayToU2) { throw "sfu modunda webrtc.signal relay olmamaliydi." }

    Write-Output "PASS: sfu policy event + direct signaling block calisti."
}
finally {
    foreach ($ws in @($ws1, $ws2, $ws3)) {
        if ($ws) {
            try {
                if ($ws.State -eq [System.Net.WebSockets.WebSocketState]::Open) {
                    $ws.CloseAsync([System.Net.WebSockets.WebSocketCloseStatus]::NormalClosure, "done", [System.Threading.CancellationToken]::None).GetAwaiter().GetResult() | Out-Null
                }
            } catch {}
            $ws.Dispose()
        }
    }
    if ($server -and !$server.HasExited) {
        Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
    }
}
