# Proje Gagabunto Media - Gelistirme Durumu

## Durum Ozeti
- [+] Proje mimarisi incelendi ve mevcut durum cikarildi.
- [+] Backendin tek process C++ REST API oldugu dogrulandi.
- [+] State'in memory tabanli tutuldugu dogrulandi.
- [+] WebRTC kisminin su an yalnizca signaling oldugu dogrulandi.
- [+] UI tarafinda polling kullanildigi dogrulandi.
- [+] Launcher'in backend exe acip health check ile UI yukledigi dogrulandi.
- [+] JSON parsing'in manuel oldugu ve kirilgan riski not edildi.
- [+] Backend'e /ws websocket endpointi, handshake ve event broadcast eklendi
- [+] Frontend app.js websocket client + otomatik reconnect + polling fallback eklendi
- [+] Room bazli chat UI eklendi (oda secimi, mesaj gonderme, canli mesaj listesi)
- [+] Backend /api/chat/messages endpointine roomId query filtreleme eklendi
- [+] WebSocket token dogrulamasi eklendi (MEDIA_WS_TOKEN / query token veya X-WS-Token)
- [+] WebSocket IP bazli rate-limit eklendi (MEDIA_WS_RATE_MAX_ATTEMPTS, MEDIA_WS_RATE_WINDOW_SEC)
- [+] GIGACHAD_WS_TOKEN ve GIGACHAD_WS_RATE_* env aliaslari eklendi (MEDIA_* ile geriye uyumlu)
- [+] Frontend apiFetch wrapper eklendi; tum /api istekleri otomatik token gonderiyor
- [+] HTTP API auth middleware eklendi (GIGACHAD_API_TOKEN / MEDIA_API_TOKEN, Bearer ve X-API-Token)
- [+] Role-based yetkilendirme eklendi (user/moderator/admin token modeli)
- [+] Moderator gerektiren endpoint politikasi eklendi (stream end, recording stop/list/detail)
- [+] Kalici veri phase-1 eklendi: data/*.jsonl disk snapshot + startup restore
- [+] SQLite phase-1 eklendi: messages icin runtime sqlite3.dll adapter + JSONL fallback/migration
- [+] Calls/streams/recordings SQLite persistence eklendi + startup restore + JSONL fallback/migration
- [+] Storage mode toggle eklendi: GIGACHAD_STORAGE_MODE=hybrid|sqlite_only|jsonl_only
- [+] Event bus dispatcher abstraction eklendi: GIGACHAD_EVENT_BUS/MEDIA_EVENT_BUS (memory + redis fallback hook)
- [+] Redis event bus pub/sub eklendi (GIGACHAD_EVENT_BUS=redis, channel+host+port+password env destegi)
- [+] Event bus mode parse icin trim eklendi (\"redis \" gibi whitespace'li env degeri artik dogru calisiyor)
## Mimari Hedef (Onceliklendirilmis)
- [+] Realtime Core: REST polling yerine WebSocket signaling (`/ws`) eklendi.
- [+] Realtime Core: Temel room/user presence eventleri eklendi.
- [+] Realtime Core: Event bus Redis transport (gercek pub/sub) entegrasyonu.
- [+] Realtime Core: Multi-instance Redis event bus test/otomasyon senaryosu (PowerShell) eklendi.
- [+] Realtime Core: Multi-instance test CI pipeline'a baglandi (GitHub Actions + Redis service).
- [-] Realtime Core: Auth temelinin (passkey/JWT) kurulmasi.

- [-] Media Plane: 2-10 kisi icin P2P WebRTC akisi.
- [-] Media Plane: 10+ icin SFU fallback (LiveKit veya mediasoup).
- [-] Media Plane: Oda boyutuna gore otomatik route/policy engine.

- [-] Federation: Matrix'in control-plane olarak baglanmasi.
- [-] Federation: Room/identity/bridge adapter katmaninin eklenmesi.
- [-] Federation: Mevcut API'nin Matrix event adapter ile konusmasi.

- [-] Offline Mesh: Mobilde Nearby/BLE/Wi-Fi Direct fallback.
- [-] Offline Mesh: Party mode'un feature-flag ile acilmasi.

## Sonraki Net Adim
- [+] Ilk implementasyon: `media_server.cpp` icine minimal `/ws` signaling katmani.
- [+] Ilk implementasyon: `public/app.js` tarafinda polling'den socket event modeline gecis.
- [+] WebSocket eventlerini room bazli filtreleyip chat UI'da canli listeleme eklendi
- [+] Role-based yetkilendirme (user/moderator/admin) eklendi
- [+] Kalici veri katmani phase-1 tamamlandi (disk JSONL snapshot + restore)
- [+] JSONL fallback'i opsiyonel moda cekildi (tam SQLite mode toggle)
- [+] Calls/streams/recordings persistence'i de SQLite tablolarina tasındı
- [+] /api/health cikisina eventBusMode alani eklendi
- [+] /api/health cikisina eventBusRedisReady alani eklendi
- [+] test_event_bus_multi_instance.ps1 eklendi (2 instance + Redis + WS event propagation testi)
- [+] Multi-instance Redis testi localde dogrulandi (Docker Redis + script PASS)
- [+] .github/workflows/media-ci.yml eklendi (build + 2 instance + Redis + websocket propagation testi)



