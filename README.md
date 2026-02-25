# 🚀 C++ Media Server API

Modern bir medya sunucusu! Mesajlaşma, sesli/görüntülü arama, canlı yayın ve **ekran kaydı** özellikleri ile tam donanımlı medya platformu. Saf C++17 ile yazılmış, harici kütüphane gerektirmez.

## ✨ Özellikler

### 💬 **Mesajlaşma**
- Gerçek zamanlı mesaj gönderme ve alma
- Thread-safe mesaj depolama
- JSON format desteği

### 🎤 **Sesli Arama**
- WebRTC sinyalizasyon desteği
- Aktif arama takibi
- SDP offer/answer değişimi

### 📹 **Görüntülü Arama**
- WebRTC video arama
- ICE aday değişimi
- Çoklu video oturumu desteği

### 📺 **Canlı Yayın**
- Canlı yayın başlatma
- Çoklu izleyici desteği
- Gerçek zamanlı izleyici sayısı takibi
- İzleyici katılma/ayrılma yönetimi

### 🎬 **Ekran Kaydı** ⭐ YENİ!
- **1080p/720p/480p** kalite seçenekleri
- **Tam ekran / Pencere / Sekme** kayıt modu
- Ses dahil etme seçeneği
- **Kayıt yeri seçimi** (File System Access API)
- Kayıt geçmişi ve durum takibi
- İki şekilde durdurma (web + tarayıcı bildirimi)

### 🌐 **Web Arayüzü**
- Modern, responsive tasarım
- Gerçek zamanlı güncellemeler
- Kullanıcı dostu kontroller
- Toast bildirimleri

### 🔧 **Teknik Özellikler**
- ✅ **Sıfır bağımlılık** - Sadece C++17 ve Windows Sockets
- ✅ Thread-safe operasyonlar
- ✅ RESTful API tasarımı
- ✅ CORS desteği
- ✅ Tek executable dosya
- ✅ Kolay dağıtım

---

## ⚡ Hızlı Başlangıç

### Yöntem 1: BAT Dosyası (Önerilen) 🎯
```batch
start_server.bat
```
Çift tıkla, hazır! Tarayıcı otomatik açılır.

### Yöntem 2: Manuel Başlatma
```powershell
# Sunucuyu başlat
.\media_server.exe

# Tarayıcıda aç
http://localhost:8080
```

### Test Et
```powershell
# Tüm API'leri test et
.\test_media.ps1

# Sadece ekran kaydı test et
.\test_recording.ps1
```

---

## 📖 Detaylı Başlatma Rehberi

PC'yi yeniden başlattıktan sonra ne yapacağını mı merak ediyorsun?

👉 **[BASLATMA_REHBERI.md](BASLATMA_REHBERI.md)** - Tam rehber  
👉 **[QUICKSTART.md](QUICKSTART.md)** - Hızlı başlangıç

---

## 📡 API Endpoints

### 💬 Mesajlaşma API
```
POST   /api/chat/messages       - Mesaj gönder
GET    /api/chat/messages       - Tüm mesajları getir
DELETE /api/chat/messages/:id   - Mesaj sil
```

### 🎤 Sesli Arama API
```
POST /api/voice/call            - Sesli arama başlat
POST /api/voice/answer/:callId  - Aramayı yanıtla
POST /api/voice/end/:callId     - Aramayı sonlandır
GET  /api/voice/active          - Aktif aramalar
```

### 📹 Görüntülü Arama API
```
POST /api/video/call            - Video arama başlat
POST /api/video/ice/:callId     - ICE candidate ekle
POST /api/video/answer/:callId  - Video aramasını yanıtla
POST /api/video/end/:callId     - Video aramasını sonlandır
GET  /api/video/active          - Aktif video aramalar
```

### 📺 Canlı Yayın API
```
POST /api/stream/start           - Canlı yayın başlat
GET  /api/stream/live            - Tüm canlı yayınlar
POST /api/stream/join/:streamId  - Yayına katıl (izleyici)
POST /api/stream/leave/:streamId - Yayından ayrıl
POST /api/stream/end/:streamId   - Yayını sonlandır
GET  /api/stream/:streamId       - Yayın detayları
```

### 🎬 Ekran Kaydı API ⭐
```
POST /api/recording/start         - Kayıt başlat
POST /api/recording/stop/:recId   - Kaydı durdur
GET  /api/recording/list          - Tüm kayıtlar
GET  /api/recording/:recId        - Kayıt detayları
```

### 🏥 Sistem
```
GET /api/health  - Sunucu sağlık kontrolü
GET /            - API dokümantasyonu
```

---

