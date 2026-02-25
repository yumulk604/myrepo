param(
    [int]$PortA = 18081,
    [int]$PortB = 18082,
    [string]$RedisHost = "127.0.0.1",
    [int]$RedisPort = 6379,
    [string]$RedisPassword = "",
    [string]$Channel = "",
    [int]$TimeoutSec = 25
)

$ErrorActionPreference = "Stop"
$LogPath = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "test_event_bus_multi_instance.log"
function LogLine([string]$m) {
    $line = ("[" + (Get-Date -Format "yyyy-MM-dd HH:mm:ss") + "] " + $m)
    Add-Content -Path $LogPath -Value $line
}
Set-Content -Path $LogPath -Value ("=== test start " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss") + " ===")

if ([string]::IsNullOrWhiteSpace($Channel)) {
    $Channel = "gagabunto.media.test." + [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
}

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$exePath = Join-Path $projectRoot "media_server.exe"
if (!(Test-Path $exePath)) {
    throw "media_server.exe bulunamadi. Once build_media.ps1 calistirin."
}

function Start-MediaServerInstance {
    param(
        [int]$Port,
        [string]$Name
    )

    $stdout = Join-Path $env:TEMP ("gagabunto_" + $Name + "_stdout.log")
    $stderr = Join-Path $env:TEMP ("gagabunto_" + $Name + "_stderr.log")

    $childEnv = @{
        GIGACHAD_SERVER_PORT = "$Port"
        MEDIA_SERVER_PORT = "$Port"
        GIGACHAD_EVENT_BUS = "redis"
        MEDIA_EVENT_BUS = "redis"
        GIGACHAD_EVENT_BUS_CHANNEL = "$Channel"
        MEDIA_EVENT_BUS_CHANNEL = "$Channel"
        GIGACHAD_REDIS_HOST = "$RedisHost"
        MEDIA_REDIS_HOST = "$RedisHost"
        GIGACHAD_REDIS_PORT = "$RedisPort"
        MEDIA_REDIS_PORT = "$RedisPort"
        GIGACHAD_API_TOKEN = ""
        MEDIA_API_TOKEN = ""
        GIGACHAD_WS_TOKEN = ""
        MEDIA_WS_TOKEN = ""
    }
    if (-not [string]::IsNullOrWhiteSpace($RedisPassword)) {
        $childEnv["GIGACHAD_REDIS_PASSWORD"] = "$RedisPassword"
        $childEnv["MEDIA_REDIS_PASSWORD"] = "$RedisPassword"
    } else {
        $childEnv["GIGACHAD_REDIS_PASSWORD"] = ""
        $childEnv["MEDIA_REDIS_PASSWORD"] = ""
    }

    Start-Process -FilePath $exePath `
        -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru `
        -Environment $childEnv `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
}

function Wait-ServerReady {
    param(
        [int]$Port,
        [int]$MaxSeconds
    )

    $url = "http://127.0.0.1:$Port/api/health"
    $deadline = (Get-Date).AddSeconds($MaxSeconds)
    $lastHealth = $null
    while ((Get-Date) -lt $deadline) {
        try {
            $health = Invoke-RestMethod -Uri $url -Method Get -TimeoutSec 2
            $lastHealth = $health
            if ($health.status -eq "healthy" -and $health.eventBusMode -eq "redis") {
                return $true
            }
        } catch {
        }
        Start-Sleep -Milliseconds 500
    }
    if ($lastHealth) {
        Write-Output ("[debug] health port " + $Port + ": " + ($lastHealth | ConvertTo-Json -Compress))
    } else {
        Write-Output ("[debug] health port " + $Port + ": no response")
    }
    return $false
}

function Receive-WebSocketText {
    param(
        [System.Net.WebSockets.ClientWebSocket]$Socket,
        [int]$ReceiveTimeoutSec
    )

    $buffer = New-Object byte[] 8192
    $segment = [System.ArraySegment[byte]]::new($buffer)
    $builder = [System.Text.StringBuilder]::new()

    while ($true) {
        $cts = [System.Threading.CancellationTokenSource]::new([TimeSpan]::FromSeconds($ReceiveTimeoutSec))
        try {
            $result = $Socket.ReceiveAsync($segment, $cts.Token).GetAwaiter().GetResult()
        } catch {
            return $null
        } finally {
            $cts.Dispose()
        }

        if ($result.MessageType -eq [System.Net.WebSockets.WebSocketMessageType]::Close) {
            return $null
        }

        if ($result.Count -gt 0) {
            [void]$builder.Append([System.Text.Encoding]::UTF8.GetString($buffer, 0, $result.Count))
        }

        if ($result.EndOfMessage) {
            return $builder.ToString()
        }
    }
}

$serverA = $null
$serverB = $null
$ws = $null
$testPassed = $false

try {
    Write-Output "[1/5] Starting media server instances..."
    LogLine "[1/5] Starting media server instances..."
    $serverA = Start-MediaServerInstance -Port $PortA -Name "A"
    $serverB = Start-MediaServerInstance -Port $PortB -Name "B"

    Write-Output "[2/5] Waiting for health..."
    LogLine "[2/5] Waiting for health..."
    if (!(Wait-ServerReady -Port $PortA -MaxSeconds $TimeoutSec)) {
        throw "Server A hazir degil veya event bus mode redis degil (Port: $PortA)."
    }
    if (!(Wait-ServerReady -Port $PortB -MaxSeconds $TimeoutSec)) {
        throw "Server B hazir degil veya event bus mode redis degil (Port: $PortB)."
    }

    Write-Output "[3/5] Connecting websocket client to instance B..."
    LogLine "[3/5] Connecting websocket client to instance B..."
    $roomId = "redis-room"
    $userId = "instance-b-listener"
    $uri = [System.Uri]("ws://127.0.0.1:$PortB/ws?room=$roomId&user=$userId")
    $ws = [System.Net.WebSockets.ClientWebSocket]::new()
    $connectCts = [System.Threading.CancellationTokenSource]::new([TimeSpan]::FromSeconds(10))
    [void]$ws.ConnectAsync($uri, $connectCts.Token).GetAwaiter().GetResult()
    $connectCts.Dispose()

    # Consume initial "connected" frame.
    [void](Receive-WebSocketText -Socket $ws -ReceiveTimeoutSec 5)

    Write-Output "[4/5] Posting chat message to instance A..."
    LogLine "[4/5] Posting chat message to instance A..."
    $marker = "multi-instance-" + [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
    $body = @{
        username = "redis-test"
        content = $marker
        roomId = $roomId
    } | ConvertTo-Json -Compress
    [void](Invoke-RestMethod -Uri "http://127.0.0.1:$PortA/api/chat/messages" `
        -Method Post -ContentType "application/json" -Body $body -TimeoutSec 5)

    Write-Output "[5/5] Waiting cross-instance event on websocket..."
    LogLine "[5/5] Waiting cross-instance event on websocket..."
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    $received = $false
    while ((Get-Date) -lt $deadline) {
        $frame = Receive-WebSocketText -Socket $ws -ReceiveTimeoutSec 5
        if ([string]::IsNullOrWhiteSpace($frame)) {
            continue
        }
        try {
            $event = $frame | ConvertFrom-Json
            if ($event.type -eq "chat.message.created" -and $event.payload.content -eq $marker -and $event.roomId -eq $roomId) {
                $received = $true
                break
            }
        } catch {
        }
    }

    if (-not $received) {
        throw "Cross-instance event alinamadi. Redis pub/sub propagation basarisiz."
    }

    Write-Output "PASS: Redis event bus multi-instance propagation calisti."
    LogLine "PASS"
    $testPassed = $true
}
catch {
    Write-Output ("FAIL: " + $_.Exception.Message)
    LogLine ("FAIL: " + $_.Exception.Message)
}
finally {
    if ($ws) {
        try {
            if ($ws.State -eq [System.Net.WebSockets.WebSocketState]::Open) {
                $closeCts = [System.Threading.CancellationTokenSource]::new([TimeSpan]::FromSeconds(2))
                $ws.CloseAsync([System.Net.WebSockets.WebSocketCloseStatus]::NormalClosure, "done", $closeCts.Token).GetAwaiter().GetResult()
                $closeCts.Dispose()
            }
        } catch {
        }
        $ws.Dispose()
    }

    if ($serverA -and !$serverA.HasExited) {
        LogLine ("Stopping serverA pid=" + $serverA.Id)
        Stop-Process -Id $serverA.Id -Force -ErrorAction SilentlyContinue
    }
    if ($serverB -and !$serverB.HasExited) {
        LogLine ("Stopping serverB pid=" + $serverB.Id)
        Stop-Process -Id $serverB.Id -Force -ErrorAction SilentlyContinue
    }
}

if (-not $testPassed) {
    LogLine "Final result: FAIL"
    throw "multi-instance redis test failed"
}
