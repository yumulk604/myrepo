param(
    [int]$Port = 18721,
    [string]$JwtSecret = "passkey_rate_secret",
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

$server = $null
try {
    $childEnv = @{
        GIGACHAD_SERVER_PORT = "$Port"
        MEDIA_SERVER_PORT = "$Port"
        GIGACHAD_JWT_SECRET = "$JwtSecret"
        MEDIA_JWT_SECRET = "$JwtSecret"
        GIGACHAD_EVENT_BUS = "memory"
        MEDIA_EVENT_BUS = "memory"
        GIGACHAD_API_TOKEN = ""
        MEDIA_API_TOKEN = ""
        GIGACHAD_PASSKEY_RATE_MAX_ATTEMPTS = "2"
        MEDIA_PASSKEY_RATE_MAX_ATTEMPTS = "2"
        GIGACHAD_PASSKEY_RATE_WINDOW_SEC = "60"
        MEDIA_PASSKEY_RATE_WINDOW_SEC = "60"
    }

    $server = Start-Process -FilePath $exePath -WorkingDirectory $projectRoot -PassThru -WindowStyle Hidden -Environment $childEnv
    if (!(Wait-Health -Port $Port -TimeoutSec $TimeoutSec)) {
        throw "Server hazir degil."
    }

    $u = "rl-user-$(Get-Date -Format 'yyyyMMddHHmmssfff')"
    $body = @{ username = $u; flow = "login"; tenantId = "tenant-rl"; rpId = "localhost"; origin = "https://localhost" } | ConvertTo-Json -Compress

    $blocked = $false
    for ($i = 1; $i -le 20; $i++) {
        try {
            Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body $body | Out-Null
        } catch {
            if ($_.Exception.Message -match "429") {
                $blocked = $true
                break
            }
        }
    }

    if (-not $blocked) {
        throw "Passkey rate-limit calismadi (20 istek icinde 429 yok)."
    }

    Write-Output "PASS: Passkey rate-limit calisti (429)."
}
finally {
    if ($server -and !$server.HasExited) {
        Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
    }
}
