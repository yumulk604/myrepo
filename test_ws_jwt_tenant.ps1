param(
    [int]$Port = 18581,
    [string]$JwtSecret = "ws_jwt_secret",
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

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $FilePath
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true

    foreach ($kv in $Environment.GetEnumerator()) {
        $key = [string]$kv.Key
        $value = [string]$kv.Value
        $startInfo.EnvironmentVariables[$key] = $value
    }

    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $startInfo
    [void]$proc.Start()
    return $proc
}

function Receive-WebSocketText {
    param([System.Net.WebSockets.ClientWebSocket]$Socket, [int]$ReceiveTimeoutSec = 5)
    $buffer = New-Object byte[] 8192
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

$server = $null
$ws = $null

try {
    $childEnv = @{
        GIGACHAD_SERVER_PORT = "$Port"
        MEDIA_SERVER_PORT = "$Port"
        GIGACHAD_JWT_SECRET = "$JwtSecret"
        MEDIA_JWT_SECRET = "$JwtSecret"
        GIGACHAD_EVENT_BUS = "memory"
        MEDIA_EVENT_BUS = "memory"
    }
    $server = Start-ProcessWithEnvironment -FilePath $exePath -WorkingDirectory $projectRoot -Environment $childEnv
    if (!(Wait-Health -Port $Port -TimeoutSec $TimeoutSec)) {
        throw "Server hazir degil."
    }

    $loginBody = @{ username = "ws-user"; role = "user"; tenantId = "tenant-alpha" } | ConvertTo-Json -Compress
    $login = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/login") -Method Post -ContentType "application/json" -Body $loginBody
    $token = $login.accessToken
    if ([string]::IsNullOrWhiteSpace($token)) {
        throw "Access token alinamadi."
    }

    # Negative-1: no token should fail websocket handshake.
    $unauthorizedFailed = $false
    try {
        $tmpWs = [System.Net.WebSockets.ClientWebSocket]::new()
        $uriNoToken = [System.Uri]("ws://127.0.0.1:$Port/ws?room=ws-room&tenant=tenant-alpha")
        $ctsNoToken = [System.Threading.CancellationTokenSource]::new([TimeSpan]::FromSeconds(5))
        [void]$tmpWs.ConnectAsync($uriNoToken, $ctsNoToken.Token).GetAwaiter().GetResult()
        $ctsNoToken.Dispose()
    } catch {
        $unauthorizedFailed = $true
    } finally {
        if ($tmpWs) { $tmpWs.Dispose() }
    }
    if (-not $unauthorizedFailed) {
        throw "Token olmadan WS baglantisi engellenmedi."
    }

    # Negative-2: tenant query override should not bypass JWT tenant.
    $ws = [System.Net.WebSockets.ClientWebSocket]::new()
    $uri = [System.Uri]("ws://127.0.0.1:$Port/ws?room=ws-room&tenant=tenant-beta&token=$token")
    $cts = [System.Threading.CancellationTokenSource]::new([TimeSpan]::FromSeconds(8))
    [void]$ws.ConnectAsync($uri, $cts.Token).GetAwaiter().GetResult()
    $cts.Dispose()

    $initialRaw = Receive-WebSocketText -Socket $ws -ReceiveTimeoutSec 5
    if ([string]::IsNullOrWhiteSpace($initialRaw)) {
        throw "WS initial event alinamadi."
    }
    $initial = $initialRaw | ConvertFrom-Json
    if ($initial.tenantId -ne "tenant-alpha") {
        throw "WS tenant override acigi var. Beklenen tenant-alpha, gelen $($initial.tenantId)"
    }

    # API post with conflicting tenant in body must be forced to token tenant.
    $body = @{ username = "ws-user"; content = "ws-jwt-tenant-check"; roomId = "ws-room"; tenantId = "tenant-beta" } | ConvertTo-Json -Compress
    $created = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/chat/messages") -Method Post -Headers @{ Authorization = ("Bearer " + $token) } -ContentType "application/json" -Body $body
    if ($created.tenantId -ne "tenant-alpha") {
        throw "HTTP tenant override acigi var. Beklenen tenant-alpha, gelen $($created.tenantId)"
    }

    Write-Output "PASS: WS JWT negative tests calisti (no-token blocked + tenant override blocked)."
}
finally {
    if ($ws) {
        try {
            if ($ws.State -eq [System.Net.WebSockets.WebSocketState]::Open) {
                $cc = [System.Threading.CancellationTokenSource]::new([TimeSpan]::FromSeconds(2))
                $ws.CloseAsync([System.Net.WebSockets.WebSocketCloseStatus]::NormalClosure, "done", $cc.Token).GetAwaiter().GetResult()
                $cc.Dispose()
            }
        } catch {}
        $ws.Dispose()
    }
    if ($server -and !$server.HasExited) {
        Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
    }
}