## 💻 Web Arayüzü Kullanımı

Tarayıcıda `http://localhost:8080` adresini açın ve şu özellikleri kullanın:

### 1. 💬 **Mesajlaşma**
- Mesaj yazın ve gönderin
- Tüm mesajları görüntüleyin
- Gerçek zamanlı güncellemeler

### 2. 🎤 **Sesli Arama**
- "Ara" butonuna tıklayın
- WebRTC bağlantısı otomatik kurulur
- "Bitir" ile aramayı sonlandırın

### 3. 📹 **Görüntülü Arama**
- Video arama başlatın
- Kamera ve mikrofon erişimi verin
- ICE adayları otomatik değişilir

### 4. 📺 **Canlı Yayın**
- Yayın başlatın
- İzleyiciler "İzle" butonuyla katılabilir
- İzleyici sayısı gerçek zamanlı güncellenir

### 5. 🎬 **Ekran Kaydı** 
**En Detaylı Özellik!**

1. **Kalite Seç:** 1080p, 720p veya 480p
2. **Kayıt Türü:** Tam ekran, Pencere veya Tarayıcı sekmesi
3. **Dosya Adı:** Özel dosya adı girin
4. **Ses:** Sistem sesini dahil et/etme
5. **Başlat:** Kaydedilecek ekranı seçin
6. **Durdur:** İki yöntem:
   - Web sitesindeki "Kaydı Durdur" butonu
   - Tarayıcı bildirimindeki "Paylaşımı Durdur" butonu
7. **Kaydet:** Kayıt yerini seçin (Masaüstü, Belgeler, vb.)
8. **Geçmiş:** Tüm kayıtları görüntüleyin

---

## 🧪 Test Örnekleri

### Otomatik Test Scriptleri
```powershell
# Tüm özellikleri test et
.\test_media.ps1

# Sadece ekran kaydı test et
.\test_recording.ps1
```

### Manuel API Testleri (PowerShell)

**Mesaj Gönder:**
```powershell
$msg = @{username="Ali"; content="Merhaba!"} | ConvertTo-Json
Invoke-RestMethod -Uri "http://localhost:8080/api/chat/messages" -Method Post -Body $msg -ContentType "application/json"
```

**Canlı Yayın Başlat:**
```powershell
$stream = @{userId="user1"; userName="Yayıncı"; title="Test Yayını"} | ConvertTo-Json
Invoke-RestMethod -Uri "http://localhost:8080/api/stream/start" -Method Post -Body $stream -ContentType "application/json"
```

**Ekran Kaydı Başlat:**
```powershell
$rec = @{userId="user1"; userName="Kayıtçı"; filename="test"; quality="1080p"; captureType="screen"} | ConvertTo-Json
Invoke-RestMethod -Uri "http://localhost:8080/api/recording/start" -Method Post -Body $rec -ContentType "application/json"
```

**Sağlık Kontrolü:**
```powershell
Invoke-RestMethod -Uri "http://localhost:8080/api/health"
```

## 📁 Proje Yapısı

```
rela/
├── 📝 Dokümantasyon
│   ├── README.md                 # Ana README (bu dosya)
│   ├── BASLATMA_REHBERI.md       # Detaylı başlatma rehberi
│   ├── QUICKSTART.md             # Hızlı başlangıç
│   ├── README_TR.md              # Türkçe README
│   ├── MONGODB_INTEGRATION.md    # MongoDB entegrasyon rehberi
│   ├── API_CHEATSHEET.md         # API referans
│   └── VISUAL_SUMMARY.txt        # Görsel özet
│
├── 🚀 Ana Dosyalar
│   ├── media_server.cpp          # C++ ana sunucu kodu
│   ├── media_server.exe          # Derlenmiş executable
│   ├── start_server.bat          # Hızlı başlatma scripti
│   └── .gitignore               # Git ignore kuralları
│
├── 🔨 Build & Test
│   ├── build_media.ps1           # Derleme scripti
│   ├── test_media.ps1            # Tüm API test scripti
│   ├── test_recording.ps1        # Ekran kaydı test scripti
│   └── test_debug.ps1            # Debug test scripti
│
└── 🌐 Web Arayüzü
    └── public/
        ├── index.html            # Ana HTML sayfası
        ├── styles.css            # CSS stilleri
        └── app.js                # JavaScript fonksiyonları
```

## 🛠️ Gereksinimler

### ✅ **Zaten Yüklü**
- **TDM-GCC** (C++ derleyici) - `C:\TDM-GCC-64\bin\g++.exe`
- **PowerShell 7**
- **Windows 10/11**

