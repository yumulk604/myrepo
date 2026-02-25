# Environment Variables Restore Script
# Yedeklenmiş ortam değişkenlerini geri yükler

# Yönetici yetkisi kontrolü (Sistem değişkenleri için gerekli)
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "⚠ UYARI: Sistem değişkenlerini geri yüklemek için Yönetici yetkisi gereklidir!" -ForegroundColor Yellow
    Write-Host "Sadece kullanıcı değişkenleri geri yüklenecek.`n" -ForegroundColor Yellow
    Write-Host "Sistem değişkenlerini de geri yüklemek için bu scripti Yönetici olarak çalıştırın.`n" -ForegroundColor Cyan
    $restoreSystem = $false
} else {
    Write-Host "✓ Yönetici yetkisi tespit edildi. Tüm değişkenler geri yüklenebilir.`n" -ForegroundColor Green
    $restoreSystem = $true
}

# JSON dosyasını oku
$jsonFile = "environment_variables_backup.json"

if (-not (Test-Path $jsonFile)) {
    Write-Host "✗ Hata: $jsonFile bulunamadı!" -ForegroundColor Red
    Write-Host "Önce backup_env_vars.ps1 scriptini çalıştırın." -ForegroundColor Yellow
    exit 1
}

Write-Host "Yedek dosyası okunuyor..." -ForegroundColor Cyan
$data = Get-Content $jsonFile -Encoding UTF8 | ConvertFrom-Json

# Kullanıcı değişkenlerini geri yükle
Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "KULLANICI DEĞİŞKENLERİ GERİ YÜKLENİYOR" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Cyan

$userCount = 0
foreach ($prop in $data.UserVariables.PSObject.Properties) {
    $name = $prop.Name
    $value = $prop.Value
    
    try {
        [System.Environment]::SetEnvironmentVariable($name, $value, [System.EnvironmentVariableTarget]::User)
        Write-Host "✓ $name" -ForegroundColor Green
        $userCount++
    } catch {
        Write-Host "✗ $name - HATA: $_" -ForegroundColor Red
    }
}

Write-Host "`n$userCount kullanıcı değişkeni geri yüklendi." -ForegroundColor Green

# Sistem değişkenlerini geri yükle (sadece yönetici ise)
if ($restoreSystem) {
    Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "SİSTEM DEĞİŞKENLERİ GERİ YÜKLENİYOR" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Cyan
    
    $systemCount = 0
    foreach ($prop in $data.SystemVariables.PSObject.Properties) {
        $name = $prop.Name
        $value = $prop.Value
        
        # Kritik sistem değişkenlerini atla
        $skipVars = @("ComSpec", "OS", "NUMBER_OF_PROCESSORS", "PROCESSOR_ARCHITECTURE", 
                      "PROCESSOR_IDENTIFIER", "PROCESSOR_LEVEL", "PROCESSOR_REVISION",
                      "SystemRoot", "windir")
        
        if ($skipVars -contains $name) {
            Write-Host "⊘ $name (atlandı - sistem değişkeni)" -ForegroundColor DarkGray
            continue
        }
        
        try {
            [System.Environment]::SetEnvironmentVariable($name, $value, [System.EnvironmentVariableTarget]::Machine)
            Write-Host "✓ $name" -ForegroundColor Green
            $systemCount++
        } catch {
            Write-Host "✗ $name - HATA: $_" -ForegroundColor Red
        }
    }
    
    Write-Host "`n$systemCount sistem değişkeni geri yüklendi." -ForegroundColor Green
}

Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Green
Write-Host "✓ GERİ YÜKLEME TAMAMLANDI!" -ForegroundColor Green
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Green

Write-Host "Not: Değişikliklerin etkili olması için yeni bir terminal açmanız gerekebilir." -ForegroundColor Yellow



