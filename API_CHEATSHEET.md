# 📋 API Hızlı Referans Kılavuzu

## 🏥 Health Check
```bash
GET /api/health
```

---

## 💬 CHAT API

| Endpoint | Method | Açıklama |
|----------|--------|----------|
| `/api/chat/messages` | GET | Tüm mesajları listele |
| `/api/chat/messages` | POST | Yeni mesaj gönder |

### Chat Örnekleri
```powershell
# Mesaj gönder
$msg = @{username="ali"; content="Selam"; roomId="genel"} | ConvertTo-Json
Invoke-RestMethod -Uri "http://localhost:8080/api/chat/messages" -Method Post -Body $msg -ContentType "application/json"

# Mesajları oku
Invoke-RestMethod -Uri "http://localhost:8080/api/chat/messages"
```

---

## 🎤 VOICE CALL API

| Endpoint | Method | Açıklama |
|----------|--------|----------|
| `/api/voice/call` | POST | Arama başlat |
| `/api/voice/answer/:callId` | POST | Aramayı yanıtla |
| `/api/voice/end/:callId` | POST | Aramayı bitir |
| `/api/voice/active` | GET | Aktif aramaları listele |

### Voice Call Örnekleri
```powershell
# Arama başlat
$call = @{callerId="u1"; callerName="Ali"; receiverId="u2"; receiverName="Veli"} | ConvertTo-Json
$r = Invoke-RestMethod -Uri "http://localhost:8080/api/voice/call" -Method Post -Body $call -ContentType "application/json"

# Yanıtla
$ans = @{answer="SDP"} | ConvertTo-Json
Invoke-RestMethod -Uri "http://localhost:8080/api/voice/answer/$($r.id)" -Method Post -Body $ans -ContentType "application/json"

# Bitir
Invoke-RestMethod -Uri "http://localhost:8080/api/voice/end/$($r.id)" -Method Post -Body "{}" -ContentType "application/json"
```

---

## 📹 VIDEO CALL API

| Endpoint | Method | Açıklama |
|----------|--------|----------|
| `/api/video/call` | POST | Video arama başlat |
| `/api/video/answer/:callId` | POST | Aramayı yanıtla |
| `/api/video/ice/:callId` | POST | ICE candidate ekle |
| `/api/video/end/:callId` | POST | Aramayı bitir |
| `/api/video/active` | GET | Aktif video aramalarını listele |

### Video Call Örnekleri
```powershell
# Video arama
$v = @{callerId="u1"; callerName="Ali"; receiverId="u2"; receiverName="Veli"; offer="SDP"} | ConvertTo-Json
$r = Invoke-RestMethod -Uri "http://localhost:8080/api/video/call" -Method Post -Body $v -ContentType "application/json"

# ICE ekle
$ice = @{candidate="ICE_DATA"} | ConvertTo-Json
Invoke-RestMethod -Uri "http://localhost:8080/api/video/ice/$($r.id)" -Method Post -Body $ice -ContentType "application/json"
```

---

## 📺 LIVE STREAMING API

| Endpoint | Method | Açıklama |
|----------|--------|----------|
| `/api/stream/start` | POST | Yayın başlat |
| `/api/stream/live` | GET | Canlı yayınları listele |
| `/api/stream/join/:streamId` | POST | Yayına katıl (izleyici) |
| `/api/stream/leave/:streamId` | POST | Yayından ayrıl |
| `/api/stream/end/:streamId` | POST | Yayını bitir |
| `/api/stream/:streamId` | GET | Yayın detayları |

### Live Streaming Örnekleri
```powershell
# Yayın başlat
$s = @{streamerId="u1"; streamerName="Ali"; title="Oyun"; description="Fortnite"} | ConvertTo-Json
$r = Invoke-RestMethod -Uri "http://localhost:8080/api/stream/start" -Method Post -Body $s -ContentType "application/json"

# İzle
$v = @{viewerId="u2"} | ConvertTo-Json
Invoke-RestMethod -Uri "http://localhost:8080/api/stream/join/$($r.id)" -Method Post -Body $v -ContentType "application/json"

# Canlı yayınlar
Invoke-RestMethod -Uri "http://localhost:8080/api/stream/live"
```

