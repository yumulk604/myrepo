# 🍃 MongoDB Entegrasyon Rehberi

Media Server API'yi MongoDB ile entegre etmek için adım adım rehber.

## 📋 İçindekiler

1. [MongoDB Kurulumu](#mongodb-kurulumu)
2. [C++ MongoDB Driver](#c-mongodb-driver)
3. [Kod Entegrasyonu](#kod-entegrasyonu)
4. [Örnek Kullanım](#örnek-kullanım)

---

## 🔧 MongoDB Kurulumu

### Windows için MongoDB Community Edition

1. **MongoDB İndir:**
   ```
   https://www.mongodb.com/try/download/community
   ```

2. **MongoDB Compass İndir (GUI):**
   ```
   https://www.mongodb.com/try/download/compass
   ```

3. **MongoDB Başlat:**
   ```powershell
   # MongoDB servisini başlat
   net start MongoDB
   
   # Veya manuel başlatma
   mongod --dbpath C:\data\db
   ```

4. **Connection String:**
   ```
   mongodb://localhost:27017
   ```

---

## 📦 C++ MongoDB Driver Kurulumu

### Seçenek 1: vcpkg (Önerilen)

```powershell
# vcpkg kur
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# MongoDB driver kur
.\vcpkg install mongo-cxx-driver

# CMake ile entegre et
.\vcpkg integrate install
```

### Seçenek 2: Manuel Kurulum

1. **mongocxx driver'ı indir:**
   ```
   https://github.com/mongodb/mongo-cxx-driver/releases
   ```

2. **Build et:**
   ```powershell
   cd mongo-cxx-driver\build
   cmake .. -DCMAKE_INSTALL_PREFIX=C:\mongo-cxx-driver
   cmake --build . --target install
   ```

---

## 💾 Veri Modeli (MongoDB Collections)

### Collection: messages
```javascript
{
  _id: ObjectId("..."),
  id: "msg_1729564800123456",
  username: "alice",
  content: "Merhaba!",
  roomId: "room1",
  timestamp: ISODate("2025-10-22T04:30:00Z"),
  createdAt: ISODate("2025-10-22T04:30:00Z")
}
```

### Collection: users
```javascript
{
  _id: ObjectId("..."),
  id: "user_alice",
  username: "alice",
  email: "alice@example.com",
  status: "online",
  lastSeen: ISODate("2025-10-22T04:30:00Z"),
  createdAt: ISODate("2025-10-22T04:30:00Z")
}
```

### Collection: call_sessions
```javascript
{
  _id: ObjectId("..."),
  id: "voice_1729564800123456",
  type: "voice", // or "video"
  callerId: "user_alice",
  callerName: "Alice",
  receiverId: "user_bob",
  receiverName: "Bob",
  status: "active", // ringing, active, ended
  offer: "SDP_OFFER_DATA",
  answer: "SDP_ANSWER_DATA",
  iceCandidates: ["candidate1", "candidate2"],
  startTime: ISODate("2025-10-22T04:30:00Z"),
  endTime: ISODate("2025-10-22T04:35:00Z"),
  duration: 300 // seconds
}
```

### Collection: live_streams
```javascript
{
  _id: ObjectId("..."),
  id: "stream_1729564800123456",
  streamerId: "user_alice",
  streamerName: "Alice",
  title: "Canlı Kodlama",
  description: "C++ öğreniyoruz",
  streamKey: "key_1729564800789012",
  status: "live", // or "ended"
  viewers: ["user_bob", "user_charlie"],
  viewerCount: 2,
  maxViewers: 150,
  startTime: ISODate("2025-10-22T04:30:00Z"),
  endTime: null,
  tags: ["coding", "cpp", "tutorial"]
}
```

---

## 🔌 Kod Entegrasyonu

### MongoDB Connection Manager

```cpp
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>

class MongoDBManager {
private:
    mongocxx::instance instance{};
    mongocxx::client client;
    mongocxx::database db;
    
public:
    MongoDBManager(const std::string& uri = "mongodb://localhost:27017") 
        : client{mongocxx::uri{uri}}, 
          db{client["media_server"]} {}
    
    // Messages Collection
    mongocxx::collection messages() {
        return db["messages"];
    }
    
    // Users Collection
    mongocxx::collection users() {
        return db["users"];
    }
    
    // Call Sessions Collection
    mongocxx::collection callSessions() {
        return db["call_sessions"];
    }
    
    // Live Streams Collection
    mongocxx::collection liveStreams() {
        return db["live_streams"];
    }
};

// Global instance
MongoDBManager mongo;
```

### Chat API MongoDB Entegrasyonu

```cpp
HTTPResponse handleChatAPI_MongoDB(const HTTPRequest& req) {
    HTTPResponse res;
    res.headers["Content-Type"] = "application/json";
    res.headers["Access-Control-Allow-Origin"] = "*";
    
    // POST /api/chat/messages
    if (req.method == "POST" && req.path == "/api/chat/messages") {
        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::finalize;
        
        std::string username = extractJSONValue(req.body, "username");
        std::string content = extractJSONValue(req.body, "content");
        std::string roomId = extractJSONValue(req.body, "roomId");
        
        // MongoDB document oluştur
        auto doc = document{}
            << "id" << generateId("msg_")
            << "username" << username
            << "content" << content
            << "roomId" << roomId
            << "timestamp" << getCurrentTimestamp()
            << "createdAt" << bsoncxx::types::b_date{std::chrono::system_clock::now()}
            << finalize;
        
        // MongoDB'ye kaydet
        auto result = mongo.messages().insert_one(doc.view());
        
        if (result) {
            res.status = 201;
            res.body = bsoncxx::to_json(doc.view());
        } else {
            res.status = 500;
            res.body = "{\"error\":\"Failed to save message\"}";
        }
        
        return res;
    }
    
    // GET /api/chat/messages
    if (req.method == "GET" && req.path == "/api/chat/messages") {
        auto cursor = mongo.messages().find({});
        
        std::stringstream ss;
        ss << "[";
        bool first = true;
        
        for (auto&& doc : cursor) {
            if (!first) ss << ",";
            ss << bsoncxx::to_json(doc);
            first = false;
        }
        
        ss << "]";
        res.body = ss.str();
        return res;
    }
    
    res.status = 404;
    res.body = "{\"error\":\"Endpoint not found\"}";
    return res;
}
```

### Call Sessions MongoDB Entegrasyonu

```cpp
// Voice/Video call başlatma
HTTPResponse startCall_MongoDB(const HTTPRequest& req) {
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::finalize;
    
    auto doc = document{}
        << "id" << generateId("call_")
        << "type" << extractJSONValue(req.body, "type") // voice or video
        << "callerId" << extractJSONValue(req.body, "callerId")
        << "callerName" << extractJSONValue(req.body, "callerName")
        << "receiverId" << extractJSONValue(req.body, "receiverId")
        << "receiverName" << extractJSONValue(req.body, "receiverName")
        << "status" << "ringing"
        << "startTime" << bsoncxx::types::b_date{std::chrono::system_clock::now()}
        << finalize;
    
    auto result = mongo.callSessions().insert_one(doc.view());
    
    HTTPResponse res;
    if (result) {
        res.status = 201;
        res.body = bsoncxx::to_json(doc.view());
    } else {
        res.status = 500;
        res.body = "{\"error\":\"Failed to create call\"}";
    }
    
    return res;
}
```

### Live Stream MongoDB Entegrasyonu

```cpp
// Canlı yayın başlatma
HTTPResponse startStream_MongoDB(const HTTPRequest& req) {
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::array;
    using bsoncxx::builder::stream::finalize;
    
    auto doc = document{}
        << "id" << generateId("stream_")
        << "streamerId" << extractJSONValue(req.body, "streamerId")
        << "streamerName" << extractJSONValue(req.body, "streamerName")
        << "title" << extractJSONValue(req.body, "title")
        << "description" << extractJSONValue(req.body, "description")
        << "streamKey" << generateId("key_")
        << "status" << "live"
        << "viewers" << array{} << bsoncxx::builder::stream::close_array
        << "viewerCount" << 0
        << "startTime" << bsoncxx::types::b_date{std::chrono::system_clock::now()}
        << finalize;
    
    auto result = mongo.liveStreams().insert_one(doc.view());
    
    HTTPResponse res;
    if (result) {
        res.status = 201;
        res.body = bsoncxx::to_json(doc.view());
    } else {
        res.status = 500;
        res.body = "{\"error\":\"Failed to start stream\"}";
    }
    
    return res;
}
```

---

## 📊 MongoDB Queries

### Faydalı Sorgular

```javascript
// En son mesajları getir
db.messages.find().sort({createdAt: -1}).limit(50)

// Belirli bir odadaki mesajlar
db.messages.find({roomId: "room1"}).sort({createdAt: 1})

// Aktif aramaları getir
db.call_sessions.find({status: "active"})

// Canlı yayınları getir
db.live_streams.find({status: "live"})

// En çok izlenen yayınlar
db.live_streams.find({status: "ended"}).sort({maxViewers: -1}).limit(10)

// Kullanıcı istatistikleri
db.messages.aggregate([
  {$group: {_id: "$username", messageCount: {$sum: 1}}},
  {$sort: {messageCount: -1}}
])

// Günlük arama istatistikleri
db.call_sessions.aggregate([
  {
    $match: {
      startTime: {
        $gte: new Date(new Date().setHours(0,0,0,0))
      }
    }
  },
  {
    $group: {
      _id: "$type",
      count: {$sum: 1},
      totalDuration: {$sum: "$duration"}
    }
  }
])
```

---

## 🔍 Indexes (Performans Optimizasyonu)

```javascript
// Messages koleksiyonu için index'ler
db.messages.createIndex({roomId: 1, createdAt: -1})
db.messages.createIndex({username: 1})
db.messages.createIndex({createdAt: -1})

// Call Sessions için index'ler
db.call_sessions.createIndex({callerId: 1, status: 1})
db.call_sessions.createIndex({receiverId: 1, status: 1})
db.call_sessions.createIndex({status: 1, startTime: -1})

// Live Streams için index'ler
db.live_streams.createIndex({status: 1, startTime: -1})
db.live_streams.createIndex({streamerId: 1})
db.live_streams.createIndex({tags: 1})

// Users için index'ler
db.users.createIndex({username: 1}, {unique: true})
db.users.createIndex({email: 1}, {unique: true})
db.users.createIndex({status: 1})
```

---

## 🎯 CMakeLists.txt Örneği

```cmake
cmake_minimum_required(VERSION 3.14)
project(MediaServer)

set(CMAKE_CXX_STANDARD 17)

# MongoDB C++ Driver
find_package(mongocxx REQUIRED)
find_package(bsoncxx REQUIRED)

add_executable(media_server_mongo media_server_mongo.cpp)

target_link_libraries(media_server_mongo 
    PRIVATE 
    mongo::mongocxx_shared
    mongo::bsoncxx_shared
    ws2_32
)
```

---

## 📝 Environment Variables

`.env` dosyası oluşturun:

```bash
MONGODB_URI=mongodb://localhost:27017
MONGODB_DATABASE=media_server
SERVER_PORT=8080
JWT_SECRET=your_secret_key_here
```

---

## 🧪 MongoDB ile Test

```powershell
# MongoDB bağlantısını test et
mongosh "mongodb://localhost:27017/media_server"

# Koleksiyonları listele
show collections

# Mesajları görüntüle
db.messages.find().pretty()

# Canlı yayınları görüntüle
db.live_streams.find({status: "live"}).pretty()

# Veritabanı istatistikleri
db.stats()
```

---

## 🔐 Production Best Practices

1. **Authentication:**
   ```javascript
   // MongoDB kullanıcısı oluştur
   use admin
   db.createUser({
     user: "mediaserver",
     pwd: "secure_password",
     roles: [{role: "readWrite", db: "media_server"}]
   })
   ```

2. **Connection Pooling:**
   ```cpp
   mongocxx::options::client client_opts;
   mongocxx::options::pool pool_opts;
   pool_opts.max_pool_size(100);
   
   mongocxx::pool pool{mongocxx::uri{"mongodb://localhost:27017"}, pool_opts};
   ```

3. **Error Handling:**
   ```cpp
   try {
       auto result = mongo.messages().insert_one(doc.view());
   } catch (const mongocxx::exception& e) {
       std::cerr << "MongoDB error: " << e.what() << std::endl;
       // Handle error
   }
   ```

4. **Replica Set (Production):**
   ```
   mongodb://host1:27017,host2:27017,host3:27017/?replicaSet=rs0
   ```

---

## 📚 Kaynaklar

- [MongoDB C++ Driver Docs](https://mongocxx.org/)
- [MongoDB Manual](https://docs.mongodb.com/)
- [MongoDB University](https://university.mongodb.com/)

---

## ⚡ Hızlı Başlangıç

```powershell
# 1. MongoDB başlat
mongod --dbpath C:\data\db

# 2. Database oluştur
mongosh
use media_server

# 3. Test verisi ekle
db.messages.insertOne({
  id: "msg_1",
  username: "test",
  content: "Hello MongoDB!",
  timestamp: new Date()
})

# 4. Verileri sorgula
db.messages.find().pretty()
```

---

**Not:** Mevcut `media_server.cpp` in-memory depolama kullanır. MongoDB entegrasyonu için 
yukarıdaki kod örneklerini kullanarak yeni bir `media_server_mongo.cpp` oluşturabilirsiniz.

MongoDB entegrasyonu production kullanımı için önerilir. Development için mevcut 
in-memory versiyon yeterlidir.



