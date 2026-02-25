param(
    [int]$Port = 18741,
    [string]$JwtSecret = "passkey_audit_secret",
    [int]$TimeoutSec = 20
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$exePath = Join-Path $projectRoot "media_server.exe"
$auditPath = Join-Path $projectRoot "data\auth_audit.jsonl"

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
    if (Test-Path $auditPath) { Remove-Item $auditPath -Force }

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
        GIGACHAD_STORAGE_MODE = "hybrid"
    }

    $server = Start-ProcessWithEnvironment -FilePath $exePath -WorkingDirectory $projectRoot -Environment $childEnv
    if (!(Wait-Health -Port $Port -TimeoutSec $TimeoutSec)) { throw "Server hazir degil." }

    $runId = Get-Date -Format "yyyyMMddHHmmssfff"
    $username = "audit-user-$runId"
    $tenantId = "tenant-audit"
    $credentialId = "cred-audit-$runId"
    $publicKey = "audit-public"
    $rpId = "localhost"
    $origin = "https://localhost"

    $chReg = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body (@{username=$username;flow="register";tenantId=$tenantId;rpId=$rpId;origin=$origin}|ConvertTo-Json -Compress)
    $regClientJson = (@{type="webauthn.create";challenge=$chReg.challenge;origin=$origin}|ConvertTo-Json -Compress)
    $regClientB64 = Convert-ToBase64Url -Bytes ([System.Text.Encoding]::UTF8.GetBytes($regClientJson))
    $regAuth = Get-AuthenticatorDataB64 -RpId $rpId -SignCount 1
    Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/register") -Method Post -ContentType "application/json" -Body (@{username=$username;challengeId=$chReg.challengeId;challenge=$chReg.challenge;credentialId=$credentialId;publicKey=$publicKey;signCount=1;rpId=$rpId;origin=$origin;clientDataType="webauthn.create";clientDataJSON=$regClientB64;authenticatorData=$regAuth}|ConvertTo-Json -Compress) | Out-Null

    $chLogin = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/challenge") -Method Post -ContentType "application/json" -Body (@{username=$username;flow="login";tenantId=$tenantId;rpId=$rpId;origin=$origin}|ConvertTo-Json -Compress)
    $loginClientJson = (@{type="webauthn.get";challenge=$chLogin.challenge;origin=$origin}|ConvertTo-Json -Compress)
    $loginClientB64 = Convert-ToBase64Url -Bytes ([System.Text.Encoding]::UTF8.GetBytes($loginClientJson))
    $loginAuth = Get-AuthenticatorDataB64 -RpId $rpId -SignCount 2
    $loginSig = Get-SignatureB64 -CredentialKey $publicKey -AuthenticatorDataB64 $loginAuth -ClientDataJson $loginClientJson
    $login = Invoke-RestMethod -Uri ("http://127.0.0.1:" + $Port + "/api/auth/passkey/login") -Method Post -ContentType "application/json" -Body (@{username=$username;challengeId=$chLogin.challengeId;challenge=$chLogin.challenge;credentialId=$credentialId;signCount=2;rpId=$rpId;origin=$origin;clientDataType="webauthn.get";clientDataJSON=$loginClientB64;authenticatorData=$loginAuth;signature=$loginSig}|ConvertTo-Json -Compress)
    if ([string]::IsNullOrWhiteSpace($login.accessToken)) { throw "Passkey login basarisiz." }

    if (!(Test-Path $auditPath)) { throw "auth_audit.jsonl olusmadi." }
    $lines = Get-Content $auditPath | Where-Object { $_.Trim().Length -gt 0 }
    if ($lines.Count -lt 3) { throw "Audit satiri yetersiz." }
    $rows = $lines | ForEach-Object { $_ | ConvertFrom-Json }

    $required = @("passkey.challenge", "passkey.register", "passkey.login")
    foreach ($evt in $required) {
        if (-not ($rows | Where-Object { $_.eventType -eq $evt })) {
            throw "Audit event eksik: $evt"
        }
    }

    Write-Output "PASS: Passkey auth audit log olustu ve event tipleri dogrulandi."
}
finally {
    if ($server -and !$server.HasExited) {
        Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
    }
}
