# 🚀 Media Server API v2.0

Tüm özellikli medya sunucusu - Chat, Sesli Görüşme, Görüntülü Görüşme ve Canlı Yayın desteği.

## ✨ Özellikler

- ✅ **Chat (Sohbet)** - Gerçek zamanlı mesajlaşma
- ✅ **Voice Calls (Sesli Görüşme)** - WebRTC tabanlı sesli arama
- ✅ **Video Calls (Görüntülü Görüşme)** - WebRTC tabanlı video arama
- ✅ **Live Streaming (Canlı Yayın)** - Canlı yayın yönetimi
- ✅ **Thread-Safe** - Güvenli eşzamanlı işlem
- ✅ **RESTful API** - Standart HTTP metodları
- ✅ **Zero Dependencies** - Sadece C++17 ve Windows Sockets

## 🏗️ Kurulum ve Çalıştırma

### 1️⃣ Build (Derleme)
```powershell
.\build_media.ps1
```

### 2️⃣ Sunucuyu Başlat
```powershell
.\media_server.exe
```

Sunucu `http://localhost:8080` adresinde çalışacak

### 3️⃣ Test Et
```powershell
.\test_media.ps1
```

## 📡 API Endpoints

### 🏥 Health Check
```
GET /api/health
```

Sunucu durumunu ve istatistikleri döner.

**Örnek Response:**
```json
{
  "status": "healthy",
  "service": "Media Server",
  "timestamp": "2025-10-22 04:30:00",
  "stats": {
    "messages": 10,
    "activeCalls": 2,
    "liveStreams": 1
  }
}
```

---

## 💬 Chat API (Sohbet)

### Mesaj Gönder
```
POST /api/chat/messages
```

**Request Body:**
```json
{
  "username": "alice",
  "content": "Merhaba!",
  "roomId": "room1"
}
```

**Response:**
```json
{
  "id": "msg_1729564800123456",
  "username": "alice",
  "content": "Merhaba!",
  "roomId": "room1",
  "timestamp": "2025-10-22 04:30:00"
}
```

### Tüm Mesajları Getir
```
GET /api/chat/messages
```

**Response:**
```json
[
  {
    "id": "msg_1729564800123456",
    "username": "alice",
    "content": "Merhaba!",
    "roomId": "room1",
    "timestamp": "2025-10-22 04:30:00"
  }
]
```

### PowerShell Örneği
```powershell
# Mesaj gönder
$msg = @{
    username = "alice"
    content = "Merhaba dünya!"
    roomId = "genel"
} | ConvertTo-Json

Invoke-RestMethod -Uri "http://localhost:8080/api/chat/messages" `
    -Method Post `
    -ContentType "application/json" `
    -Body $msg

# Mesajları oku
Invoke-RestMethod -Uri "http://localhost:8080/api/chat/messages"
```

---

## 🎤 Voice Call API (Sesli Görüşme)

### Sesli Arama Başlat
```
POST /api/voice/call
```

**Request Body:**
```json
{
  "callerId": "user_alice",
  "callerName": "Alice",
  "receiverId": "user_bob",
  "receiverName": "Bob"
}
```

**Response:**
```json
{
  "id": "voice_1729564800123456",
  "type": "voice",
  "callerId": "user_alice",
  "callerName": "Alice",
  "receiverId": "user_bob",
  "receiverName": "Bob",
  "status": "ringing",
  "startTime": "2025-10-22 04:30:00",
  "endTime": ""
}
```

### Aramayı Yanıtla
```
POST /api/voice/answer/:callId
```

**Request Body:**
```json
{
  "answer": "SDP_ANSWER_DATA"
}
```

### Aramayı Sonlandır
```
POST /api/voice/end/:callId
```

### Aktif Aramaları Listele
```
GET /api/voice/active
```

### PowerShell Örneği
```powershell
# Arama başlat
$call = @{
    callerId = "user_alice"
    callerName = "Alice"
    receiverId = "user_bob"
    receiverName = "Bob"
} | ConvertTo-Json

