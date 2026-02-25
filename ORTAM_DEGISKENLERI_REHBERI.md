# 🔧 Ortam Değişkenleri Yedekleme ve Geri Yükleme Rehberi

Bu klasörde Windows ortam değişkenlerinizi yedeklemek ve geri yüklemek için gerekli tüm araçlar bulunmaktadır.

## 📋 Dosyalar

- **`backup_env_vars.ps1`** - Ortam değişkenlerini yedekler
- **`restore_env_vars.ps1`** - Yedeklenmiş değişkenleri geri yükler
- **`environment_variables_backup.json`** - Yedek (JSON formatı)
- **`environment_variables_backup.txt`** - Yedek (Okunabilir metin formatı)

---

## 💾 Yedekleme Nasıl Yapılır?

### Adım 1: Backup Scriptini Çalıştırın

PowerShell'i açın ve şu komutu çalıştırın:

```powershell
cd C:\Users\thor\Desktop\rela
.\backup_env_vars.ps1
```

veya

```powershell
powershell -ExecutionPolicy Bypass -File backup_env_vars.ps1
```

### Sonuç

✓ İki dosya oluşturulacak:
- `environment_variables_backup.json` - Makinenin okuyabileceği format
- `environment_variables_backup.txt` - Sizin okuyabileceğiniz format

---

## 🔄 Geri Yükleme Nasıl Yapılır?

### Format Attıktan Sonra:

1. **Bu klasörü USB'ye kopyalayın** veya bulut depolama kullanın
2. Format sonrası bu dosyaları yeni sisteme kopyalayın
3. PowerShell'i **YÖNETİCİ OLARAK** açın
4. Şu komutu çalıştırın:

```powershell
cd C:\Users\thor\Desktop\rela
.\restore_env_vars.ps1
```

### ⚠️ Önemli Notlar:

- **Kullanıcı değişkenleri** için yönetici yetkisi gerekmez
- **Sistem değişkenleri** için PowerShell'i **Yönetici olarak** çalıştırmalısınız
- Bazı kritik sistem değişkenleri (ComSpec, OS, vb.) otomatik atlanır

---

## 📖 Manuel Geri Yükleme

Eğer scriptleri kullanmak istemezseniz, `environment_variables_backup.txt` dosyasını açıp manuel olarak ekleyebilirsiniz:

### Windows UI ile:
1. `Windows + R` > `sysdm.cpl` yazın > Enter
2. "Gelişmiş" (Advanced) sekmesi > "Ortam Değişkenleri" (Environment Variables)
3. TXT dosyasındaki değişkenleri manuel olarak ekleyin

### PowerShell ile:
```powershell
# Kullanıcı değişkeni eklemek için:
[System.Environment]::SetEnvironmentVariable("DEĞİŞKEN_ADI", "DEĞER", [System.EnvironmentVariableTarget]::User)

# Sistem değişkeni eklemek için (Yönetici gerekli):
[System.Environment]::SetEnvironmentVariable("DEĞİŞKEN_ADI", "DEĞER", [System.EnvironmentVariableTarget]::Machine)
```

---

## 🎯 Özel Senaryolar

### Sadece PATH değişkenini geri yüklemek:

```powershell
$data = Get-Content environment_variables_backup.json | ConvertFrom-Json
$userPath = $data.UserVariables.Path
[System.Environment]::SetEnvironmentVariable("Path", $userPath, [System.EnvironmentVariableTarget]::User)
```

### Belirli bir değişkeni kontrol etmek:

```powershell
$data = Get-Content environment_variables_backup.json | ConvertFrom-Json
$data.UserVariables.JAVA_HOME
```

---

## 📊 Yedeklenen Değişkenler

### Kullanıcı Değişkenleri (7 adet):
- ChocolateyLastPathUpdate
- OneDrive
- OneDriveConsumer
- Path
- TEMP
- TMP
- Ve diğerleri...

### Sistem Değişkenleri (19 adet):
- ComSpec
- DriverData
- JAVA_HOME
- NUMBER_OF_PROCESSORS
- OS
- Path
- PATHEXT
- Ve diğerleri...

---

## 🛡️ Güvenlik

- Bu dosyaları **güvenli bir yerde** saklayın (USB, bulut depolama)
- PATH değişkenleri gibi hassas yollar içerebilir
- Sistem değişkenlerini değiştirmek yönetici yetkisi gerektirir
- Format öncesi **mutlaka yedek alın**

---

## 💡 İpuçları

1. **Düzenli yedekleyin**: Önemli bir program yükledikten sonra yedek alın
2. **Bulut depolama**: Dosyaları OneDrive/Google Drive'a koyun
3. **Tarih ekleyin**: Birden fazla yedek alıyorsanız tarihlendirin
4. **Test edin**: Format atmadan önce restore scriptinin çalıştığını test edin

---

## ❓ Sorun Giderme

### "Script yürütme devre dışı" hatası:

```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### Yönetici yetkisi nasıl alınır:

1. PowerShell'e sağ tıklayın
2. "Yönetici olarak çalıştır" seçin

### Değişkenler etkili olmuyor:

- Tüm açık terminalleri kapatın ve yeniden açın
- Bazı programlar yeniden başlatma gerektirebilir
- Ekstrem durumlarda bilgisayarı yeniden başlatın

---

## 📞 Ek Bilgi

Oluşturulma tarihi: $(Get-Date -Format "yyyy-MM-dd")
Yedek bilgisayar: thor

**Not**: Bu dosyalar Windows 10/11 için oluşturulmuştur.