### 📦 **İsteğe Bağlı**
- **MongoDB** - Kalıcı veri depolama için (şu an dokümante edilmiş, kod entegrasyonu bekliyor)
- **Chrome/Edge** - Ekran kaydı için File System Access API gerekli

---

## 🔧 Manuel Derleme

### PowerShell ile (Önerilen)
```powershell
.\build_media.ps1
```

### g++ ile
```bash
g++ -std=c++17 media_server.cpp -o media_server.exe -lws2_32 -O2 -static -pthread
```

---

## 💡 Nasıl Çalışır?

### **Backend (C++)**
- **HTTP Server:** Windows Sockets ile sıfırdan yazılmış
- **Multi-threading:** Her istek ayrı thread'de işlenir
- **Thread Safety:** Mutex kilitleri ile güvenli eşzamanlı erişim
- **In-Memory Storage:** Veriler RAM'de saklanır (yeniden başlatmada kaybolur)
- **Hafif JSON:** Özel JSON parser/builder

### **Frontend (Web)**
- **Vanilla JavaScript:** Framework yok, saf JS
- **Real-time Updates:** setInterval ile otomatik yenileme
- **WebRTC:** Tarayıcı API'leri ile ekran kaydı
- **Responsive:** Mobil ve masaüstü uyumlu

### **Ekran Kaydı Mekanizması**
1. Frontend: `navigator.mediaDevices.getDisplayMedia()` ile ekran yakalar
2. `MediaRecorder` API ile video kaydı başlar
3. Backend: Kayıt durumunu takip eder
4. Kayıt chunks'ları browser'da toplanır
5. Durdurulduğunda: `showSaveFilePicker()` ile kayıt yeri seçilir
6. Video WebM formatında kaydedilir

---

## 📊 Performans & Özellikler

| Özellik | Detay |
|---------|-------|
| **Boyut** | ~500KB tek executable |
| **Bağımlılık** | Sıfır (sadece Windows Sockets) |
| **Port** | 8080 |
| **Protokol** | HTTP/1.1 |
| **Thread Model** | Thread-per-request |
| **CORS** | Tüm originler için aktif |
| **Video Format** | WebM (VP9 codec) |
| **Max Kalite** | 1080p @ 30fps |

---

## 🐛 Sorun Giderme

### **Sunucu Başlamıyor**
```powershell
# Çalışan sunucuyu durdur
Get-Process media_server -ErrorAction SilentlyContinue | Stop-Process -Force

# Tekrar başlat
.\media_server.exe
```

### **Port 8080 Kullanımda**
```powershell
# Port kullanan programı bul
netstat -ano | findstr :8080

# Process ID'yi not et ve durdur
Stop-Process -Id <PID> -Force
```

### **Ekran Kaydı Çalışmıyor**
- **Tarayıcı:** Chrome veya Edge kullanın
- **HTTPS:** Localhost'ta HTTP yeterli
- **İzinler:** Ekran paylaşımı iznini verin
- **Kayıt Yeri:** File System Access API desteklenmelidir

### **Derleme Hatası**
```powershell
# Derleyici yolunu kontrol et
Test-Path "C:\TDM-GCC-64\bin\g++.exe"

# Yoksa TDM-GCC'yi tekrar yükle
# https://jmeubank.github.io/tdm-gcc/
```

Daha fazla sorun giderme için: **[BASLATMA_REHBERI.md](BASLATMA_REHBERI.md#sorun-giderme)**

---

## 🚧 Gelecek Özellikler

- [ ] MongoDB entegrasyonu (kodlanacak)
- [ ] WebSocket desteği (gerçek zamanlı push)
- [ ] Kullanıcı kimlik doğrulama
- [ ] Dosya yükleme/indirme
- [ ] Rate limiting
- [ ] HTTPS desteği
- [ ] Kayıt dosyalarını sunucuda saklama
- [ ] Çoklu dil desteği

---

## 📄 Lisans

MIT License - Bu kodu istediğiniz amaçla kullanabilirsiniz.

---

## 🎉 Özellikler Özeti

✅ **Mesajlaşma** - Gerçek zamanlı chat  
✅ **Sesli Arama** - WebRTC sinyal desteği  
✅ **Görüntülü Arama** - ICE ve SDP değişimi  
✅ **Canlı Yayın** - Çoklu izleyici  
✅ **Ekran Kaydı** - 1080p, kayıt yeri seçimi ⭐  
✅ **Web Arayüzü** - Modern ve responsive  
📝 **MongoDB** - Dokümante edildi (kod entegrasyonu bekliyor)

---

**Son Güncelleme:** 2025-10-21  
**Versiyon:** 2.0 - Ekran Kaydı Edition
