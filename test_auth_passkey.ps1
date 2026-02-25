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

function Convert-ToBase64Url {
    param([byte[]]$Bytes)
    $b64 = [Convert]::ToBase64String($Bytes)
    return $b64.TrimEnd('=').Replace('+', '-').Replace('/', '_')
}

function Get-AuthenticatorDataB64 {
    param(
        [string]$RpId,
        [int]$SignCount
    )
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $rpHash = $sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($RpId))
    } finally {
        $sha.Dispose()
    }
    $bytes = New-Object byte[] 37
    [Array]::Copy($rpHash, 0, $bytes, 0, 32)
    $bytes[32] = 0x01
    $bytes[33] = [byte](($SignCount -shr 24) -band 0xFF)
    $bytes[34] = [byte](($SignCount -shr 16) -band 0xFF)
    $bytes[35] = [byte](($SignCount -shr 8) -band 0xFF)
    $bytes[36] = [byte]($SignCount -band 0xFF)
    return Convert-ToBase64Url -Bytes $bytes
}

function Convert-FromBase64Url {
    param([string]$Value)
    $padded = $Value.Replace('-', '+').Replace('_', '/')
    switch ($padded.Length % 4) {
        2 { $padded += "==" }
        3 { $padded += "=" }
    }
    return [Convert]::FromBase64String($padded)
}

