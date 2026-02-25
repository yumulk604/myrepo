param(
    [int]$Port = 18681,
    [string]$JwtSecret = "passkey_jwt_secret",
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
    }

    $server = Start-Process -FilePath $exePath -WorkingDirectory $projectRoot -PassThru -WindowStyle Hidden -Environment $childEnv
    if (!(Wait-Health -Port $Port -TimeoutSec $TimeoutSec)) {
        throw "Server hazir degil."
    }

    $username = "passkey-user"
    $tenantId = "tenant-passkey"
    $credentialId = "cred-passkey-001"
    $publicKey = "pk_test_material"
    $rpId = "localhost"
    $origin = "https://localhost"

    $registerChallengeBody = @{ username = $username; flow = "register"; role = "moderator"; tenantId = $tenantId; rpId = $rpId; origin = $origin } | ConvertTo-Json -Compress
    $registerChallenge = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body $registerChallengeBody

    $registerBody = @{
        username = $username
        challengeId = $registerChallenge.challengeId
        challenge = $registerChallenge.challenge
        credentialId = $credentialId
        publicKey = $publicKey
        signCount = 1
        rpId = $rpId
        origin = $origin
        clientDataType = "webauthn.create"
    } | ConvertTo-Json -Compress
    $registered = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/register") -Method Post -ContentType "application/json" -Body $registerBody
    if ($registered.status -ne "registered") {
        throw "Passkey register basarisiz."
    }

    $loginChallengeBody = @{ username = $username; flow = "login"; tenantId = $tenantId; rpId = $rpId; origin = $origin } | ConvertTo-Json -Compress
    $loginChallenge = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body $loginChallengeBody

    $loginBody = @{
        username = $username
        challengeId = $loginChallenge.challengeId
        challenge = $loginChallenge.challenge
        credentialId = $credentialId
        signCount = 2
        rpId = $rpId
        origin = $origin
        clientDataType = "webauthn.get"
    } | ConvertTo-Json -Compress
    $login = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/login") -Method Post -ContentType "application/json" -Body $loginBody

    if ([string]::IsNullOrWhiteSpace($login.accessToken) -or [string]::IsNullOrWhiteSpace($login.refreshToken)) {
        throw "Passkey login tokenlari olusmadi."
    }
    if ($login.tenantId -ne $tenantId) {
        throw "Tenant claim yanlis. Beklenen=$tenantId gelen=$($login.tenantId)"
    }
    if ($login.role -ne "moderator") {
        throw "Role claim yanlis. Beklenen=moderator gelen=$($login.role)"
    }

    $chatBody = @{ username = $username; content = "passkey-auth-check"; roomId = "pk-room"; tenantId = "wrong-tenant" } | ConvertTo-Json -Compress
    $created = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/chat/messages") -Method Post -ContentType "application/json" -Headers @{ Authorization = ("Bearer " + $login.accessToken) } -Body $chatBody
    if ($created.tenantId -ne $tenantId) {
        throw "JWT tenant enforcement calismadi."
    }

    $reusedBlocked = $false
    try {
        Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/login") -Method Post -ContentType "application/json" -Body $loginBody | Out-Null
    } catch {
        $reusedBlocked = $true
    }
    if (-not $reusedBlocked) {
        throw "Challenge reuse engellenmedi."
    }

    Write-Output "PASS: Passkey phase-1 challenge/register/login + JWT enforcement calisti."
}
finally {
    if ($server -and !$server.HasExited) {
        Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
    }
}
