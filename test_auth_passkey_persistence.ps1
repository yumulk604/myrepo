param(
    [int]$Port = 18901,
    [string]$JwtSecret = "passkey_persist_secret",
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

function Start-TestServer {
    param([int]$Port, [string]$JwtSecret)
    $childEnv = @{
        GIGACHAD_SERVER_PORT = "$Port"
        MEDIA_SERVER_PORT = "$Port"
        GIGACHAD_JWT_SECRET = "$JwtSecret"
        MEDIA_JWT_SECRET = "$JwtSecret"
        GIGACHAD_EVENT_BUS = "memory"
        MEDIA_EVENT_BUS = "memory"
        GIGACHAD_API_TOKEN = ""
        MEDIA_API_TOKEN = ""
    }
    return Start-Process -FilePath $exePath -WorkingDirectory $projectRoot -PassThru -WindowStyle Hidden -Environment $childEnv
}

$s1 = $null
$s2 = $null
try {
    $username = "persist-passkey-user"
    $tenantId = "tenant-passkey-persist"
    $credentialId = "cred-persist-001"
    $rpId = "localhost"
    $origin = "https://localhost"

    $s1 = Start-TestServer -Port $Port -JwtSecret $JwtSecret
    if (!(Wait-Health -Port $Port -TimeoutSec $TimeoutSec)) { throw "Server-1 hazir degil." }

    $chReg = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body (@{username=$username;flow="register";role="user";tenantId=$tenantId;rpId=$rpId;origin=$origin}|ConvertTo-Json -Compress)
    Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/register") -Method Post -ContentType "application/json" -Body (@{username=$username;challengeId=$chReg.challengeId;challenge=$chReg.challenge;credentialId=$credentialId;publicKey="persist-key";signCount=1;rpId=$rpId;origin=$origin;clientDataType="webauthn.create"}|ConvertTo-Json -Compress) | Out-Null

    Stop-Process -Id $s1.Id -Force -ErrorAction SilentlyContinue
    $s1 = $null
    Start-Sleep -Seconds 1

    $s2 = Start-TestServer -Port $Port -JwtSecret $JwtSecret
    if (!(Wait-Health -Port $Port -TimeoutSec $TimeoutSec)) { throw "Server-2 hazir degil." }

    $chLogin = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body (@{username=$username;flow="login";tenantId=$tenantId;rpId=$rpId;origin=$origin}|ConvertTo-Json -Compress)
    $login = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/login") -Method Post -ContentType "application/json" -Body (@{username=$username;challengeId=$chLogin.challengeId;challenge=$chLogin.challenge;credentialId=$credentialId;signCount=2;rpId=$rpId;origin=$origin;clientDataType="webauthn.get"}|ConvertTo-Json -Compress)

    if ([string]::IsNullOrWhiteSpace($login.accessToken)) {
        throw "Restart sonrasi passkey login basarisiz."
    }
    if ($login.tenantId -ne $tenantId) {
        throw "Restart sonrasi tenant claim yanlis."
    }

    Write-Output "PASS: Passkey credential persistence (SQLite) calisti."
}
finally {
    if ($s1 -and !$s1.HasExited) { Stop-Process -Id $s1.Id -Force -ErrorAction SilentlyContinue }
    if ($s2 -and !$s2.HasExited) { Stop-Process -Id $s2.Id -Force -ErrorAction SilentlyContinue }
}