function Get-SignatureB64 {
    param(
        [string]$CredentialKey,
        [string]$AuthenticatorDataB64,
        [string]$ClientDataJson
    )
    $authBytes = Convert-FromBase64Url -Value $AuthenticatorDataB64
    $clientBytes = [System.Text.Encoding]::UTF8.GetBytes($ClientDataJson)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $clientHash = $sha.ComputeHash($clientBytes)
    } finally {
        $sha.Dispose()
    }

    $msg = New-Object byte[] ($authBytes.Length + $clientHash.Length)
    [Array]::Copy($authBytes, 0, $msg, 0, $authBytes.Length)
    [Array]::Copy($clientHash, 0, $msg, $authBytes.Length, $clientHash.Length)

    $hmac = New-Object System.Security.Cryptography.HMACSHA256 (,([System.Text.Encoding]::UTF8.GetBytes($CredentialKey)))
    try {
        $sig = $hmac.ComputeHash($msg)
    } finally {
        $hmac.Dispose()
    }
    return Convert-ToBase64Url -Bytes $sig
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
        GIGACHAD_PASSKEY_STRICT_METADATA = "1"
        MEDIA_PASSKEY_STRICT_METADATA = "1"
        GIGACHAD_PASSKEY_COUNTER_STRICT = "1"
        MEDIA_PASSKEY_COUNTER_STRICT = "1"
        GIGACHAD_STORAGE_MODE = "hybrid"
        MEDIA_STORAGE_MODE = "hybrid"
    }

    $server = Start-ProcessWithEnvironment -FilePath $exePath -WorkingDirectory $projectRoot -Environment $childEnv
    if (!(Wait-Health -Port $Port -TimeoutSec $TimeoutSec)) {
        throw "Server hazir degil."
    }

    $runId = Get-Date -Format "yyyyMMddHHmmssfff"
    $username = "passkey-user-$runId"
    $tenantId = "tenant-passkey"
    $credentialId = "cred-passkey-$runId"
    $publicKey = "pk_test_material"
    $rpId = "localhost"
    $origin = "https://localhost"

    $registerChallengeBody = @{ username = $username; flow = "register"; role = "moderator"; tenantId = $tenantId; rpId = $rpId; origin = $origin } | ConvertTo-Json -Compress
    $registerChallenge = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body $registerChallengeBody
    $registerClientDataJson = (@{ type = "webauthn.create"; challenge = $registerChallenge.challenge; origin = $origin } | ConvertTo-Json -Compress)
    $registerClientDataB64 = Convert-ToBase64Url -Bytes ([System.Text.Encoding]::UTF8.GetBytes($registerClientDataJson))
    $registerAuthData = Get-AuthenticatorDataB64 -RpId $rpId -SignCount 1

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
        clientDataJSON = $registerClientDataB64
        authenticatorData = $registerAuthData
    } | ConvertTo-Json -Compress
    $registered = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/register") -Method Post -ContentType "application/json" -Body $registerBody
    if ($registered.status -ne "registered") {
        throw "Passkey register basarisiz."
    }

    $loginChallengeBody = @{ username = $username; flow = "login"; tenantId = $tenantId; rpId = $rpId; origin = $origin } | ConvertTo-Json -Compress
    $loginChallenge = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body $loginChallengeBody
    $loginClientDataJson = (@{ type = "webauthn.get"; challenge = $loginChallenge.challenge; origin = $origin } | ConvertTo-Json -Compress)
    $loginClientDataB64 = Convert-ToBase64Url -Bytes ([System.Text.Encoding]::UTF8.GetBytes($loginClientDataJson))
    $loginAuthData = Get-AuthenticatorDataB64 -RpId $rpId -SignCount 2
    $signatureB64 = Get-SignatureB64 -CredentialKey $publicKey -AuthenticatorDataB64 $loginAuthData -ClientDataJson $loginClientDataJson

    $loginBody = @{
        username = $username
        challengeId = $loginChallenge.challengeId
        challenge = $loginChallenge.challenge
        credentialId = $credentialId
        signCount = 2
        rpId = $rpId
        origin = $origin
        clientDataType = "webauthn.get"
        clientDataJSON = $loginClientDataB64
        authenticatorData = $loginAuthData
        signature = $signatureB64
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

    $otherTenant = "tenant-other"
    $crossChallengeBody = @{ username = $username; flow = "login"; tenantId = $otherTenant; rpId = $rpId; origin = $origin } | ConvertTo-Json -Compress
    $crossChallenge = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body $crossChallengeBody
    $crossClientDataJson = (@{ type = "webauthn.get"; challenge = $crossChallenge.challenge; origin = $origin } | ConvertTo-Json -Compress)
    $crossClientDataB64 = Convert-ToBase64Url -Bytes ([System.Text.Encoding]::UTF8.GetBytes($crossClientDataJson))
    $crossAuthData = Get-AuthenticatorDataB64 -RpId $rpId -SignCount 3
    $crossSig = Get-SignatureB64 -CredentialKey $publicKey -AuthenticatorDataB64 $crossAuthData -ClientDataJson $crossClientDataJson
    $crossBody = @{
        username = $username
        challengeId = $crossChallenge.challengeId
        challenge = $crossChallenge.challenge
        credentialId = $credentialId
        signCount = 3
        rpId = $rpId
        origin = $origin
        clientDataType = "webauthn.get"
        clientDataJSON = $crossClientDataB64
        authenticatorData = $crossAuthData
        signature = $crossSig
    } | ConvertTo-Json -Compress
    $crossBlocked = $false
    try {
        Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/login") -Method Post -ContentType "application/json" -Body $crossBody | Out-Null
    } catch {
        $crossBlocked = $true
    }
    if (-not $crossBlocked) {
        throw "Tenant izolasyonu bozuk: baska tenantta ayni credential ile login basarili oldu."
    }

    $badSigChallenge = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body $loginChallengeBody
    $badSigClientDataJson = (@{ type = "webauthn.get"; challenge = $badSigChallenge.challenge; origin = $origin } | ConvertTo-Json -Compress)
    $badSigClientDataB64 = Convert-ToBase64Url -Bytes ([System.Text.Encoding]::UTF8.GetBytes($badSigClientDataJson))
    $badSigAuthData = Get-AuthenticatorDataB64 -RpId $rpId -SignCount 3
    $badSig = Get-SignatureB64 -CredentialKey "wrong-secret" -AuthenticatorDataB64 $badSigAuthData -ClientDataJson $badSigClientDataJson
    $badSigBody = @{
        username = $username
        challengeId = $badSigChallenge.challengeId
        challenge = $badSigChallenge.challenge
        credentialId = $credentialId
        signCount = 3
        rpId = $rpId
        origin = $origin
        clientDataType = "webauthn.get"
        clientDataJSON = $badSigClientDataB64
        authenticatorData = $badSigAuthData
        signature = $badSig
    } | ConvertTo-Json -Compress

    $badSigBlocked = $false
    try {
        Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/login") -Method Post -ContentType "application/json" -Body $badSigBody | Out-Null
    } catch {
        $badSigBlocked = $true
    }
    if (-not $badSigBlocked) {
        throw "Invalid signature engellenmedi."
    }

    $rollbackChallenge = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body $loginChallengeBody
    $rollbackClientDataJson = (@{ type = "webauthn.get"; challenge = $rollbackChallenge.challenge; origin = $origin } | ConvertTo-Json -Compress)
    $rollbackClientDataB64 = Convert-ToBase64Url -Bytes ([System.Text.Encoding]::UTF8.GetBytes($rollbackClientDataJson))
    $rollbackAuthData = Get-AuthenticatorDataB64 -RpId $rpId -SignCount 2
    $rollbackSig = Get-SignatureB64 -CredentialKey $publicKey -AuthenticatorDataB64 $rollbackAuthData -ClientDataJson $rollbackClientDataJson
    $rollbackBody = @{
        username = $username
        challengeId = $rollbackChallenge.challengeId
        challenge = $rollbackChallenge.challenge
        credentialId = $credentialId
        signCount = 2
        rpId = $rpId
        origin = $origin
        clientDataType = "webauthn.get"
        clientDataJSON = $rollbackClientDataB64
        authenticatorData = $rollbackAuthData
        signature = $rollbackSig
    } | ConvertTo-Json -Compress

    $rollbackBlocked = $false
    try {
        Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/login") -Method Post -ContentType "application/json" -Body $rollbackBody | Out-Null
    } catch {
        $rollbackBlocked = $true
    }
    if (-not $rollbackBlocked) {
        throw "signCount rollback engellenmedi."
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
