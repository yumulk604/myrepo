param(
    [int]$PortA = 18181,
    [int]$PortB = 18182,
    [string]$RedisHost = "127.0.0.1",
    [int]$RedisPort = 6379,
    [string]$RedisPassword = "",
    [string]$Channel = "",
    [int]$TimeoutSec = 25
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($Channel)) {
    $Channel = "gagabunto.media.tenanttest." + [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
}

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$exePath = Join-Path $projectRoot "media_server.exe"
if (!(Test-Path $exePath)) {
    throw "media_server.exe bulunamadi. Once build_media.ps1 calistirin."
}

function Start-MediaServerInstance {
    param([int]$Port, [string]$Name)
    $stdout = Join-Path $env:TEMP ("gagabunto_tenant_" + $Name + "_stdout.log")
    $stderr = Join-Path $env:TEMP ("gagabunto_tenant_" + $Name + "_stderr.log")
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
        GIGACHAD_REDIS_PASSWORD = "$RedisPassword"
        MEDIA_REDIS_PASSWORD = "$RedisPassword"
    }
    Start-Process -FilePath $exePath `
        -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru `
        -Environment $childEnv `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
}

function Wait-ServerReady {
    param([int]$Port, [int]$MaxSeconds)
    $url = "http://127.0.0.1:$Port/api/health"
    $deadline = (Get-Date).AddSeconds($MaxSeconds)
    while ((Get-Date) -lt $deadline) {
        try {
            $health = Invoke-RestMethod -Uri $url -Method Get -TimeoutSec 2
            if ($health.status -eq "healthy" -and $health.eventBusMode -eq "redis" -and $health.eventBusRedisReady -eq $true) {
                return $true
            }
        } catch {}
        Start-Sleep -Milliseconds 400
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
    $serverA = Start-MediaServerInstance -Port $PortA -Name "A"
    $serverB = Start-MediaServerInstance -Port $PortB -Name "B"
    if (!(Wait-ServerReady -Port $PortA -MaxSeconds $TimeoutSec)) { throw "Server A hazir degil." }
    if (!(Wait-ServerReady -Port $PortB -MaxSeconds $TimeoutSec)) { throw "Server B hazir degil." }

    $roomId = "tenant-room"
    $tenantA = "tenant-alpha"
    $tenantB = "tenant-beta"

    $ws = [System.Net.WebSockets.ClientWebSocket]::new()
    $uri = [System.Uri]("ws://127.0.0.1:$PortB/ws?room=$roomId&user=tenant-listener&tenant=$tenantB")
    $cts = [System.Threading.CancellationTokenSource]::new([TimeSpan]::FromSeconds(10))
    [void]$ws.ConnectAsync($uri, $cts.Token).GetAwaiter().GetResult()
    $cts.Dispose()
    $initialFrame = Receive-WebSocketText -Socket $ws -ReceiveTimeoutSec 5
    [void]$initialFrame

    # Post different-tenant message first, then same-tenant message.
    # Validation rule:
    # - if markerA appears on websocket => isolation failure
    # - markerB must appear on websocket within timeout
    $markerA = "tenantA-" + [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
    $bodyA = @{ username="tenant-test"; content=$markerA; roomId=$roomId; tenantId=$tenantA } | ConvertTo-Json -Compress
    [void](Invoke-RestMethod -Uri "http://127.0.0.1:$PortA/api/chat/messages" -Method Post -ContentType "application/json" -Body $bodyA -TimeoutSec 5)
    $markerB = "tenantB-" + [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
    $bodyB = @{ username="tenant-test"; content=$markerB; roomId=$roomId; tenantId=$tenantB } | ConvertTo-Json -Compress
    [void](Invoke-RestMethod -Uri "http://127.0.0.1:$PortA/api/chat/messages" -Method Post -ContentType "application/json" -Body $bodyB -TimeoutSec 5)
    $tenantView = Invoke-RestMethod -Uri "http://127.0.0.1:$PortA/api/chat/messages?roomId=$roomId&tenantId=$tenantB" -Method Get -TimeoutSec 5
    $foundInApi = $false
    foreach ($m in $tenantView) {
        if ($m.content -eq $markerB) {
            $foundInApi = $true
            break
        }
    }
    if (-not $foundInApi) {
        throw "Tenant API FAIL: tenant-beta mesaji filtreli listede bulunamadi."
    }

    $received = $false
    $leakageDetected = $false
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        $frame = Receive-WebSocketText -Socket $ws -ReceiveTimeoutSec 5
        if ([string]::IsNullOrWhiteSpace($frame)) { continue }
        try {
            $ev = $frame | ConvertFrom-Json
            $eventContent = ($ev.payload | Select-Object -ExpandProperty content -ErrorAction SilentlyContinue)
            if ($ev.type -eq "chat.message.created" -and $eventContent -eq $markerA) {
                $leakageDetected = $true
                break
            }
            if ($ev.type -eq "chat.message.created" -and $ev.payload.content -eq $markerB -and $ev.tenantId -eq $tenantB) {
                $received = $true
                break
            }
        } catch {}
    }
    if ($leakageDetected) {
        throw "Tenant isolation FAIL: farkli tenant eventi alindi."
    }
    if (-not $received) {
        throw "Tenant delivery FAIL: ayni tenant eventi alinamadi."
    }

    Write-Output "PASS: Multi-tenant Redis event isolation + delivery calisti."
    $testPassed = $true
}
catch {
    Write-Output ("FAIL: " + $_.Exception.Message)
}
finally {
    if ($ws) {
        try {
            if ($ws.State -eq [System.Net.WebSockets.WebSocketState]::Open) {
                $closeCts = [System.Threading.CancellationTokenSource]::new([TimeSpan]::FromSeconds(2))
                $ws.CloseAsync([System.Net.WebSockets.WebSocketCloseStatus]::NormalClosure, "done", $closeCts.Token).GetAwaiter().GetResult()
                $closeCts.Dispose()
            }
        } catch {}
        $ws.Dispose()
    }
    if ($serverA -and !$serverA.HasExited) { Stop-Process -Id $serverA.Id -Force -ErrorAction SilentlyContinue }
    if ($serverB -and !$serverB.HasExited) { Stop-Process -Id $serverB.Id -Force -ErrorAction SilentlyContinue }
}

if (-not $testPassed) {
    throw "multi-tenant redis test failed"
}
