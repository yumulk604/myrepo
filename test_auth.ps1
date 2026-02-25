param(
    [int]$Port = 18481,
    [string]$JwtSecret = "local_dev_jwt_secret",
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

$server = $null
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

    $loginBody = @{ username = "auth-tester"; role = "moderator"; tenantId = "tenant-auth" } | ConvertTo-Json -Compress
    $login = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/login") -Method Post -ContentType "application/json" -Body $loginBody
    if ([string]::IsNullOrWhiteSpace($login.accessToken) -or [string]::IsNullOrWhiteSpace($login.refreshToken)) {
        throw "Login tokenlari olusmadi."
    }

    $chatBody = @{ username = "auth-tester"; content = "jwt-auth-check"; roomId = "auth-room"; tenantId = "ignored-tenant" } | ConvertTo-Json -Compress
    $created = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/chat/messages") -Method Post -ContentType "application/json" -Headers @{ Authorization = ("Bearer " + $login.accessToken) } -Body $chatBody
    if ($created.tenantId -ne "tenant-auth") {
        throw "Tenant override calismadi. Beklenen=tenant-auth gelen=$($created.tenantId)"
    }

    $refreshBody = @{ refreshToken = $login.refreshToken } | ConvertTo-Json -Compress
    $refresh = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/refresh") -Method Post -ContentType "application/json" -Body $refreshBody
    if ([string]::IsNullOrWhiteSpace($refresh.accessToken) -or [string]::IsNullOrWhiteSpace($refresh.refreshToken)) {
        throw "Refresh tokenlari olusmadi."
    }

    $logoutBody = @{ refreshToken = $refresh.refreshToken } | ConvertTo-Json -Compress
    Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/logout") -Method Post -ContentType "application/json" -Headers @{ Authorization = ("Bearer " + $login.accessToken) } -Body $logoutBody | Out-Null

    $blocked = $false
    try {
        Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/chat/messages") -Method Post -ContentType "application/json" -Headers @{ Authorization = ("Bearer " + $login.accessToken) } -Body $chatBody | Out-Null
    } catch {
        $blocked = $true
    }
    if (-not $blocked) {
        throw "Logout sonrasi eski access token bloke edilmedi."
    }

    Write-Output "PASS: JWT auth login/refresh/logout + tenant claim enforcement calisti."
}
finally {
    if ($server -and !$server.HasExited) {
        Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
    }
}
