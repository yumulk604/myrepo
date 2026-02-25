param(
    [int]$Port = 18688,
    [string]$JwtSecret = "passkey_eddsa_secret",
    [int]$TimeoutSec = 20
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$exePath = Join-Path $projectRoot "media_server.exe"
$libsodiumDll = Join-Path $projectRoot "libsodium.dll"

if (!(Test-Path $exePath)) {
    throw "media_server.exe bulunamadi. Once build_media.ps1 calistirin."
}
if (!(Test-Path $libsodiumDll)) {
    throw "libsodium.dll bulunamadi. Once setup_libsodium_windows.ps1 calistirin."
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
    param([string]$RpId, [int]$SignCount)
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

function Get-CoseEd25519PublicKeyB64 {
    param([byte[]]$PublicKey32)
    if ($PublicKey32.Length -ne 32) {
        throw "Ed25519 public key 32 byte olmali."
    }
    $bytes = New-Object System.Collections.Generic.List[byte]
    [void]$bytes.Add(0xA4) # map(4)
    [void]$bytes.Add(0x01); [void]$bytes.Add(0x01) # 1:kty=1(OKP)
    [void]$bytes.Add(0x03); [void]$bytes.Add(0x27) # 3:alg=-8(EdDSA)
    [void]$bytes.Add(0x20); [void]$bytes.Add(0x06) # -1:crv=6(Ed25519)
    [void]$bytes.Add(0x21); [void]$bytes.Add(0x58); [void]$bytes.Add(0x20) # -2:x
    foreach ($b in $PublicKey32) { [void]$bytes.Add([byte]$b) }
    return Convert-ToBase64Url -Bytes ($bytes.ToArray())
}

$csharp = @"
using System;
using System.Runtime.InteropServices;
public static class SodiumNative {
    [DllImport("libsodium", CallingConvention = CallingConvention.Cdecl)]
    public static extern int sodium_init();
    [DllImport("libsodium", CallingConvention = CallingConvention.Cdecl)]
    public static extern int crypto_sign_keypair(byte[] pk, byte[] sk);
    [DllImport("libsodium", CallingConvention = CallingConvention.Cdecl)]
    public static extern int crypto_sign_detached(byte[] sig, out ulong siglen, byte[] m, ulong mlen, byte[] sk);
}
"@
Add-Type -TypeDefinition $csharp

if ([SodiumNative]::sodium_init() -lt 0) {
    throw "libsodium init basarisiz."
}

function New-Ed25519KeyPair {
    $pk = New-Object byte[] 32
    $sk = New-Object byte[] 64
    $rc = [SodiumNative]::crypto_sign_keypair($pk, $sk)
    if ($rc -ne 0) { throw "crypto_sign_keypair failed" }
    return @{ PublicKey = $pk; SecretKey = $sk }
}

function Get-Ed25519SignatureB64 {
    param([byte[]]$SecretKey64, [string]$AuthenticatorDataB64, [string]$ClientDataJson)
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

    $sig = New-Object byte[] 64
    [UInt64]$sigLen = 0
    $rc = [SodiumNative]::crypto_sign_detached($sig, [ref]$sigLen, $msg, [UInt64]$msg.Length, $SecretKey64)
    if ($rc -ne 0 -or $sigLen -ne 64) {
        throw "crypto_sign_detached failed"
    }
    return Convert-ToBase64Url -Bytes $sig
}

$server = $null
try {
    $kp = New-Ed25519KeyPair
    $wrong = New-Ed25519KeyPair
    $publicKeyCose = Get-CoseEd25519PublicKeyB64 -PublicKey32 $kp.PublicKey

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

    $runId = Get-Date -Format "yyyyMMddHHmmssfff"
    $username = "passkey-eddsa-user-$runId"
    $tenantId = "tenant-passkey-eddsa"
    $credentialId = "cred-passkey-eddsa-$runId"
    $rpId = "localhost"
    $origin = "https://localhost"

    $chReg = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body (@{username=$username;flow="register";role="moderator";tenantId=$tenantId;rpId=$rpId;origin=$origin}|ConvertTo-Json -Compress)
    $regClientJson = (@{ type="webauthn.create"; challenge=$chReg.challenge; origin=$origin } | ConvertTo-Json -Compress)
    $regClientB64 = Convert-ToBase64Url -Bytes ([System.Text.Encoding]::UTF8.GetBytes($regClientJson))
    $regAuth = Get-AuthenticatorDataB64 -RpId $rpId -SignCount 1
    $registered = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/register") -Method Post -ContentType "application/json" -Body (@{
        username=$username;challengeId=$chReg.challengeId;challenge=$chReg.challenge;credentialId=$credentialId;publicKey=$publicKeyCose;signCount=1;
        rpId=$rpId;origin=$origin;clientDataType="webauthn.create";clientDataJSON=$regClientB64;authenticatorData=$regAuth
    }|ConvertTo-Json -Compress)
    if ($registered.status -ne "registered") { throw "Passkey EdDSA register basarisiz." }

    $chLogin = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body (@{username=$username;flow="login";tenantId=$tenantId;rpId=$rpId;origin=$origin}|ConvertTo-Json -Compress)
    $loginClientJson = (@{ type="webauthn.get"; challenge=$chLogin.challenge; origin=$origin } | ConvertTo-Json -Compress)
    $loginClientB64 = Convert-ToBase64Url -Bytes ([System.Text.Encoding]::UTF8.GetBytes($loginClientJson))
    $loginAuth = Get-AuthenticatorDataB64 -RpId $rpId -SignCount 2
    $sig = Get-Ed25519SignatureB64 -SecretKey64 $kp.SecretKey -AuthenticatorDataB64 $loginAuth -ClientDataJson $loginClientJson
    $login = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/login") -Method Post -ContentType "application/json" -Body (@{
        username=$username;challengeId=$chLogin.challengeId;challenge=$chLogin.challenge;credentialId=$credentialId;signCount=2;rpId=$rpId;origin=$origin;
        clientDataType="webauthn.get";clientDataJSON=$loginClientB64;authenticatorData=$loginAuth;signature=$sig
    }|ConvertTo-Json -Compress)
    if ([string]::IsNullOrWhiteSpace($login.accessToken)) { throw "Passkey EdDSA login basarisiz." }

    $chBad = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body (@{username=$username;flow="login";tenantId=$tenantId;rpId=$rpId;origin=$origin}|ConvertTo-Json -Compress)
    $badClientJson = (@{ type="webauthn.get"; challenge=$chBad.challenge; origin=$origin } | ConvertTo-Json -Compress)
    $badClientB64 = Convert-ToBase64Url -Bytes ([System.Text.Encoding]::UTF8.GetBytes($badClientJson))
    $badAuth = Get-AuthenticatorDataB64 -RpId $rpId -SignCount 3
    $badSig = Get-Ed25519SignatureB64 -SecretKey64 $wrong.SecretKey -AuthenticatorDataB64 $badAuth -ClientDataJson $badClientJson
    $badBlocked = $false
    try {
        Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/login") -Method Post -ContentType "application/json" -Body (@{
            username=$username;challengeId=$chBad.challengeId;challenge=$chBad.challenge;credentialId=$credentialId;signCount=3;rpId=$rpId;origin=$origin;
            clientDataType="webauthn.get";clientDataJSON=$badClientB64;authenticatorData=$badAuth;signature=$badSig
        }|ConvertTo-Json -Compress) | Out-Null
    } catch {
        $badBlocked = $true
    }
    if (-not $badBlocked) { throw "EdDSA invalid signature engellenmedi." }

    Write-Output "PASS: Passkey EdDSA COSE register/login + invalid signature bloklama calisti."
}
finally {
    if ($server -and !$server.HasExited) {
        Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
    }
}