$response = Invoke-RestMethod -Uri "http://localhost:8080/api/voice/call" `
    -Method Post `
    -ContentType "application/json" `
    -Body $call

$callId = $response.id

# Aramayı yanıtla
$answer = @{ answer = "SDP_DATA" } | ConvertTo-Json
Invoke-RestMethod -Uri "http://localhost:8080/api/voice/answer/$callId" `
    -Method Post `
    -ContentType "application/json" `
    -Body $answer

# Aramayı bitir
Invoke-RestMethod -Uri "http://localhost:8080/api/voice/end/$callId" `
    -Method Post `
    -ContentType "application/json" `
    -Body "{}"
```

---

## 📹 Video Call API (Görüntülü Görüşme)

### Video Arama Başlat
```
POST /api/video/call
```

**Request Body:**
```json
{
  "callerId": "user_alice",
  "callerName": "Alice",
  "receiverId": "user_bob",
  "receiverName": "Bob",
  "offer": "WEBRTC_SDP_OFFER"
}
```

### Video Aramayı Yanıtla
```
POST /api/video/answer/:callId
```

**Request Body:**
```json
{
  "answer": "WEBRTC_SDP_ANSWER"
}
```

### ICE Candidate Ekle
```
POST /api/video/ice/:callId
```

**Request Body:**
```json
{
  "candidate": "ICE_CANDIDATE_DATA"
}
```

### Video Aramayı Sonlandır
```
POST /api/video/end/:callId
```

### Aktif Video Aramalarını Listele
```
GET /api/video/active
```

### PowerShell Örneği
```powershell
# Video arama başlat
$call = @{
    callerId = "user_alice"
    callerName = "Alice"
    receiverId = "user_bob"
    receiverName = "Bob"
    offer = "WEBRTC_OFFER_SDP"
} | ConvertTo-Json

$response = Invoke-RestMethod -Uri "http://localhost:8080/api/video/call" `
    -Method Post `
    -ContentType "application/json" `
    -Body $call

$callId = $response.id

# ICE candidate ekle
$ice = @{ candidate = "ICE_DATA" } | ConvertTo-Json
Invoke-RestMethod -Uri "http://localhost:8080/api/video/ice/$callId" `
    -Method Post `
    -ContentType "application/json" `
    -Body $ice

# Aramayı yanıtla
$answer = @{ answer = "WEBRTC_ANSWER_SDP" } | ConvertTo-Json
Invoke-RestMethod -Uri "http://localhost:8080/api/video/answer/$callId" `
    -Method Post `
    -ContentType "application/json" `
    -Body $answer
```

---

## 📺 Live Streaming API (Canlı Yayın)

### Canlı Yayın Başlat
```
POST /api/stream/start
```

**Request Body:**
```json
{
  "streamerId": "user_alice",
  "streamerName": "Alice",
  "title": "Oyun Yayını",
  "description": "Minecraft oynuyorum!"
}
```

**Response:**
```json
{
  "id": "stream_1729564800123456",
  "streamerId": "user_alice",
  "streamerName": "Alice",
  "title": "Oyun Yayını",
  "description": "Minecraft oynuyorum!",
  "status": "live",
  "startTime": "2025-10-22 04:30:00",
  "endTime": "",
  "viewerCount": 0,
  "streamKey": "key_1729564800789012"
}
```

### Canlı Yayınları Listele
```
GET /api/stream/live
```

### Yayına Katıl (İzleyici)
```
POST /api/stream/join/:streamId
```

**Request Body:**
```json
{
  "viewerId": "user_bob"
}
```

### Yayından Ayrıl
```
POST /api/stream/leave/:streamId
```

**Request Body:**
```json
{
  "viewerId": "user_bob"
}
```

### Yayını Sonlandır
```
POST /api/stream/end/:streamId
```

### Yayın Detaylarını Getir
```
GET /api/stream/:streamId
```

### PowerShell Örneği
```powershell
# Canlı yayın başlat
$stream = @{
    streamerId = "user_alice"
    streamerName = "Alice"
    title = "Canlı Kodlama"
    description = "C++ ile game development"
} | ConvertTo-Json

