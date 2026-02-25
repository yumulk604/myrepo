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
- [+] Realtime Core: Multi-tenant instance Redis event bus test/otomasyon eklendi (tenant izolasyonu + tenant delivery).
- [+] Realtime Core: Multi-tenant instance test CI pipeline'a baglandi.
- [+] Realtime Core: JWT auth temeli kuruldu (login/refresh/logout + access/refresh token + revoke).
- [+] Realtime Core: JWT key rotation (kid + coklu secret dogrulama) eklendi.
- [+] Realtime Core: Refresh/access revoke kaydi SQLite'a kalici yaziliyor (restart sonrasi da gecerli).
- [+] Realtime Core: Passkey onboarding + JWT ile birlesik auth akisi (phase-1 minimal challenge/register/login).
- [+] Realtime Core: Passkey strict metadata kontrolleri eklendi (rpId/origin/clientDataType + allowlist policy).
- [+] Realtime Core: Passkey challenge bagli clientDataJSON ve authenticatorData (rpIdHash + UP flag + signCount) kontrolu eklendi.
- [+] Realtime Core: Passkey login icin credential-bound HMAC signature verification eklendi (strict mode).
- [+] Realtime Core: Passkey counter rollback/replay korumasi eklendi (GIGACHAD_PASSKEY_COUNTER_STRICT).
- [+] Realtime Core: Passkey credential persistence eklendi (SQLite + disk fallback, restart sonrasi login devam).
- [-] Realtime Core: Passkey tam WebAuthn COSE public-key (ECDSA/EdDSA) signature verification (production hardening).

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
- [+] Multi-tenant chat izolasyonu eklendi (tenantId: API + WS + event frame + Redis subscriber filtreleme)
- [+] test_event_bus_multi_tenant.ps1 eklendi ve localde PASS alindi
- [+] JWT claim tabanli role/tenant enforcement eklendi (HTTP API + WebSocket)
- [+] /api/auth/login, /api/auth/refresh, /api/auth/logout endpointleri eklendi
- [+] test_auth.ps1 eklendi ve localde PASS alindi (login/refresh/logout + tenant claim enforcement)
- [+] test_ws_jwt_tenant.ps1 eklendi ve localde PASS alindi (WS no-token block + tenant override block)
- [+] test_auth_revocation_persistence.ps1 eklendi ve localde PASS alindi
- [+] CI pipeline'a JWT auth + WS tenant negative test adimi eklendi
- [+] /api/auth/passkey/challenge, /api/auth/passkey/register, /api/auth/passkey/login endpointleri eklendi (phase-1)
- [+] test_auth_passkey.ps1 eklendi (challenge reuse block + JWT tenant enforcement)
- [+] CI pipeline'a passkey phase-1 auth flow adimi eklendi
- [+] Passkey env policy eklendi: GIGACHAD_PASSKEY_RP_ID, GIGACHAD_PASSKEY_ALLOWED_ORIGINS, GIGACHAD_PASSKEY_STRICT_METADATA
- [+] test_auth_passkey_persistence.ps1 eklendi ve PASS alindi (restart sonrasi passkey login)
- [+] CI pipeline'a passkey credential persistence test adimi eklendi.
- [+] Passkey negatif testleri genisletildi (bad signature + signCount rollback bloklama).



