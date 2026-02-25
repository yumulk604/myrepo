param(
    [string]$File = "GELISTIRME_DURUMU.md",
    [ValidateSet("done", "todo")]
    [string]$Type,
    [string]$Section = "Durum Ozeti",
    [Parameter(Mandatory = $true)]
    [string]$Text
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $File)) {
    @"
# Proje Gagabunto Media - Gelistirme Durumu

## Durum Ozeti

## Mimari Hedef (Onceliklendirilmis)

## Sonraki Net Adim
"@ | Set-Content -LiteralPath $File -Encoding UTF8
}

$marker = if ($Type -eq "done") { "[+]" } else { "[-]" }
$entry = "- $marker $Text"

$content = Get-Content -LiteralPath $File -Raw
$sectionHeader = "## $Section"

if ($content -notmatch [regex]::Escape($sectionHeader)) {
    $content = $content.TrimEnd() + "`r`n`r`n$sectionHeader`r`n"
}

$start = $content.IndexOf($sectionHeader)
$afterHeader = $content.IndexOf("`n", $start)
if ($afterHeader -lt 0) {
    $afterHeader = $content.Length - 1
}
$insertPos = $afterHeader + 1

$nextSection = $content.IndexOf("`n## ", $insertPos)
if ($nextSection -lt 0) {
    $nextSection = $content.Length
}

$before = $content.Substring(0, $insertPos)
$sectionBody = $content.Substring($insertPos, $nextSection - $insertPos).TrimEnd()
$after = $content.Substring($nextSection)

if ($sectionBody.Length -eq 0) {
    $sectionBody = "$entry"
} else {
    $sectionBody = "$sectionBody`r`n$entry"
}

$newContent = ($before.TrimEnd() + "`r`n" + $sectionBody.TrimEnd() + "`r`n" + $after.TrimStart())
$newContent | Set-Content -LiteralPath $File -Encoding UTF8

Write-Output "Updated: $File"
Write-Output "Section: $Section"
Write-Output "Entry: $entry"