$response = Invoke-RestMethod -Uri "http://localhost:8080/api/stream/start" `
    -Method Post `
    -ContentType "application/json" `
    -Body $stream

$streamId = $response.id
Write-Host "Stream Key: $($response.streamKey)"

# Yayını izlemeye başla
$viewer = @{ viewerId = "user_bob" } | ConvertTo-Json
Invoke-RestMethod -Uri "http://localhost:8080/api/stream/join/$streamId" `
    -Method Post `
    -ContentType "application/json" `
    -Body $viewer

# Canlı yayınları listele
$liveStreams = Invoke-RestMethod -Uri "http://localhost:8080/api/stream/live"
$liveStreams | Format-Table

# Yayını bitir
Invoke-RestMethod -Uri "http://localhost:8080/api/stream/end/$streamId" `
    -Method Post `
    -ContentType "application/json" `
    -Body "{}"
```

---

## 🔧 Teknik Detaylar

### Mimari
- **Dil:** C++17
- **Ağ:** Windows Sockets (cross-platform uyumlu)
- **Concurrency:** Thread-per-request modeli
- **Veri Depolama:** In-memory (restart sonrası kaybolur)
- **WebRTC:** Signaling server (medya akışı client-side)

### WebRTC Entegrasyonu

Bu sunucu bir **WebRTC Signaling Server** görevi görür. Gerçek medya akışı (ses/video) 
peer-to-peer yapılır, sunucu sadece:

1. **SDP (Session Description Protocol)** alışverişini yönetir
2. **ICE (Interactive Connectivity Establishment)** candidate'lerini paylaşır
3. **Call state'i** (ringing, active, ended) takip eder

### Örnek WebRTC Client Akışı

```javascript
// 1. Arama başlat
const pc = new RTCPeerConnection(config);

// 2. Offer oluştur
const offer = await pc.createOffer();
await pc.setLocalDescription(offer);

// 3. Offer'ı sunucuya gönder
const response = await fetch('/api/video/call', {
  method: 'POST',
  body: JSON.stringify({
    callerId: 'alice',
    callerName: 'Alice',
    receiverId: 'bob',
    receiverName: 'Bob',
    offer: offer.sdp
  })
});

// 4. ICE candidate'leri topla ve gönder
pc.onicecandidate = (event) => {
  if (event.candidate) {
    fetch(`/api/video/ice/${callId}`, {
      method: 'POST',
      body: JSON.stringify({ candidate: event.candidate })
    });
  }
};
```

---

## 📊 Veri Yapıları

### Message
```cpp
{
  id: string,         // Benzersiz mesaj ID
  username: string,   // Gönderen kullanıcı adı
  content: string,    // Mesaj içeriği
  roomId: string,     // Oda ID (opsiyonel)
  timestamp: string   // Gönderim zamanı
}
```

### CallSession
```cpp
{
  id: string,           // Benzersiz arama ID
  type: string,         // "voice" veya "video"
  callerId: string,     // Arayan kullanıcı ID
  callerName: string,   // Arayan kullanıcı adı
  receiverId: string,   // Aranan kullanıcı ID
  receiverName: string, // Aranan kullanıcı adı
  status: string,       // "ringing", "active", "ended"
  startTime: string,    // Başlangıç zamanı
  endTime: string,      // Bitiş zamanı
  offer: string,        // WebRTC SDP offer
  answer: string        // WebRTC SDP answer
}
```

