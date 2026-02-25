# 🌐 Web Arayüzü - Media Server Dashboard

Modern ve kullanıcı dostu web arayüzü ile Media Server API'nizin tüm özelliklerini kullanın!

## ✨ Özellikler

### 💬 **Sohbet (Chat)**
- Gerçek zamanlı mesajlaşma
- Mesaj geçmişini görüntüleme
- Kullanıcı adı desteği
- Otomatik yenileme (5 saniyede bir)

### 🎤 **Sesli Arama (Voice Calls)**
- Sesli arama başlatma
- Aktif aramaları görüntüleme
- Arama durumu takibi (Çalıyor, Aktif, Bitti)
- Aramayı sonlandırma

### 📹 **Görüntülü Arama (Video Calls)**
- Video arama başlatma
- WebRTC signaling desteği
- ICE candidate yönetimi
- Aktif video aramalarını listeleme

### 📺 **Canlı Yayın (Live Streaming)**
- Canlı yayın başlatma
- Yayın başlığı ve açıklama
- İzleyici sayısı takibi
- Canlı yayınları görüntüleme
- Yayına katılma/izleme

### 📊 **İstatistikler**
- Sunucu durumu
- Toplam mesaj sayısı
- Aktif arama sayıları
- Canlı yayın sayısı
- Gerçek zamanlı güncelleme

## 🚀 Kullanım

### 1. Sunucuyu Başlatın
```powershell
.\media_server.exe
```

### 2. Tarayıcınızı Açın
```
http://localhost:8080
```

### 3. Kullanmaya Başlayın!
Web arayüzü otomatik olarak açılacak ve kullanıma hazır olacak.

## 🎨 Arayüz Bileşenleri

### Header (Üst Bölüm)
- **Logo ve Başlık** - Media Server v2.0
- **Sunucu Durumu** - Çevrimiçi/Çevrimdışı göstergesi
- **Kullanıcı Adı** - Kendi kullanıcı adınızı ayarlayın

### Sidebar (Yan Menü)
- 💬 Sohbet
- 🎤 Sesli Arama  
- 📹 Görüntülü Arama
- 📺 Canlı Yayın
- 📊 İstatistikler

Her menü öğesinde aktif öğe sayısını gösteren rozet var.

### Ana İçerik Alanı
Her sekme için özel arayüz:
- Form elemanları
- Listeler ve kartlar
- Aksiyon butonları
- Gerçek zamanlı güncellemeler

## 💡 Kullanım Örnekleri

### Chat (Sohbet)
1. Kullanıcı adınızı sağ üstten girin
2. Sohbet sekmesine gidin
3. Mesajınızı yazın
4. "Gönder" butonuna tıklayın veya Enter'a basın
5. Mesajınız anında görünecek!

### Sesli Arama
1. Sesli Arama sekmesine gidin
2. Aranacak kişinin ID'sini girin (örn: user_bob)
3. Kişinin ismini girin (örn: Bob)
4. "Aramayı Başlat" butonuna tıklayın
5. Arama durumunu aktif aramalar bölümünden takip edin

### Video Arama
1. Görüntülü Arama sekmesine gidin
2. Aranacak kişi bilgilerini girin
3. "Video Aramayı Başlat" butonuna tıklayın
4. WebRTC signaling otomatik olarak başlar
5. Aramayı yönetin veya sonlandırın

### Canlı Yayın
1. Canlı Yayın sekmesine gidin
2. Yayın başlığı girin (örn: "Oyun Yayını")
3. Açıklama ekleyin
4. "Yayını Başlat" butonuna tıklayın
5. Stream key'iniz bildirim olarak gösterilecek
6. Canlı yayınlar bölümünde tüm aktif yayınları görün

## 🎯 Klavye Kısayolları

- **Enter** - Sohbette mesaj gönder
- **Ctrl + R** - Sayfayı yenile
- **F5** - Sayfayı yenile

## 🔔 Bildirimler

Tüm işlemler için gerçek zamanlı bildirimler:
- ✅ Başarılı (Yeşil)
- ❌ Hata (Kırmızı)
- ℹ️ Bilgi (Mavi)

Bildirimler 3 saniye sonra otomatik olarak kaybolur.

## 📱 Responsive Tasarım

Web arayüzü tüm cihazlarda mükemmel çalışır:
- 💻 Masaüstü
- 📱 Tablet
- 📱 Mobil

## 🎨 Tema ve Renkler