---

## 📦 Request Body Şablonları

### Chat Message
```json
{
  "username": "kullanici_adi",
  "content": "mesaj_icerigi",
  "roomId": "oda_id"
}
```

### Voice Call
```json
{
  "callerId": "arayan_id",
  "callerName": "arayan_ad",
  "receiverId": "aranan_id",
  "receiverName": "aranan_ad"
}
```

### Video Call
```json
{
  "callerId": "arayan_id",
  "callerName": "arayan_ad",
  "receiverId": "aranan_id",
  "receiverName": "aranan_ad",
  "offer": "webrtc_sdp_offer"
}
```

### Live Stream
```json
{
  "streamerId": "yayinci_id",
  "streamerName": "yayinci_ad",
  "title": "yayin_basligi",
  "description": "yayin_aciklamasi"
}
```

---

## 🔑 Response Status Codes

| Code | Anlamı |
|------|--------|
| 200 | OK - Başarılı |
| 201 | Created - Oluşturuldu |
| 400 | Bad Request - Hatalı istek |
| 404 | Not Found - Bulunamadı |
| 500 | Internal Server Error - Sunucu hatası |

---

## 🛠️ Faydalı Komutlar

```powershell
# Sunucuyu başlat
.\media_server.exe

# Sunucuyu durdur
Get-Process media_server | Stop-Process

# Testleri çalıştır
.\test_media.ps1

# Health check
Invoke-RestMethod http://localhost:8080/api/health

# Tüm endpoint'leri gör
Invoke-RestMethod http://localhost:8080
```

---

## 📱 JavaScript Fetch Örnekleri

```javascript
// Mesaj gönder
fetch('http://localhost:8080/api/chat/messages', {
  method: 'POST',
  headers: {'Content-Type': 'application/json'},
  body: JSON.stringify({
    username: 'ali',
    content: 'Merhaba!',
    roomId: 'genel'
  })
}).then(r => r.json()).then(console.log);

// Video arama başlat
fetch('http://localhost:8080/api/video/call', {
  method: 'POST',
  headers: {'Content-Type': 'application/json'},
  body: JSON.stringify({
    callerId: 'user1',
    callerName: 'Ali',
    receiverId: 'user2',
    receiverName: 'Veli',
    offer: 'SDP_OFFER'
  })
}).then(r => r.json()).then(console.log);

// Canlı yayınları listele
fetch('http://localhost:8080/api/stream/live')
  .then(r => r.json())
  .then(streams => {
    streams.forEach(s => {
      console.log(`${s.title} - ${s.viewerCount} izleyici`);
    });
  });
```

---

## 🎯 Hızlı Test Senaryosu

```powershell
# 1. Health check
Invoke-RestMethod http://localhost:8080/api/health

# 2. Mesaj gönder
$m = @{username="test"; content="test"} | ConvertTo-Json
Invoke-RestMethod http://localhost:8080/api/chat/messages -Method Post -Body $m -ContentType "application/json"

# 3. Mesajları oku
Invoke-RestMethod http://localhost:8080/api/chat/messages

# 4. Yayın başlat
$s = @{streamerId="s1"; streamerName="Streamer"; title="Test"; description="Test"} | ConvertTo-Json
$stream = Invoke-RestMethod http://localhost:8080/api/stream/start -Method Post -Body $s -ContentType "application/json"

# 5. Yayını izle
Write-Host "Stream ID: $($stream.id)"
Write-Host "Stream Key: $($stream.streamKey)"
```

---

**Bu kılavuzu yazdır ve masanda tut! 📌**



