# Environment Variables Backup Script
# Kullanıcı ve Sistem değişkenlerini yedekler

Write-Host "Ortam değişkenleri yedekleniyor..." -ForegroundColor Cyan

# Kullanıcı ve Sistem değişkenlerini al
$userVars = [System.Environment]::GetEnvironmentVariables([System.EnvironmentVariableTarget]::User)
$systemVars = [System.Environment]::GetEnvironmentVariables([System.EnvironmentVariableTarget]::Machine)

# JSON formatında kaydet
$data = @{
    UserVariables = $userVars
    SystemVariables = $systemVars
    BackupDate = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
    ComputerName = $env:COMPUTERNAME
}

$data | ConvertTo-Json -Depth 10 | Out-File -Encoding UTF8 'environment_variables_backup.json'

# TXT formatında kaydet (daha okunabilir)
$txtContent = @"
╔══════════════════════════════════════════════════════════════════════
║ ORTAM DEĞİŞKENLERİ YEDEĞİ
║ Tarih: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
║ Bilgisayar: $env:COMPUTERNAME
╚══════════════════════════════════════════════════════════════════════

═══════════════════════════════════════════════════════════════════════
  KULLANICI DEĞİŞKENLERİ (USER VARIABLES)
═══════════════════════════════════════════════════════════════════════

"@

foreach($key in $userVars.Keys | Sort-Object) {
    $txtContent += "$key = $($userVars[$key])`r`n"
}

$txtContent += @"

═══════════════════════════════════════════════════════════════════════
  SİSTEM DEĞİŞKENLERİ (SYSTEM VARIABLES)
═══════════════════════════════════════════════════════════════════════

"@

foreach($key in $systemVars.Keys | Sort-Object) {
    $txtContent += "$key = $($systemVars[$key])`r`n"
}

$txtContent += @"

═══════════════════════════════════════════════════════════════════════
  GERİ YÜKLEME TALİMATLARI
═══════════════════════════════════════════════════════════════════════

KULLANICI DEĞİŞKENİ EKLEMEK İÇİN:
[System.Environment]::SetEnvironmentVariable("DEĞİŞKEN_ADI", "DEĞER", [System.EnvironmentVariableTarget]::User)

SİSTEM DEĞİŞKENİ EKLEMEK İÇİN (Yönetici yetkisi gerekli):
[System.Environment]::SetEnvironmentVariable("DEĞİŞKEN_ADI", "DEĞER", [System.EnvironmentVariableTarget]::Machine)

VEYA restore_env_vars.ps1 scriptini kullanabilirsiniz.
"@

$txtContent | Out-File -Encoding UTF8 'environment_variables_backup.txt'

Write-Host "`n✓ Yedekleme tamamlandı!" -ForegroundColor Green
Write-Host "  - environment_variables_backup.json" -ForegroundColor Yellow
Write-Host "  - environment_variables_backup.txt" -ForegroundColor Yellow
Write-Host "`nKullanıcı değişkenleri: $($userVars.Count)" -ForegroundColor Cyan
Write-Host "Sistem değişkenleri: $($systemVars.Count)" -ForegroundColor Cyan



