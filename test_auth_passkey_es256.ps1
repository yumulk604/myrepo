param(
    [int]$Port = 18684,
    [string]$JwtSecret = "passkey_es256_secret",
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

function Convert-ToBase64Url {
    param([byte[]]$Bytes)
    $b64 = [Convert]::ToBase64String($Bytes)
    return $b64.TrimEnd('=').Replace('+', '-').Replace('/', '_')
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

function Get-CoseEs256PublicKeyB64 {
    param([System.Security.Cryptography.ECDsa]$Ecdsa)
    $pub = $Ecdsa.ExportParameters($false)
    if ($pub.Q.X.Length -ne 32 -or $pub.Q.Y.Length -ne 32) {
        throw "Beklenen P-256 public key uzunlugu alinmadi."
    }
    $bytes = New-Object System.Collections.Generic.List[byte]
    [void]$bytes.Add(0xA5) # map(5)
    [void]$bytes.Add(0x01); [void]$bytes.Add(0x02) # 1:kty=2(EC2)
    [void]$bytes.Add(0x03); [void]$bytes.Add(0x26) # 3:alg=-7(ES256)
    [void]$bytes.Add(0x20); [void]$bytes.Add(0x01) # -1:crv=1(P-256)
    [void]$bytes.Add(0x21); [void]$bytes.Add(0x58); [void]$bytes.Add(0x20) # -2:x
    foreach ($b in $pub.Q.X) { [void]$bytes.Add([byte]$b) }
    [void]$bytes.Add(0x22); [void]$bytes.Add(0x58); [void]$bytes.Add(0x20) # -3:y
    foreach ($b in $pub.Q.Y) { [void]$bytes.Add([byte]$b) }
    return Convert-ToBase64Url -Bytes ($bytes.ToArray())
}

function Get-Es256SignatureB64 {
    param(
        [System.Security.Cryptography.ECDsa]$Ecdsa,
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
    $sig = $Ecdsa.SignData($msg, [System.Security.Cryptography.HashAlgorithmName]::SHA256)
    return Convert-ToBase64Url -Bytes $sig
}

$server = $null
$ecdsa = $null
$wrongEcdsa = $null
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
        GIGACHAD_PASSKEY_SIGNATURE_MODE = "es256"
        MEDIA_PASSKEY_SIGNATURE_MODE = "es256"
    }

    $server = Start-ProcessWithEnvironment -FilePath $exePath -WorkingDirectory $projectRoot -Environment $childEnv
    if (!(Wait-Health -Port $Port -TimeoutSec $TimeoutSec)) {
        throw "Server hazir degil."
    }

    $ecdsa = New-Object System.Security.Cryptography.ECDsaCng(256)
    $wrongEcdsa = New-Object System.Security.Cryptography.ECDsaCng(256)
    $publicKeyCose = Get-CoseEs256PublicKeyB64 -Ecdsa $ecdsa

    $runId = Get-Date -Format "yyyyMMddHHmmssfff"
    $username = "passkey-es256-user-$runId"
    $tenantId = "tenant-passkey-es256"
    $credentialId = "cred-passkey-es256-$runId"
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
        publicKey = $publicKeyCose
        signCount = 1
        rpId = $rpId
        origin = $origin
        clientDataType = "webauthn.create"
        clientDataJSON = $registerClientDataB64
        authenticatorData = $registerAuthData
    } | ConvertTo-Json -Compress
    $registered = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/register") -Method Post -ContentType "application/json" -Body $registerBody
    if ($registered.status -ne "registered") {
        throw "Passkey ES256 register basarisiz."
    }

    $loginChallengeBody = @{ username = $username; flow = "login"; tenantId = $tenantId; rpId = $rpId; origin = $origin } | ConvertTo-Json -Compress
    $loginChallenge = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body $loginChallengeBody
    $loginClientDataJson = (@{ type = "webauthn.get"; challenge = $loginChallenge.challenge; origin = $origin } | ConvertTo-Json -Compress)
    $loginClientDataB64 = Convert-ToBase64Url -Bytes ([System.Text.Encoding]::UTF8.GetBytes($loginClientDataJson))
    $loginAuthData = Get-AuthenticatorDataB64 -RpId $rpId -SignCount 2
    $loginSignatureB64 = Get-Es256SignatureB64 -Ecdsa $ecdsa -AuthenticatorDataB64 $loginAuthData -ClientDataJson $loginClientDataJson
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
        signature = $loginSignatureB64
    } | ConvertTo-Json -Compress
    $login = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/login") -Method Post -ContentType "application/json" -Body $loginBody
    if ([string]::IsNullOrWhiteSpace($login.accessToken) -or [string]::IsNullOrWhiteSpace($login.refreshToken)) {
        throw "Passkey ES256 login tokenlari olusmadi."
    }

    $badChallenge = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body $loginChallengeBody
    $badClientDataJson = (@{ type = "webauthn.get"; challenge = $badChallenge.challenge; origin = $origin } | ConvertTo-Json -Compress)
    $badClientDataB64 = Convert-ToBase64Url -Bytes ([System.Text.Encoding]::UTF8.GetBytes($badClientDataJson))
    $badAuthData = Get-AuthenticatorDataB64 -RpId $rpId -SignCount 3
    $badSignatureB64 = Get-Es256SignatureB64 -Ecdsa $wrongEcdsa -AuthenticatorDataB64 $badAuthData -ClientDataJson $badClientDataJson
    $badBody = @{
        username = $username
        challengeId = $badChallenge.challengeId
        challenge = $badChallenge.challenge
        credentialId = $credentialId
        signCount = 3
        rpId = $rpId
        origin = $origin
        clientDataType = "webauthn.get"
        clientDataJSON = $badClientDataB64
        authenticatorData = $badAuthData
        signature = $badSignatureB64
    } | ConvertTo-Json -Compress
    $badBlocked = $false
    try {
        Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/login") -Method Post -ContentType "application/json" -Body $badBody | Out-Null
    } catch {
        $badBlocked = $true
    }
    if (-not $badBlocked) {
        throw "ES256 invalid signature engellenmedi."
    }

    Write-Output "PASS: Passkey ES256 COSE register/login + invalid signature bloklama calisti."
}
finally {
    if ($ecdsa) { $ecdsa.Dispose() }
    if ($wrongEcdsa) { $wrongEcdsa.Dispose() }
    if ($server -and !$server.HasExited) {
        Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
    }
}