### Ana Renkler
- **Primary (Birincil)**: Mavi (#2563eb)
- **Success (Başarı)**: Yeşil (#10b981)
- **Danger (Tehlike)**: Kırmızı (#ef4444)
- **Warning (Uyarı)**: Turuncu (#f59e0b)

### Gradient Arka Plan
Modern gradient arka plan ile şık görünüm

## 🔄 Otomatik Yenileme

Aşağıdaki veriler her 5 saniyede bir otomatik yenilenir:
- Chat mesajları
- Aktif sesli aramalar
- Aktif video aramaları
- Canlı yayınlar

## 🛠️ Teknik Detaylar

### Kullanılan Teknolojiler
- **HTML5** - Yapı
- **CSS3** - Stil (Grid, Flexbox, Animations)
- **JavaScript (ES6+)** - İşlevsellik
- **Font Awesome 6** - İkonlar
- **Fetch API** - HTTP istekleri

### Dosya Yapısı
```
public/
├── index.html      - Ana HTML dosyası
├── styles.css      - Stil dosyası (~350 satır)
└── app.js          - JavaScript mantığı (~600 satır)
```

### API Entegrasyonu
Tüm API endpoint'leri otomatik olarak entegre:
```javascript
const API_BASE = 'http://localhost:8080';

// Örnek kullanım
await fetch(`${API_BASE}/api/chat/messages`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ username, content, roomId })
});
```

## 🐛 Sorun Giderme

### Arayüz Açılmıyor
```powershell
# Sunucunun çalıştığını kontrol edin
Invoke-RestMethod -Uri "http://localhost:8080/api/health"

# Port 8080'in boş olduğunu kontrol edin
Get-Process | Where-Object {$_.ProcessName -eq 'media_server'}
```

### Veriler Güncellenmiyor
- Tarayıcı konsolunu açın (F12)
- Hata mesajlarını kontrol edin
- Sayfayı yenileyin (Ctrl+R)

### CORS Hatası
- Sunucu CORS desteği içerir
- Farklı porttan erişiyorsanız sorun yaşayabilirsiniz
- Aynı domain'den erişin: `http://localhost:8080`

## 📊 Performans

- **Hafif**: Toplam ~150KB (sıkıştırılmamış)
- **Hızlı**: Anında yükleme
- **Optimize**: Minimum kaynak kullanımı
- **Responsive**: Tüm cihazlarda hızlı

## 🔐 Güvenlik Notları

⚠️ **Geliştirme/Demo Amaçlı**

Production kullanımı için:
- [ ] HTTPS ekleyin
- [ ] Kullanıcı kimlik doğrulama ekleyin
- [ ] Input sanitization yapın
- [ ] Rate limiting uygulayın
- [ ] XSS koruması ekleyin
- [ ] CSRF token kullanın

## 🎁 Bonus Özellikler

### Animasyonlar
- ✨ Fade-in animasyonları
- 🎬 Slide-in efektleri
- 💫 Hover efektleri
- 🌊 Smooth transitions

### UX İyileştirmeleri
- 📍 Sticky header (sabit başlık)
- 🎯 Auto-scroll (mesajlarda)
- 💾 Local storage (kullanıcı adı)
- 🔔 Toast notifications
- 📱 Touch-friendly buttons

## 📸 Ekran Görüntüleri

### Dashboard Ana Sayfa
Modern ve temiz arayüz, tüm özelliklere kolay erişim.

### Chat Arayüzü
WhatsApp tarzı mesajlaşma deneyimi.

### Canlı Yayınlar
Netflix tarzı kart görünümü ile yayınlar.

### İstatistikler
Renkli kartlar ile görsel istatistikler.

## 🚀 Gelecek Özellikler

- [ ] Dark mode (karanlık tema)
- [ ] Kullanıcı profilleri
- [ ] Emoji desteği
- [ ] Dosya paylaşımı
- [ ] Sesli/görsel bildiri ğimler
- [ ] Arama ve filtreleme
- [ ] Export/import özellikleri
- [ ] Ayarlar paneli

## 💬 İpuçları

1. **Kullanıcı Adı**: İlk girişte kullanıcı adınızı ayarlayın, otomatik kaydedilir
2. **Mesaj Geçmişi**: Tüm mesajlar gösterilir, yeni mesajlar en altta
3. **Rozet Sayıları**: Yan menüde aktif öğe sayıları gösterilir
4. **Bildirimler**: Tüm işlemler için bildirim alırsınız
5. **Yenileme**: Sayfa otomatik yenilenir, manual yenilemeye gerek yok

## 🎓 Öğretici

### İlk Kullanım
1. Sunucuyu başlatın: `.\media_server.exe`
2. Tarayıcıda açın: `http://localhost:8080`
3. Kullanıcı adınızı girin
4. Bir mesaj gönderin
5. Diğer sekmeleri keşfedin!

### İleri Kullanım
1. Çoklu sekmede açın (farklı kullanıcılar simüle edin)
2. Bir sekmede yayın başlatın
3. Diğer sekmede yayını izleyin
4. Chat'te iletişim kurun
5. İstatistikleri takip edin

## 📞 Destek

Sorunuz mu var? 
- 📖 `README_TR.md` dosyasını okuyun
- 🔍 `API_CHEATSHEET.md` dosyasına bakın
- 📊 `PROJE_OZETI.md` dosyasını inceleyin

## 🎉 Tebrikler!

Artık tam özellikli bir **web arayüzü** ile media server'ınızı kullanabilirsiniz!

**Keyifli kullanımlar! 🚀**

---

*Son Güncelleme: 22 Ekim 2025*
*Versiyon: 2.0.0*
*Durum: ✅ Çalışıyor*