### LiveStream
```cpp
{
  id: string,           // Benzersiz yayın ID
  streamerId: string,   // Yayıncı kullanıcı ID
  streamerName: string, // Yayıncı kullanıcı adı
  title: string,        // Yayın başlığı
  description: string,  // Yayın açıklaması
  status: string,       // "live" veya "ended"
  startTime: string,    // Başlangıç zamanı
  endTime: string,      // Bitiş zamanı
  viewerCount: number,  // İzleyici sayısı
  streamKey: string     // Yayın anahtarı (OBS vb. için)
}
```

---

## 🎯 Kullanım Senaryoları

### Senaryo 1: Chat Uygulaması
```powershell
# Alice mesaj gönderir
$msg = @{username="alice"; content="Merhaba!"} | ConvertTo-Json
Invoke-RestMethod -Uri "http://localhost:8080/api/chat/messages" -Method Post -Body $msg -ContentType "application/json"

# Bob mesajları okur
$messages = Invoke-RestMethod -Uri "http://localhost:8080/api/chat/messages"
```

### Senaryo 2: Video Arama
```powershell
# Alice Bob'u arar
$call = @{
    callerId="alice"; callerName="Alice"
    receiverId="bob"; receiverName="Bob"
    offer="SDP_OFFER"
} | ConvertTo-Json

$response = Invoke-RestMethod -Uri "http://localhost:8080/api/video/call" -Method Post -Body $call -ContentType "application/json"

# Bob aramayı yanıtlar
$answer = @{answer="SDP_ANSWER"} | ConvertTo-Json
Invoke-RestMethod -Uri "http://localhost:8080/api/video/answer/$($response.id)" -Method Post -Body $answer -ContentType "application/json"
```

### Senaryo 3: Canlı Yayın
```powershell
# Alice yayın başlatır
$stream = @{
    streamerId="alice"; streamerName="Alice"
    title="Kodlama Yayını"; description="C++ öğreniyoruz"
} | ConvertTo-Json

$s = Invoke-RestMethod -Uri "http://localhost:8080/api/stream/start" -Method Post -Body $stream -ContentType "application/json"

# Bob yayını izler
$viewer = @{viewerId="bob"} | ConvertTo-Json
Invoke-RestMethod -Uri "http://localhost:8080/api/stream/join/$($s.id)" -Method Post -Body $viewer -ContentType "application/json"

# Canlı yayınları listele
Invoke-RestMethod -Uri "http://localhost:8080/api/stream/live"
```

---

## 🔒 Güvenlik Notları

⚠️ **Bu bir demo/development sunucusudur.** Production kullanımı için:

- ✅ Authentication (JWT, OAuth) ekleyin
- ✅ HTTPS kullanın
- ✅ Rate limiting uygulayın
- ✅ Input validation yapın
- ✅ CORS policy düzenleyin
- ✅ Database entegrasyonu ekleyin (MongoDB, PostgreSQL)
- ✅ Logging sistemi kurun
- ✅ Error handling geliştirin

---

## 🚀 Gelecek Özellikler

- [ ] MongoDB entegrasyonu (kalıcı veri depolama)
- [ ] WebSocket desteği (gerçek zamanlı bildirimler)
- [ ] Kullanıcı kimlik doğrulama
- [ ] Dosya paylaşımı
- [ ] Ekran paylaşımı desteği
- [ ] Kayıt/Replay özelliği
- [ ] Analytics ve raporlama
- [ ] Admin panel

---

## 📚 Ek Kaynaklar

- [WebRTC Dökümanları](https://webrtc.org/)
- [REST API Best Practices](https://restfulapi.net/)
- [C++ Concurrency](https://en.cppreference.com/w/cpp/thread)

---

## 📄 Lisans

MIT License - İstediğiniz gibi kullanabilirsiniz.

---

## 🤝 Katkıda Bulunma

Pull request'ler kabul edilir! Büyük değişiklikler için önce issue açın.

---

**Başarılı Kodlamalar! 🎉**



