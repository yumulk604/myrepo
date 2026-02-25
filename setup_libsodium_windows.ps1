param(
    [string]$Version = "1.0.21-RELEASE"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$zipName = "libsodium-$Version-msvc.zip"
$url = "https://github.com/jedisct1/libsodium/releases/download/$Version/$zipName"
$zipPath = Join-Path $projectRoot $zipName
$extractDir = Join-Path $projectRoot "_libsodium_extract"
$targetDll = Join-Path $projectRoot "libsodium.dll"

Write-Output ("Downloading " + $url)
cmd /c curl -L -o "$zipPath" "$url"
if ($LASTEXITCODE -ne 0) {
    throw "libsodium zip indirilemedi."
}

if (Test-Path $extractDir) {
    Remove-Item $extractDir -Recurse -Force
}
Expand-Archive -Path $zipPath -DestinationPath $extractDir

$candidate = Get-ChildItem $extractDir -Recurse -Filter libsodium.dll |
    Where-Object { $_.FullName -match "\\x64\\Release\\v143\\dynamic\\libsodium.dll$" } |
    Select-Object -First 1

if (-not $candidate) {
    $candidate = Get-ChildItem $extractDir -Recurse -Filter libsodium.dll |
        Where-Object { $_.FullName -match "\\x64\\Release\\.*\\dynamic\\libsodium.dll$" } |
        Select-Object -First 1
}
if (-not $candidate) {
    $candidate = Get-ChildItem $extractDir -Recurse -Filter libsodium.dll | Select-Object -First 1
}
if (-not $candidate) {
    throw "libsodium.dll arsivde bulunamadi."
}

Copy-Item $candidate.FullName $targetDll -Force
Write-Output ("Installed libsodium.dll => " + $targetDll)

