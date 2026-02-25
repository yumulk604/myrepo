param(
    [int]$Port = 18731,
    [string]$JwtSecret = "passkey_lockout_secret",
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
    param([string]$RpId,[int]$SignCount)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { $rpHash = $sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($RpId)) } finally { $sha.Dispose() }
    $bytes = New-Object byte[] 37
    [Array]::Copy($rpHash, 0, $bytes, 0, 32)
    $bytes[32] = 0x01
    $bytes[33] = [byte](($SignCount -shr 24) -band 0xFF)
    $bytes[34] = [byte](($SignCount -shr 16) -band 0xFF)
    $bytes[35] = [byte](($SignCount -shr 8) -band 0xFF)
    $bytes[36] = [byte]($SignCount -band 0xFF)
    return Convert-ToBase64Url -Bytes $bytes
}

function Get-SignatureB64 {
    param([string]$CredentialKey,[string]$AuthenticatorDataB64,[string]$ClientDataJson)
    $authBytes = Convert-FromBase64Url -Value $AuthenticatorDataB64
    $clientBytes = [System.Text.Encoding]::UTF8.GetBytes($ClientDataJson)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { $clientHash = $sha.ComputeHash($clientBytes) } finally { $sha.Dispose() }
    $msg = New-Object byte[] ($authBytes.Length + $clientHash.Length)
    [Array]::Copy($authBytes, 0, $msg, 0, $authBytes.Length)
    [Array]::Copy($clientHash, 0, $msg, $authBytes.Length, $clientHash.Length)
    $hmac = New-Object System.Security.Cryptography.HMACSHA256 (,([System.Text.Encoding]::UTF8.GetBytes($CredentialKey)))
    try { $sig = $hmac.ComputeHash($msg) } finally { $hmac.Dispose() }
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
        GIGACHAD_PASSKEY_COUNTER_STRICT = "1"
        GIGACHAD_PASSKEY_LOCKOUT_SEC = "120"
        GIGACHAD_PASSKEY_RATE_MAX_ATTEMPTS = "30"
        GIGACHAD_PASSKEY_RATE_WINDOW_SEC = "60"
        GIGACHAD_STORAGE_MODE = "hybrid"
    }

    $server = Start-Process -FilePath $exePath -WorkingDirectory $projectRoot -PassThru -WindowStyle Hidden -Environment $childEnv
    if (!(Wait-Health -Port $Port -TimeoutSec $TimeoutSec)) { throw "Server hazir degil." }

    $runId = Get-Date -Format "yyyyMMddHHmmssfff"
    $username = "lockout-user-$runId"
    $tenantId = "tenant-lockout"
    $credentialId = "cred-lockout-$runId"
    $publicKey = "lockout-public"
    $rpId = "localhost"
    $origin = "https://localhost"

    $chReg = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body (@{username=$username;flow="register";tenantId=$tenantId;rpId=$rpId;origin=$origin}|ConvertTo-Json -Compress)
    $regClientJson = (@{type="webauthn.create";challenge=$chReg.challenge;origin=$origin}|ConvertTo-Json -Compress)
    $regClientB64 = Convert-ToBase64Url -Bytes ([System.Text.Encoding]::UTF8.GetBytes($regClientJson))
    $regAuth = Get-AuthenticatorDataB64 -RpId $rpId -SignCount 1
    Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/register") -Method Post -ContentType "application/json" -Body (@{username=$username;challengeId=$chReg.challengeId;challenge=$chReg.challenge;credentialId=$credentialId;publicKey=$publicKey;signCount=1;rpId=$rpId;origin=$origin;clientDataType="webauthn.create";clientDataJSON=$regClientB64;authenticatorData=$regAuth}|ConvertTo-Json -Compress) | Out-Null

    $lockTriggered = $false
    for ($i = 2; $i -le 14; $i++) {
        $chBad = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body (@{username=$username;flow="login";tenantId=$tenantId;rpId=$rpId;origin=$origin}|ConvertTo-Json -Compress)
        $badClientJson = (@{type="webauthn.get";challenge=$chBad.challenge;origin=$origin}|ConvertTo-Json -Compress)
        $badClientB64 = Convert-ToBase64Url -Bytes ([System.Text.Encoding]::UTF8.GetBytes($badClientJson))
        $badAuth = Get-AuthenticatorDataB64 -RpId $rpId -SignCount $i
        $badSig = Get-SignatureB64 -CredentialKey "wrong-secret" -AuthenticatorDataB64 $badAuth -ClientDataJson $badClientJson
        $badBody = @{username=$username;challengeId=$chBad.challengeId;challenge=$chBad.challenge;credentialId=$credentialId;signCount=$i;rpId=$rpId;origin=$origin;clientDataType="webauthn.get";clientDataJSON=$badClientB64;authenticatorData=$badAuth;signature=$badSig} | ConvertTo-Json -Compress
        try {
            Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/login") -Method Post -ContentType "application/json" -Body $badBody | Out-Null
            throw "Beklenen login fail gelmedi."
        } catch {
            if ($_.Exception.Message -match "429") {
                $lockTriggered = $true
                break
            }
        }
    }

    if (-not $lockTriggered) {
        throw "Lockout tetiklenmedi (bad signature denemelerinde 429 yok)."
    }

    $chGood = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body (@{username=$username;flow="login";tenantId=$tenantId;rpId=$rpId;origin=$origin}|ConvertTo-Json -Compress)
    $goodClientJson = (@{type="webauthn.get";challenge=$chGood.challenge;origin=$origin}|ConvertTo-Json -Compress)
    $goodClientB64 = Convert-ToBase64Url -Bytes ([System.Text.Encoding]::UTF8.GetBytes($goodClientJson))
    $goodAuth = Get-AuthenticatorDataB64 -RpId $rpId -SignCount 20
    $goodSig = Get-SignatureB64 -CredentialKey $publicKey -AuthenticatorDataB64 $goodAuth -ClientDataJson $goodClientJson
    $goodBody = @{username=$username;challengeId=$chGood.challengeId;challenge=$chGood.challenge;credentialId=$credentialId;signCount=4;rpId=$rpId;origin=$origin;clientDataType="webauthn.get";clientDataJSON=$goodClientB64;authenticatorData=$goodAuth;signature=$goodSig} | ConvertTo-Json -Compress

    $locked = $false
    try { Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/login") -Method Post -ContentType "application/json" -Body $goodBody | Out-Null } catch {
        if ($_.Exception.Message -match "429") { $locked = $true }
    }

    if (-not $locked) {
        throw "Lockout devreye girmedi."
    }

    Write-Output "PASS: Passkey lockout calisti (429)."
}
finally {
    if ($server -and !$server.HasExited) {
        Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
    }
}
