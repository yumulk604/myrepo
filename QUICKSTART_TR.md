# 🚀 Hızlı Başlangıç - Media Server API

## 3 Basit Adım

### 1️⃣ Derle
```powershell
.\build_media.ps1
```

### 2️⃣ Çalıştır
```powershell
.\media_server.exe
```

### 3️⃣ Test Et
Başka bir PowerShell penceresi aç:
```powershell
.\test_media.ps1
```

---

## 🎯 Hemen Dene!

### 💬 Chat Mesajı Gönder
```powershell
$msg = @{username="Adın"; content="Merhaba Dünya!"; roomId="genel"} | ConvertTo-Json
Invoke-RestMethod -Uri "http://localhost:8080/api/chat/messages" -Method Post -ContentType "application/json" -Body $msg
```

### 🎤 Sesli Arama Başlat
```powershell
$call = @{
    callerId="user1"; callerName="Ali"
    receiverId="user2"; receiverName="Ayşe"
} | ConvertTo-Json

Invoke-RestMethod -Uri "http://localhost:8080/api/voice/call" -Method Post -ContentType "application/json" -Body $call
```

### 📹 Video Arama Başlat
```powershell
$video = @{
    callerId="user1"; callerName="Ali"
    receiverId="user2"; receiverName="Ayşe"
    offer="WEBRTC_SDP_OFFER"
} | ConvertTo-Json

Invoke-RestMethod -Uri "http://localhost:8080/api/video/call" -Method Post -ContentType "application/json" -Body $video
```

### 📺 Canlı Yayın Başlat
```powershell
$stream = @{
    streamerId="user1"; streamerName="Ali"
    title="Oyun Yayını"; description="Minecraft oynuyorum"
} | ConvertTo-Json

Invoke-RestMethod -Uri "http://localhost:8080/api/stream/start" -Method Post -ContentType "application/json" -Body $stream
```

---

## 🌐 Tarayıcıda Dene

`http://localhost:8080` adresini aç - API dokümantasyonunu gör!

---

## 📊 Sunucu Durumu

```powershell
Invoke-RestMethod -Uri "http://localhost:8080/api/health"
```

---

## 🛑 Sunucuyu Durdur

Sunucu penceresinde `Ctrl+C` bas, veya:

```powershell
Get-Process | Where-Object {$_.ProcessName -eq 'media_server'} | Stop-Process -Force
```

---

## 📚 Daha Fazlası

- 🇹🇷 Türkçe Dokümantasyon: `README_TR.md`
- 🇬🇧 English Documentation: `README.md`
- 🍃 MongoDB Entegrasyonu: `MONGODB_INTEGRATION.md`
- 📖 API Örnekleri: `API_EXAMPLES.md` (İngilizce)

---

## 🎉 Hazırsın!

Artık tam özellikli bir medya sunucun var:
- ✅ Chat (Sohbet)
- ✅ Sesli Görüşme
- ✅ Görüntülü Görüşme
- ✅ Canlı Yayın

**Başarılar! 🚀**



