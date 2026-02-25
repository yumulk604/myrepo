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
        GIGACHAD_PASSKEY_STRICT_METADATA = "1"
        MEDIA_PASSKEY_STRICT_METADATA = "1"
        GIGACHAD_PASSKEY_COUNTER_STRICT = "1"
        MEDIA_PASSKEY_COUNTER_STRICT = "1"
        GIGACHAD_STORAGE_MODE = "hybrid"
        MEDIA_STORAGE_MODE = "hybrid"
    }
    return Start-Process -FilePath $exePath -WorkingDirectory $projectRoot -PassThru -WindowStyle Hidden -Environment $childEnv
}

$s1 = $null
$s2 = $null
try {
    $runId = Get-Date -Format "yyyyMMddHHmmssfff"
    $username = "persist-passkey-user-$runId"
    $tenantId = "tenant-passkey-persist"
    $credentialId = "cred-persist-$runId"
    $publicKey = "persist-key"
    $rpId = "localhost"
    $origin = "https://localhost"

    $s1 = Start-TestServer -Port $Port -JwtSecret $JwtSecret
    if (!(Wait-Health -Port $Port -TimeoutSec $TimeoutSec)) { throw "Server-1 hazir degil." }

    $chReg = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body (@{username=$username;flow="register";role="user";tenantId=$tenantId;rpId=$rpId;origin=$origin}|ConvertTo-Json -Compress)
    $regClientDataJson = (@{ type = "webauthn.create"; challenge = $chReg.challenge; origin = $origin } | ConvertTo-Json -Compress)
    $regClientDataB64 = Convert-ToBase64Url -Bytes ([System.Text.Encoding]::UTF8.GetBytes($regClientDataJson))
    $regAuthData = Get-AuthenticatorDataB64 -RpId $rpId -SignCount 1
    Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/register") -Method Post -ContentType "application/json" -Body (@{username=$username;challengeId=$chReg.challengeId;challenge=$chReg.challenge;credentialId=$credentialId;publicKey=$publicKey;signCount=1;rpId=$rpId;origin=$origin;clientDataType="webauthn.create";clientDataJSON=$regClientDataB64;authenticatorData=$regAuthData}|ConvertTo-Json -Compress) | Out-Null

    Stop-Process -Id $s1.Id -Force -ErrorAction SilentlyContinue
    $s1 = $null
    Start-Sleep -Seconds 1

    $s2 = Start-TestServer -Port $Port -JwtSecret $JwtSecret
    if (!(Wait-Health -Port $Port -TimeoutSec $TimeoutSec)) { throw "Server-2 hazir degil." }

    $chLogin = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body (@{username=$username;flow="login";tenantId=$tenantId;rpId=$rpId;origin=$origin}|ConvertTo-Json -Compress)
    $loginClientDataJson = (@{ type = "webauthn.get"; challenge = $chLogin.challenge; origin = $origin } | ConvertTo-Json -Compress)
    $loginClientDataB64 = Convert-ToBase64Url -Bytes ([System.Text.Encoding]::UTF8.GetBytes($loginClientDataJson))
    $loginAuthData = Get-AuthenticatorDataB64 -RpId $rpId -SignCount 2
    $signatureB64 = Get-SignatureB64 -CredentialKey $publicKey -AuthenticatorDataB64 $loginAuthData -ClientDataJson $loginClientDataJson
    $login = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/login") -Method Post -ContentType "application/json" -Body (@{username=$username;challengeId=$chLogin.challengeId;challenge=$chLogin.challenge;credentialId=$credentialId;signCount=2;rpId=$rpId;origin=$origin;clientDataType="webauthn.get";clientDataJSON=$loginClientDataB64;authenticatorData=$loginAuthData;signature=$signatureB64}|ConvertTo-Json -Compress)

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
