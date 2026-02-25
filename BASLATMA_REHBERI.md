# 🚀 Media Server - Başlatma Rehberi

## 📋 İçindekiler
1. [Hızlı Başlangıç (Masaüstü)](#hızlı-başlangıç-masaüstü)
2. [Hızlı Başlangıç (Sunucu)](#hızlı-başlangıç-sunucu)
3. [Gereksinimler](#gereksinimler)
4. [Projeyi Başlatma](#projeyi-başlatma)
5. [Test Etme](#test-etme)
6. [Sorun Giderme](#sorun-giderme)

---

## ⚡ Hızlı Başlangıç (Masaüstü - Önerilen)

En kolay yöntem Electron uygulamasını kullanmaktır:

```powershell
# 1. Launcher klasörüne git
cd launcher

# 2. Uygulamayı başlat
npm start
```
Bu komut sunucuyu ve arayüzü sizin için otomatik açar.

---

## ⚡ Hızlı Başlangıç (Sadece Sunucu)

### 1️⃣ **Sunucuyu Başlat**
```powershell
# Terminal/PowerShell'i aç
cd C:\Users\thor\Desktop\rela

# Sunucuyu başlat
.\media_server.exe
```

### 2️⃣ **Tarayıcıda Aç**
```
http://localhost:8080
```

✅ **HAZIR!** Web arayüzü açılmalı.

---

## 🔧 Gereksinimler

### **Zaten Yüklü Olanlar:**
- ✅ **TDM-GCC** (C++ derleyici) - `C:\TDM-GCC-64\bin\g++.exe`
- ✅ **PowerShell 7** - Komutları çalıştırmak için
- ✅ **Windows 10/11** - İşletim sistemi

### **Gerekli Portlar:**
- 🌐 **Port 8080** - Media server (HTTP)

> **Not:** Port 8080 kullanımda olmamalı. Eğer başka bir uygulama kullanıyorsa kapatın.

---

## 🎬 Projeyi Başlatma

### **Seçenek 1: Hazır Executable Kullan (Önerilen)**

En basit yöntem! Sadece derlenmiş programı çalıştırın:

```powershell
# 1. Proje klasörüne git
cd C:\Users\thor\Desktop\rela

# 2. Sunucuyu başlat
.\media_server.exe
```

**Çıktı:**
```
========================================
  🚀 MEDIA SERVER API v2.0
========================================
Server: http://localhost:8080

📡 Available Services:
  💬 Chat Messages
  🎤 Voice Calls (WebRTC Signaling)
  📹 Video Calls (WebRTC Signaling)
  📺 Live Streaming
  🎬 Screen Recording

⚡ Endpoints:
  GET  / - API Documentation
  GET  /api/health - Health Check

Press Ctrl+C to stop
========================================
```

### **Seçenek 2: Yeniden Derle ve Başlat**

Kod değiştirdiyseniz veya yeni derlemek istiyorsanız:

```powershell
# 1. Proje klasörüne git
cd C:\Users\thor\Desktop\rela

# 2. Yeniden derle
.\build_media.ps1

# 3. Başlat
.\media_server.exe
```

---

## 🌐 Web Arayüzünü Kullanma

### **1. Tarayıcıyı Aç**
- Tarayıcıda şu adresi aç: **http://localhost:8080**

### **2. Kullanılabilir Özellikler**

#### **💬 Mesajlaşma**
- Mesaj gönder
- Mesaj listesini görüntüle
- Gerçek zamanlı güncellemeler

#### **🎤 Sesli Arama**
- Arama başlat
- Aktif aramalar
- WebRTC sinyal alışverişi

#### **📹 Görüntülü Arama**
- Video arama başlat
- ICE aday değişimi
- Aktif video aramalar

#### **📺 Canlı Yayın**
- Yayın başlat
- Yayını izle / Ayrıl
- İzleyici sayısı takibi

#### **🎬 Ekran Kaydı** ⭐
- **1080p/720p/480p** kalite seçenekleri
- **Tam ekran / Pencere / Sekme** kayıt
- **Kayıt yeri seçme** (masaüstü, belgeler, vb.)
- Kayıt geçmişi
- Ses dahil etme seçeneği

---

## 🧪 Test Etme

### **Manuel Test (Web Arayüzü)**
1. `http://localhost:8080` aç
2. Her bir sekmeyi test et
3. Fonksiyonları dene

### **Otomatik Test (API)**

#### **Tüm API'leri Test Et:**
```powershell
.\test_media.ps1
```

#### **Sadece Ekran Kaydı Test Et:**
```powershell
.\test_recording.ps1
```

**Beklenen Çıktı:**
```
=== Testing Screen Recording API ===

1. Starting screen recording...
✓ Recording started successfully

4. Stopping recording...
✓ Recording stopped successfully
  Status: stopped
  Duration: 3s
```

---

## 🐛 Sorun Giderme

### **Problem 1: Sunucu başlamıyor**
```
"bind failed" hatası
```

**Çözüm:**
```powershell
# Port 8080'i kullanan programı bul ve kapat
Get-Process | Where-Object {$_.ProcessName -like "*media_server*"} | Stop-Process -Force

# Tekrar başlat
.\media_server.exe
```

---

### **Problem 2: `media_server.exe` bulunamıyor**
```
"The term '.\media_server.exe' is not recognized"
```

**Çözüm:**
```powershell
# Doğru klasörde olduğunuzdan emin olun
cd C:\Users\thor\Desktop\rela

# Dosya var mı kontrol et
ls media_server.exe

# Yoksa derleyin
.\build_media.ps1
```

---

### **Problem 3: Web sayfası açılmıyor**
```
"This site can't be reached"
```

**Çözüm:**
1. Sunucu çalışıyor mu kontrol et:
```powershell
Get-Process media_server
```

2. Health check yap:
```powershell
curl http://localhost:8080/api/health
```

3. Firewall kontrolü:
   - Windows Defender Firewall ayarlarını kontrol et
   - Port 8080'e izin verdiğinden emin ol

---

### **Problem 4: Ekran kaydı çalışmıyor**

**A) Kayıt yeri dialogu açılmıyor:**
- **Chrome/Edge** kullanın (File System Access API gerekli)
- Tarayıcıyı güncelleyin

**B) "KAYIT EDİLİYOR" durumunda takılı kaldı:**
```powershell
# Sunucuyu yeniden başlat
Get-Process media_server | Stop-Process -Force
.\media_server.exe
```

**C) Kayıt başlamıyor:**
- Ekran paylaşımı iznini verin
- HTTPS değil, HTTP kullanın (localhost'ta çalışır)
- Tarayıcı güncel mi kontrol edin

---

### **Problem 5: Derleme hatası**
```
"g++ is not recognized"
```

**Çözüm:**
```powershell
# GCC yolunu kontrol et
Test-Path "C:\TDM-GCC-64\bin\g++.exe"

# Yoksa TDM-GCC'yi yeniden yükle
# https://jmeubank.github.io/tdm-gcc/
```

---

## 📚 Ek Belgeler

### **API Dokümantasyonu**
```powershell
# Tarayıcıda API dokümantasyonunu görüntüle
http://localhost:8080
```

### **Test Scriptleri**
- `test_media.ps1` - Tüm API endpoint'lerini test eder
- `test_recording.ps1` - Ekran kaydı API'sini test eder
- `test_debug.ps1` - Debug amaçlı basit test

### **Build Scripti**
- `build_media.ps1` - C++ kodunu derler

---

## 🔄 PC Yeniden Başladıktan Sonra

### **1. Terminal Aç**
```powershell
PowerShell
```

### **2. Proje Klasörüne Git**
```powershell
cd C:\Users\thor\Desktop\rela
```

### **3. Sunucuyu Başlat**
```powershell
.\media_server.exe
```

### **4. Tarayıcıda Aç**
```
http://localhost:8080
```

> **İpucu:** Bu adımları bir `.bat` dosyasına kaydedebilirsiniz!

---

## 🎯 Hızlı Başlatma Script'i (İsteğe Bağlı)

Hızlı başlatmak için bir kısayol oluşturun:

**`start_server.bat`** (Yeni dosya oluştur):
```batch
@echo off
echo Starting Media Server...
cd /d "C:\Users\thor\Desktop\rela"
start "" "http://localhost:8080"
media_server.exe
pause
```

**Kullanımı:**
1. `start_server.bat` dosyasına çift tıkla
2. Sunucu başlar ve tarayıcı otomatik açılır ✨

---

## 📞 Özellikler Özeti

| Özellik | Durum | Açıklama |
|---------|-------|----------|
| 💬 Mesajlaşma | ✅ | Gerçek zamanlı mesajlaşma |
| 🎤 Sesli Arama | ✅ | WebRTC sesli arama |
| 📹 Görüntülü Arama | ✅ | WebRTC video arama |
| 📺 Canlı Yayın | ✅ | Çoklu izleyici desteği |
| 🎬 Ekran Kaydı | ✅ | 1080p, kayıt yeri seçimi |
| 💾 MongoDB | 📝 | Dokümante edildi (entegre değil) |

---

## 💡 İpuçları

1. **Performans:**
   - Kayıt kalitesini 720p yapın (daha az CPU)
   - Chrome/Edge kullanın (en iyi performans)

2. **Güvenlik:**
   - Sadece localhost'ta çalışır
   - Dış ağdan erişim yok

3. **Kayıt:**
   - WebM formatında kaydeder
   - VLC ile oynatabilirsiniz

4. **Geliştirme:**
   - Kod değişikliği → `build_media.ps1` → `media_server.exe`

---

## 🎉 Başarılı Başlatma Kontrol Listesi

- [ ] Terminal açıldı
- [ ] Proje klasörüne gidildi (`cd C:\Users\thor\Desktop\rela`)
- [ ] Sunucu başlatıldı (`.\media_server.exe`)
- [ ] Sunucu mesajı göründü ("Server: http://localhost:8080")
- [ ] Tarayıcıda `http://localhost:8080` açıldı
- [ ] Web arayüzü yüklendi
- [ ] Özellikler test edildi

✨ **Hepsi tamamsa, başarıyla çalışıyor!**

---

**Son Güncelleme:** 2025-10-21  
**Versiyon:** 2.0



