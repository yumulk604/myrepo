param(
    [int]$Port = 18691,
    [string]$JwtSecret = "persist_jwt_secret",
    [int]$TimeoutSec = 20
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$exePath = Join-Path $projectRoot "media_server.exe"

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

function Start-TestServer {
    param([int]$Port, [string]$JwtSecret)
    $childEnv = @{
        GIGACHAD_SERVER_PORT = "$Port"
        MEDIA_SERVER_PORT = "$Port"
        GIGACHAD_JWT_SECRET = "$JwtSecret"
        MEDIA_JWT_SECRET = "$JwtSecret"
        GIGACHAD_EVENT_BUS = "memory"
        MEDIA_EVENT_BUS = "memory"
    }
    return Start-ProcessWithEnvironment -FilePath $exePath -WorkingDirectory $projectRoot -Environment $childEnv
}

$s1 = $null
$s2 = $null
try {
    $s1 = Start-TestServer -Port $Port -JwtSecret $JwtSecret
    if (!(Wait-Health -Port $Port -TimeoutSec $TimeoutSec)) { throw "Server-1 hazir degil." }

    $login = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/login") -Method Post -ContentType "application/json" -Body (@{username="persist-user";role="user";tenantId="persist-tenant"}|ConvertTo-Json -Compress)
    $access = $login.accessToken
    $refresh = $login.refreshToken
    if ([string]::IsNullOrWhiteSpace($access) -or [string]::IsNullOrWhiteSpace($refresh)) {
        throw "Tokenlar olusmadi."
    }

    Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/logout") -Method Post -Headers @{Authorization=("Bearer " + $access)} -ContentType "application/json" -Body (@{refreshToken=$refresh}|ConvertTo-Json -Compress) | Out-Null
    Stop-Process -Id $s1.Id -Force -ErrorAction SilentlyContinue
    $s1 = $null
    Start-Sleep -Seconds 1

    $s2 = Start-TestServer -Port $Port -JwtSecret $JwtSecret
    if (!(Wait-Health -Port $Port -TimeoutSec $TimeoutSec)) { throw "Server-2 hazir degil." }

    $blocked = $false
    try {
        Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/chat/messages") -Method Post -Headers @{Authorization=("Bearer " + $access)} -ContentType "application/json" -Body (@{username="persist-user";content="should-block";roomId="persist-room"}|ConvertTo-Json -Compress) | Out-Null
    } catch {
        $blocked = $true
    }
    if (-not $blocked) {
        throw "Restart sonrasi revoke persistence calismadi."
    }

    Write-Output "PASS: JWT revoke persistence (SQLite) calisti."
}
finally {
    if ($s1 -and !$s1.HasExited) { Stop-Process -Id $s1.Id -Force -ErrorAction SilentlyContinue }
    if ($s2 -and !$s2.HasExited) { Stop-Process -Id $s2.Id -Force -ErrorAction SilentlyContinue }
}
