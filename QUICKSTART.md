# ⚡ Hızlı Başlangıç

## 🚀 3 Saniyede Başlat

### Yöntem 1: BAT Dosyası (EN KOLAY) ⭐
```
start_server.bat dosyasına çift tıkla
```
✅ Otomatik başlar ve tarayıcıyı açar!

---

### Yöntem 2: Manuel (Terminal)
```powershell
cd C:\Users\thor\Desktop\rela
.\media_server.exe
```
Sonra tarayıcıda: `http://localhost:8080`

---

## 📌 Önemli Bilgiler

**Port:** 8080  
**Adres:** http://localhost:8080  
**Durdurmak:** Ctrl+C

---

## ✨ Özellikler

| Özellik | Kısayol |
|---------|---------|
| 💬 Mesajlaşma | Ana sayfa → Mesajlar |
| 🎤 Sesli Arama | Ana sayfa → Sesli Arama |
| 📹 Video Arama | Ana sayfa → Görüntülü Arama |
| 📺 Canlı Yayın | Ana sayfa → Canlı Yayın |
| 🎬 **Ekran Kaydı** | Ana sayfa → **Ekran Kaydı** ⭐ |

---

## 🎬 Ekran Kaydı Nasıl Kullanılır?

1. Ana sayfada "**Ekran Kaydı**" sekmesine tıkla
2. Kalite seç (1080p/720p/480p)
3. "**Kaydı Başlat**" → Ekranı seç
4. "**Kaydı Durdur**" → Kayıt yerini seç
5. ✅ Video kaydedildi!

**İki Şekilde Durdurabilirsin:**
- Web sitesindeki "Kaydı Durdur" butonu
- Tarayıcı bildirimindeki "Paylaşımı Durdur" butonu

---

## ❓ Sorun mu var?

**Sunucu başlamıyor?**
```powershell
Get-Process media_server -ErrorAction SilentlyContinue | Stop-Process -Force
.\media_server.exe
```

**Sayfa açılmıyor?**
- Sunucu çalışıyor mu kontrol et
- Port 8080 başka program tarafından kullanılıyor olabilir

**Detaylı yardım:**
→ `BASLATMA_REHBERI.md` dosyasını oku

---

**Hazır!** 🎉
