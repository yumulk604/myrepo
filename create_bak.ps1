param(
    [Parameter(Mandatory = $true, ValueFromRemainingArguments = $true)]
    [string[]]$Files
)

$ErrorActionPreference = "Stop"
$ts = Get-Date -Format "yyyyMMdd_HHmmss"

foreach ($file in $Files) {
    if (-not (Test-Path -LiteralPath $file)) {
        Write-Warning "File not found: $file"
        continue
    }

    Copy-Item -LiteralPath $file -Destination ($file + ".bak") -Force
    Copy-Item -LiteralPath $file -Destination ($file + ".bak." + $ts) -Force
    Write-Output "BAK_CREATED $file.bak"
    Write-Output "SNAPSHOT_CREATED $file.bak.$ts"
}
