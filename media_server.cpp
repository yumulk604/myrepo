// Complete Media Server with MongoDB Integration
// Supports: Chat, Voice Calls, Video Calls, Live Streaming
// Compile: g++ -std=c++17 media_server.cpp -o media_server.exe -lws2_32 -O2 -static

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <mutex>
#include <thread>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <random>
#include <fstream>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <unordered_map>
#include <atomic>
#include <cstdio>
#include <cerrno>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct Message {
    std::string id;
    std::string username;
    std::string content;
    std::string timestamp;
    std::string roomId;
    
    std::string toJSON() const {
        return "{\"id\":\"" + escapeJSON(id) + 
               "\",\"username\":\"" + escapeJSON(username) + 
               "\",\"content\":\"" + escapeJSON(content) + 
               "\",\"timestamp\":\"" + escapeJSON(timestamp) + 
               "\",\"roomId\":\"" + escapeJSON(roomId) + "\"}";
    }
    
private:
    static std::string escapeJSON(const std::string& s) {
        std::string result;
        for (char c : s) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c;
            }
        }
        return result;
    }
};

struct User {
    std::string id;
    std::string username;
    std::string status; // online, offline, busy, in-call
    std::string lastSeen;
    
    std::string toJSON() const {
        return "{\"id\":\"" + id + 
               "\",\"username\":\"" + username + 
               "\",\"status\":\"" + status + 
               "\",\"lastSeen\":\"" + lastSeen + "\"}";
    }
};

struct CallSession {
    std::string id;
    std::string type; // voice, video
    std::string callerId;
    std::string callerName;
    std::string receiverId;
    std::string receiverName;
    std::string status; // ringing, active, ended
    std::string startTime;
    std::string endTime;
    std::string offer; // WebRTC offer SDP
    std::string answer; // WebRTC answer SDP
    std::vector<std::string> iceCandidates;
    
    std::string toJSON() const {
        return "{\"id\":\"" + id + 
               "\",\"type\":\"" + type + 
               "\",\"callerId\":\"" + callerId + 
               "\",\"callerName\":\"" + callerName + 
               "\",\"receiverId\":\"" + receiverId + 
               "\",\"receiverName\":\"" + receiverName + 
               "\",\"status\":\"" + status + 
               "\",\"startTime\":\"" + startTime + 
               "\",\"endTime\":\"" + endTime + "\"}";
    }
};

struct LiveStream {
    std::string id;
    std::string streamerId;
    std::string streamerName;
    std::string title;
    std::string description;
    std::string status; // live, ended
    std::string startTime;
    std::string endTime;
    int viewerCount;
    std::vector<std::string> viewers;
    std::string streamKey;
    
    std::string toJSON() const {
        std::stringstream ss;
        ss << "{\"id\":\"" << id << "\""
           << ",\"streamerId\":\"" << streamerId << "\""
           << ",\"streamerName\":\"" << streamerName << "\""
           << ",\"title\":\"" << title << "\""
           << ",\"description\":\"" << description << "\""
           << ",\"status\":\"" << status << "\""
           << ",\"startTime\":\"" << startTime << "\""
           << ",\"endTime\":\"" << endTime << "\""
           << ",\"viewerCount\":" << viewerCount
           << ",\"streamKey\":\"" << streamKey << "\"}";
        return ss.str();
    }
};

struct ScreenRecording {
    std::string id;
    std::string userId;
    std::string userName;
    std::string filename;
    std::string quality; // 1080p, 720p, 480p
    std::string captureType; // screen, window, tab
    std::string status; // recording, stopped
    std::string startTime;
    std::string endTime;
    int duration; // seconds
    
    std::string toJSON() const {
        std::stringstream ss;
        ss << "{\"id\":\"" << id << "\""
           << ",\"userId\":\"" << userId << "\""
           << ",\"userName\":\"" << userName << "\""
           << ",\"filename\":\"" << filename << "\""
           << ",\"quality\":\"" << quality << "\""
           << ",\"captureType\":\"" << captureType << "\""
           << ",\"status\":\"" << status << "\""
           << ",\"startTime\":\"" << startTime << "\""
           << ",\"endTime\":\"" << endTime << "\""
           << ",\"duration\":" << duration << "}";
        return ss.str();
    }
};

// ============================================================================
// GLOBAL STATE (In-Memory Storage - MongoDB-like structure)
// ============================================================================

std::vector<Message> messages;
std::map<std::string, User> users;
std::map<std::string, CallSession> callSessions;
std::map<std::string, LiveStream> liveStreams;
std::map<std::string, ScreenRecording> screenRecordings;

std::mutex messages_mutex;
std::mutex users_mutex;
std::mutex calls_mutex;
std::mutex streams_mutex;
std::mutex recordings_mutex;

int message_counter = 0;
int call_counter = 0;
int stream_counter = 0;
int recording_counter = 0;

struct WebSocketClient {
    int socket;
    std::string roomId;
    std::string userId;
};

std::vector<WebSocketClient> wsClients;
std::mutex ws_mutex;
std::unordered_map<std::string, std::deque<std::chrono::steady_clock::time_point>> wsConnectionAttempts;
std::mutex ws_rate_mutex;

const std::string kDataDir = "data";
const std::string kMessagesFile = "data/messages.jsonl";
const std::string kCallsFile = "data/calls.jsonl";
const std::string kStreamsFile = "data/streams.jsonl";
const std::string kRecordingsFile = "data/recordings.jsonl";
const std::string kSqliteDbFile = "data/media.db";

std::string detectStorageMode() {
    const char* primary = std::getenv("GIGACHAD_STORAGE_MODE");
    const char* fallback = std::getenv("MEDIA_STORAGE_MODE");
    std::string mode = primary && *primary ? primary : (fallback && *fallback ? fallback : "hybrid");
    auto trim = [](std::string& v) {
        auto isWs = [](unsigned char c) { return std::isspace(c) != 0; };
        while (!v.empty() && isWs(static_cast<unsigned char>(v.front()))) v.erase(v.begin());
        while (!v.empty() && isWs(static_cast<unsigned char>(v.back()))) v.pop_back();
    };
    trim(mode);
    std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return mode;
}

bool storageModeIsJsonlOnly() {
    static const std::string mode = detectStorageMode();
    return mode == "jsonl_only" || mode == "jsonl";
}

bool storageModeIsSqliteOnly() {
    static const std::string mode = detectStorageMode();
    return mode == "sqlite_only" || mode == "sqlite";
}

std::string detectEventBusMode() {
    const char* primary = std::getenv("GIGACHAD_EVENT_BUS");
    const char* fallback = std::getenv("MEDIA_EVENT_BUS");
    std::string mode = primary && *primary ? primary : (fallback && *fallback ? fallback : "memory");
    auto trim = [](std::string& v) {
        auto isWs = [](unsigned char c) { return std::isspace(c) != 0; };
        while (!v.empty() && isWs(static_cast<unsigned char>(v.front()))) v.erase(v.begin());
        while (!v.empty() && isWs(static_cast<unsigned char>(v.back()))) v.pop_back();
    };
    trim(mode);
    std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return mode;
}

bool eventBusModeIsRedis() {
    static const std::string mode = detectEventBusMode();
    return mode == "redis";
}

std::string detectRedisHost() {
    const char* primary = std::getenv("GIGACHAD_REDIS_HOST");
    const char* fallback = std::getenv("MEDIA_REDIS_HOST");
    auto trimCopy = [](std::string s) {
        auto isWs = [](unsigned char c) { return std::isspace(c) != 0; };
        while (!s.empty() && isWs(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
        while (!s.empty() && isWs(static_cast<unsigned char>(s.back()))) s.pop_back();
        return s;
    };
    if (primary && *primary) {
        const std::string host = trimCopy(primary);
        if (!host.empty()) return host;
    }
    if (fallback && *fallback) {
        const std::string host = trimCopy(fallback);
        if (!host.empty()) return host;
    }
    return "127.0.0.1";
}

int detectRedisPort() {
    const char* primary = std::getenv("GIGACHAD_REDIS_PORT");
    const char* fallback = std::getenv("MEDIA_REDIS_PORT");
    const char* selected = (primary && *primary) ? primary : ((fallback && *fallback) ? fallback : nullptr);
    if (!selected) return 6379;
    try {
        std::string raw = selected;
        auto isWs = [](unsigned char c) { return std::isspace(c) != 0; };
        while (!raw.empty() && isWs(static_cast<unsigned char>(raw.front()))) raw.erase(raw.begin());
        while (!raw.empty() && isWs(static_cast<unsigned char>(raw.back()))) raw.pop_back();
        int parsed = std::stoi(raw);
        return parsed > 0 ? parsed : 6379;
    } catch (...) {
        return 6379;
    }
}

std::string detectRedisPassword() {
    const char* primary = std::getenv("GIGACHAD_REDIS_PASSWORD");
    const char* fallback = std::getenv("MEDIA_REDIS_PASSWORD");
    auto trimCopy = [](std::string s) {
        auto isWs = [](unsigned char c) { return std::isspace(c) != 0; };
        while (!s.empty() && isWs(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
        while (!s.empty() && isWs(static_cast<unsigned char>(s.back()))) s.pop_back();
        return s;
    };
    if (primary && *primary) return trimCopy(primary);
    if (fallback && *fallback) return trimCopy(fallback);
    return "";
}

std::string detectEventBusChannel() {
    const char* primary = std::getenv("GIGACHAD_EVENT_BUS_CHANNEL");
    const char* fallback = std::getenv("MEDIA_EVENT_BUS_CHANNEL");
    auto trimCopy = [](std::string s) {
        auto isWs = [](unsigned char c) { return std::isspace(c) != 0; };
        while (!s.empty() && isWs(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
        while (!s.empty() && isWs(static_cast<unsigned char>(s.back()))) s.pop_back();
        return s;
    };
    if (primary && *primary) {
        const std::string channel = trimCopy(primary);
        if (!channel.empty()) return channel;
    }
    if (fallback && *fallback) {
        const std::string channel = trimCopy(fallback);
        if (!channel.empty()) return channel;
    }
    return "gagabunto.media.events";
}

int detectServerPort() {
    const char* primary = std::getenv("GIGACHAD_SERVER_PORT");
    const char* fallback = std::getenv("MEDIA_SERVER_PORT");
    const char* selected = (primary && *primary) ? primary : ((fallback && *fallback) ? fallback : nullptr);
    if (!selected) return 8080;
    try {
        int parsed = std::stoi(selected);
        if (parsed >= 1 && parsed <= 65535) {
            return parsed;
        }
    } catch (...) {
    }
    return 8080;
}

struct sqlite3;
struct sqlite3_stmt;

using sqlite3_open_fn = int (*)(const char*, sqlite3**);
using sqlite3_close_fn = int (*)(sqlite3*);
using sqlite3_exec_fn = int (*)(sqlite3*, const char*, int (*)(void*, int, char**, char**), void*, char**);
using sqlite3_free_fn = void (*)(void*);
using sqlite3_prepare_v2_fn = int (*)(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
using sqlite3_step_fn = int (*)(sqlite3_stmt*);
using sqlite3_finalize_fn = int (*)(sqlite3_stmt*);
using sqlite3_bind_text_fn = int (*)(sqlite3_stmt*, int, const char*, int, void (*)(void*));
using sqlite3_reset_fn = int (*)(sqlite3_stmt*);
using sqlite3_clear_bindings_fn = int (*)(sqlite3_stmt*);
using sqlite3_column_text_fn = const unsigned char* (*)(sqlite3_stmt*, int);

#ifdef _WIN32
HMODULE g_sqliteModule = nullptr;
#else
void* g_sqliteModule = nullptr;
#endif
sqlite3_open_fn p_sqlite3_open = nullptr;
sqlite3_close_fn p_sqlite3_close = nullptr;
sqlite3_exec_fn p_sqlite3_exec = nullptr;
sqlite3_free_fn p_sqlite3_free = nullptr;
sqlite3_prepare_v2_fn p_sqlite3_prepare_v2 = nullptr;
sqlite3_step_fn p_sqlite3_step = nullptr;
sqlite3_finalize_fn p_sqlite3_finalize = nullptr;
sqlite3_bind_text_fn p_sqlite3_bind_text = nullptr;
sqlite3_reset_fn p_sqlite3_reset = nullptr;
sqlite3_clear_bindings_fn p_sqlite3_clear_bindings = nullptr;
sqlite3_column_text_fn p_sqlite3_column_text = nullptr;

const int SQLITE_OK = 0;
const int SQLITE_ROW = 100;
const int SQLITE_DONE = 101;
bool g_sqliteReady = false;
std::mutex sqlite_mutex;
std::atomic<bool> g_eventBusRedisWarned{false};
std::atomic<bool> g_eventBusRedisStarted{false};
std::atomic<bool> g_eventBusRedisReady{false};
std::atomic<bool> g_eventBusRedisPublishWarned{false};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    
    #ifdef _WIN32
    localtime_s(&tm_buf, &time);
    #else
    localtime_r(&time, &tm_buf);
    #endif
    
    std::stringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string generateId(const std::string& prefix) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    return prefix + std::to_string(ms) + std::to_string(dis(gen));
}

std::string extractJSONValue(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";
    
    pos += searchKey.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == ':')) {
        pos++;
    }
    
    if (pos >= json.length() || json[pos] != '"') return "";
    pos++;
    
    size_t endPos = json.find("\"", pos);
    if (endPos == std::string::npos) return "";
    
    return json.substr(pos, endPos - pos);
}

bool hasJSONKey(const std::string& json, const std::string& key) {
    return json.find("\"" + key + "\":") != std::string::npos;
}

int extractJSONIntValue(const std::string& json, const std::string& key, int defaultValue = 0) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return defaultValue;

    pos += searchKey.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == ':')) {
        pos++;
    }
    if (pos >= json.length()) return defaultValue;

    bool negative = false;
    if (json[pos] == '-') {
        negative = true;
        pos++;
    }

    size_t start = pos;
    while (pos < json.length() && std::isdigit(static_cast<unsigned char>(json[pos]))) {
        pos++;
    }
    if (start == pos) return defaultValue;

    try {
        int value = std::stoi(json.substr(start, pos - start));
        return negative ? -value : value;
    } catch (...) {
        return defaultValue;
    }
}

bool ensureDataDirectory() {
#ifdef _WIN32
    int rc = _mkdir(kDataDir.c_str());
#else
    int rc = mkdir(kDataDir.c_str(), 0755);
#endif
    if (rc == 0) return true;
    return errno == EEXIST;
}

bool writeLinesToFile(const std::string& filepath, const std::vector<std::string>& lines) {
    std::string tmpPath = filepath + ".tmp";
    std::ofstream out(tmpPath, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    for (const auto& line : lines) {
        out << line << "\n";
    }
    out.close();

    std::remove(filepath.c_str());
    return std::rename(tmpPath.c_str(), filepath.c_str()) == 0;
}

std::vector<std::string> readLinesFromFile(const std::string& filepath) {
    std::vector<std::string> lines;
    std::ifstream in(filepath);
    if (!in.is_open()) {
        return lines;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

bool loadSqliteSymbols() {
#ifdef _WIN32
    if (g_sqliteModule == nullptr) {
        const std::vector<std::string> candidates = {
            "sqlite3.dll",
            "C:\\TDM-GCC-64\\gdb64\\bin\\DLLs\\sqlite3.dll",
            "C:\\TDM-GCC-64\\gdb32\\bin\\DLLs\\sqlite3.dll"
        };
        for (const auto& path : candidates) {
            HMODULE m = LoadLibraryA(path.c_str());
            if (m != nullptr) {
                g_sqliteModule = m;
                break;
            }
        }
    }
    if (g_sqliteModule == nullptr) {
        return false;
    }

    p_sqlite3_open = reinterpret_cast<sqlite3_open_fn>(GetProcAddress(g_sqliteModule, "sqlite3_open"));
    p_sqlite3_close = reinterpret_cast<sqlite3_close_fn>(GetProcAddress(g_sqliteModule, "sqlite3_close"));
    p_sqlite3_exec = reinterpret_cast<sqlite3_exec_fn>(GetProcAddress(g_sqliteModule, "sqlite3_exec"));
    p_sqlite3_free = reinterpret_cast<sqlite3_free_fn>(GetProcAddress(g_sqliteModule, "sqlite3_free"));
    p_sqlite3_prepare_v2 = reinterpret_cast<sqlite3_prepare_v2_fn>(GetProcAddress(g_sqliteModule, "sqlite3_prepare_v2"));
    p_sqlite3_step = reinterpret_cast<sqlite3_step_fn>(GetProcAddress(g_sqliteModule, "sqlite3_step"));
    p_sqlite3_finalize = reinterpret_cast<sqlite3_finalize_fn>(GetProcAddress(g_sqliteModule, "sqlite3_finalize"));
    p_sqlite3_bind_text = reinterpret_cast<sqlite3_bind_text_fn>(GetProcAddress(g_sqliteModule, "sqlite3_bind_text"));
    p_sqlite3_reset = reinterpret_cast<sqlite3_reset_fn>(GetProcAddress(g_sqliteModule, "sqlite3_reset"));
    p_sqlite3_clear_bindings = reinterpret_cast<sqlite3_clear_bindings_fn>(GetProcAddress(g_sqliteModule, "sqlite3_clear_bindings"));
    p_sqlite3_column_text = reinterpret_cast<sqlite3_column_text_fn>(GetProcAddress(g_sqliteModule, "sqlite3_column_text"));
#else
    return false;
#endif

    return p_sqlite3_open && p_sqlite3_close && p_sqlite3_exec && p_sqlite3_free &&
           p_sqlite3_prepare_v2 && p_sqlite3_step && p_sqlite3_finalize &&
           p_sqlite3_bind_text && p_sqlite3_reset && p_sqlite3_clear_bindings &&
           p_sqlite3_column_text;
}

bool sqliteExec(sqlite3* db, const std::string& sql) {
    char* err = nullptr;
    int rc = p_sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);
    if (err) {
        p_sqlite3_free(err);
    }
    return rc == SQLITE_OK;
}

bool initSqliteStorage() {
    std::lock_guard<std::mutex> lock(sqlite_mutex);
    if (g_sqliteReady) {
        return true;
    }
    if (!loadSqliteSymbols()) {
        return false;
    }

    sqlite3* db = nullptr;
    if (p_sqlite3_open(kSqliteDbFile.c_str(), &db) != SQLITE_OK || db == nullptr) {
        if (db) p_sqlite3_close(db);
        return false;
    }

    const std::string createMessagesSql =
        "CREATE TABLE IF NOT EXISTS messages ("
        "id TEXT PRIMARY KEY,"
        "username TEXT,"
        "content TEXT,"
        "timestamp TEXT,"
        "roomId TEXT"
        ");";
    const std::string createCallsSql =
        "CREATE TABLE IF NOT EXISTS call_sessions ("
        "id TEXT PRIMARY KEY,"
        "type TEXT,"
        "callerId TEXT,"
        "callerName TEXT,"
        "receiverId TEXT,"
        "receiverName TEXT,"
        "status TEXT,"
        "startTime TEXT,"
        "endTime TEXT,"
        "offer TEXT,"
        "answer TEXT,"
        "iceCandidates TEXT"
        ");";
    const std::string createStreamsSql =
        "CREATE TABLE IF NOT EXISTS live_streams ("
        "id TEXT PRIMARY KEY,"
        "streamerId TEXT,"
        "streamerName TEXT,"
        "title TEXT,"
        "description TEXT,"
        "status TEXT,"
        "startTime TEXT,"
        "endTime TEXT,"
        "viewerCount TEXT,"
        "streamKey TEXT,"
        "viewers TEXT"
        ");";
    const std::string createRecordingsSql =
        "CREATE TABLE IF NOT EXISTS recordings ("
        "id TEXT PRIMARY KEY,"
        "userId TEXT,"
        "userName TEXT,"
        "filename TEXT,"
        "quality TEXT,"
        "captureType TEXT,"
        "status TEXT,"
        "startTime TEXT,"
        "endTime TEXT,"
        "duration TEXT"
        ");";
    bool ok = sqliteExec(db, createMessagesSql) &&
              sqliteExec(db, createCallsSql) &&
              sqliteExec(db, createStreamsSql) &&
              sqliteExec(db, createRecordingsSql);
    p_sqlite3_close(db);
    g_sqliteReady = ok;
    return ok;
}

bool persistMessagesToSqliteUnlocked() {
    if (!g_sqliteReady && !initSqliteStorage()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(sqlite_mutex);
    sqlite3* db = nullptr;
    if (p_sqlite3_open(kSqliteDbFile.c_str(), &db) != SQLITE_OK || db == nullptr) {
        if (db) p_sqlite3_close(db);
        return false;
    }

    if (!sqliteExec(db, "BEGIN TRANSACTION;")) {
        p_sqlite3_close(db);
        return false;
    }
    if (!sqliteExec(db, "DELETE FROM messages;")) {
        sqliteExec(db, "ROLLBACK;");
        p_sqlite3_close(db);
        return false;
    }

    const char* insertSql = "INSERT INTO messages(id, username, content, timestamp, roomId) VALUES(?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (p_sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr) != SQLITE_OK || stmt == nullptr) {
        sqliteExec(db, "ROLLBACK;");
        p_sqlite3_close(db);
        return false;
    }

    bool ok = true;
    for (const auto& msg : messages) {
        auto transient = reinterpret_cast<void(*)(void*)>(-1);
        if (p_sqlite3_bind_text(stmt, 1, msg.id.c_str(), -1, transient) != SQLITE_OK ||
            p_sqlite3_bind_text(stmt, 2, msg.username.c_str(), -1, transient) != SQLITE_OK ||
            p_sqlite3_bind_text(stmt, 3, msg.content.c_str(), -1, transient) != SQLITE_OK ||
            p_sqlite3_bind_text(stmt, 4, msg.timestamp.c_str(), -1, transient) != SQLITE_OK ||
            p_sqlite3_bind_text(stmt, 5, msg.roomId.c_str(), -1, transient) != SQLITE_OK) {
            ok = false;
            break;
        }
        int rc = p_sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            ok = false;
            break;
        }
        p_sqlite3_reset(stmt);
        p_sqlite3_clear_bindings(stmt);
    }

    p_sqlite3_finalize(stmt);
    if (ok) {
        ok = sqliteExec(db, "COMMIT;");
    } else {
        sqliteExec(db, "ROLLBACK;");
    }
    p_sqlite3_close(db);
    return ok;
}

bool loadMessagesFromSqlite() {
    if (!g_sqliteReady && !initSqliteStorage()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(sqlite_mutex);
    sqlite3* db = nullptr;
    if (p_sqlite3_open(kSqliteDbFile.c_str(), &db) != SQLITE_OK || db == nullptr) {
        if (db) p_sqlite3_close(db);
        return false;
    }

    const char* querySql = "SELECT id, username, content, timestamp, roomId FROM messages ORDER BY rowid ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (p_sqlite3_prepare_v2(db, querySql, -1, &stmt, nullptr) != SQLITE_OK || stmt == nullptr) {
        p_sqlite3_close(db);
        return false;
    }

    messages.clear();
    while (true) {
        int rc = p_sqlite3_step(stmt);
        if (rc == SQLITE_DONE) {
            break;
        }
        if (rc != SQLITE_ROW) {
            p_sqlite3_finalize(stmt);
            p_sqlite3_close(db);
            return false;
        }

        Message msg;
        const unsigned char* c0 = p_sqlite3_column_text(stmt, 0);
        const unsigned char* c1 = p_sqlite3_column_text(stmt, 1);
        const unsigned char* c2 = p_sqlite3_column_text(stmt, 2);
        const unsigned char* c3 = p_sqlite3_column_text(stmt, 3);
        const unsigned char* c4 = p_sqlite3_column_text(stmt, 4);
        msg.id = c0 ? reinterpret_cast<const char*>(c0) : "";
        msg.username = c1 ? reinterpret_cast<const char*>(c1) : "";
        msg.content = c2 ? reinterpret_cast<const char*>(c2) : "";
        msg.timestamp = c3 ? reinterpret_cast<const char*>(c3) : "";
        msg.roomId = c4 ? reinterpret_cast<const char*>(c4) : "";
        if (!msg.id.empty()) {
            messages.push_back(msg);
        }
    }

    p_sqlite3_finalize(stmt);
    p_sqlite3_close(db);
    return true;
}

bool sqliteBindText(sqlite3_stmt* stmt, int idx, const std::string& value) {
    auto transient = reinterpret_cast<void(*)(void*)>(-1);
    return p_sqlite3_bind_text(stmt, idx, value.c_str(), -1, transient) == SQLITE_OK;
}

std::string joinVector(const std::vector<std::string>& items, const std::string& sep = "|") {
    std::stringstream ss;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) ss << sep;
        ss << items[i];
    }
    return ss.str();
}

std::vector<std::string> splitString(const std::string& input, char sep = '|') {
    std::vector<std::string> out;
    std::stringstream ss(input);
    std::string part;
    while (std::getline(ss, part, sep)) {
        if (!part.empty()) {
            out.push_back(part);
        }
    }
    return out;
}

int parseIntSafe(const std::string& text, int defaultValue = 0) {
    try {
        return std::stoi(text);
    } catch (...) {
        return defaultValue;
    }
}

bool persistCallsToSqliteUnlocked() {
    if (!g_sqliteReady && !initSqliteStorage()) return false;
    std::lock_guard<std::mutex> lock(sqlite_mutex);
    sqlite3* db = nullptr;
    if (p_sqlite3_open(kSqliteDbFile.c_str(), &db) != SQLITE_OK || db == nullptr) {
        if (db) p_sqlite3_close(db);
        return false;
    }
    if (!sqliteExec(db, "BEGIN TRANSACTION;") || !sqliteExec(db, "DELETE FROM call_sessions;")) {
        sqliteExec(db, "ROLLBACK;");
        p_sqlite3_close(db);
        return false;
    }
    const char* sql =
        "INSERT INTO call_sessions(id,type,callerId,callerName,receiverId,receiverName,status,startTime,endTime,offer,answer,iceCandidates) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    if (p_sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK || stmt == nullptr) {
        sqliteExec(db, "ROLLBACK;");
        p_sqlite3_close(db);
        return false;
    }
    bool ok = true;
    for (const auto& pair : callSessions) {
        const auto& call = pair.second;
        if (!sqliteBindText(stmt, 1, call.id) ||
            !sqliteBindText(stmt, 2, call.type) ||
            !sqliteBindText(stmt, 3, call.callerId) ||
            !sqliteBindText(stmt, 4, call.callerName) ||
            !sqliteBindText(stmt, 5, call.receiverId) ||
            !sqliteBindText(stmt, 6, call.receiverName) ||
            !sqliteBindText(stmt, 7, call.status) ||
            !sqliteBindText(stmt, 8, call.startTime) ||
            !sqliteBindText(stmt, 9, call.endTime) ||
            !sqliteBindText(stmt, 10, call.offer) ||
            !sqliteBindText(stmt, 11, call.answer) ||
            !sqliteBindText(stmt, 12, joinVector(call.iceCandidates))) {
            ok = false;
            break;
        }
        if (p_sqlite3_step(stmt) != SQLITE_DONE) {
            ok = false;
            break;
        }
        p_sqlite3_reset(stmt);
        p_sqlite3_clear_bindings(stmt);
    }
    p_sqlite3_finalize(stmt);
    if (ok) ok = sqliteExec(db, "COMMIT;"); else sqliteExec(db, "ROLLBACK;");
    p_sqlite3_close(db);
    return ok;
}

bool loadCallsFromSqlite() {
    if (!g_sqliteReady && !initSqliteStorage()) return false;
    std::lock_guard<std::mutex> lock(sqlite_mutex);
    sqlite3* db = nullptr;
    if (p_sqlite3_open(kSqliteDbFile.c_str(), &db) != SQLITE_OK || db == nullptr) {
        if (db) p_sqlite3_close(db);
        return false;
    }
    const char* sql =
        "SELECT id,type,callerId,callerName,receiverId,receiverName,status,startTime,endTime,offer,answer,iceCandidates "
        "FROM call_sessions ORDER BY rowid ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (p_sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK || stmt == nullptr) {
        p_sqlite3_close(db);
        return false;
    }
    callSessions.clear();
    while (true) {
        int rc = p_sqlite3_step(stmt);
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) {
            p_sqlite3_finalize(stmt);
            p_sqlite3_close(db);
            return false;
        }
        CallSession call;
        auto c0 = p_sqlite3_column_text(stmt, 0); auto c1 = p_sqlite3_column_text(stmt, 1);
        auto c2 = p_sqlite3_column_text(stmt, 2); auto c3 = p_sqlite3_column_text(stmt, 3);
        auto c4 = p_sqlite3_column_text(stmt, 4); auto c5 = p_sqlite3_column_text(stmt, 5);
        auto c6 = p_sqlite3_column_text(stmt, 6); auto c7 = p_sqlite3_column_text(stmt, 7);
        auto c8 = p_sqlite3_column_text(stmt, 8); auto c9 = p_sqlite3_column_text(stmt, 9);
        auto c10 = p_sqlite3_column_text(stmt, 10); auto c11 = p_sqlite3_column_text(stmt, 11);
        call.id = c0 ? reinterpret_cast<const char*>(c0) : "";
        call.type = c1 ? reinterpret_cast<const char*>(c1) : "";
        call.callerId = c2 ? reinterpret_cast<const char*>(c2) : "";
        call.callerName = c3 ? reinterpret_cast<const char*>(c3) : "";
        call.receiverId = c4 ? reinterpret_cast<const char*>(c4) : "";
        call.receiverName = c5 ? reinterpret_cast<const char*>(c5) : "";
        call.status = c6 ? reinterpret_cast<const char*>(c6) : "";
        call.startTime = c7 ? reinterpret_cast<const char*>(c7) : "";
        call.endTime = c8 ? reinterpret_cast<const char*>(c8) : "";
        call.offer = c9 ? reinterpret_cast<const char*>(c9) : "";
        call.answer = c10 ? reinterpret_cast<const char*>(c10) : "";
        call.iceCandidates = splitString(c11 ? reinterpret_cast<const char*>(c11) : "");
        if (!call.id.empty()) callSessions[call.id] = call;
    }
    p_sqlite3_finalize(stmt);
    p_sqlite3_close(db);
    return true;
}

bool persistStreamsToSqliteUnlocked() {
    if (!g_sqliteReady && !initSqliteStorage()) return false;
    std::lock_guard<std::mutex> lock(sqlite_mutex);
    sqlite3* db = nullptr;
    if (p_sqlite3_open(kSqliteDbFile.c_str(), &db) != SQLITE_OK || db == nullptr) {
        if (db) p_sqlite3_close(db);
        return false;
    }
    if (!sqliteExec(db, "BEGIN TRANSACTION;") || !sqliteExec(db, "DELETE FROM live_streams;")) {
        sqliteExec(db, "ROLLBACK;");
        p_sqlite3_close(db);
        return false;
    }
    const char* sql =
        "INSERT INTO live_streams(id,streamerId,streamerName,title,description,status,startTime,endTime,viewerCount,streamKey,viewers) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    if (p_sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK || stmt == nullptr) {
        sqliteExec(db, "ROLLBACK;");
        p_sqlite3_close(db);
        return false;
    }
    bool ok = true;
    for (const auto& pair : liveStreams) {
        const auto& s = pair.second;
        if (!sqliteBindText(stmt, 1, s.id) ||
            !sqliteBindText(stmt, 2, s.streamerId) ||
            !sqliteBindText(stmt, 3, s.streamerName) ||
            !sqliteBindText(stmt, 4, s.title) ||
            !sqliteBindText(stmt, 5, s.description) ||
            !sqliteBindText(stmt, 6, s.status) ||
            !sqliteBindText(stmt, 7, s.startTime) ||
            !sqliteBindText(stmt, 8, s.endTime) ||
            !sqliteBindText(stmt, 9, std::to_string(s.viewerCount)) ||
            !sqliteBindText(stmt, 10, s.streamKey) ||
            !sqliteBindText(stmt, 11, joinVector(s.viewers))) {
            ok = false;
            break;
        }
        if (p_sqlite3_step(stmt) != SQLITE_DONE) {
            ok = false;
            break;
        }
        p_sqlite3_reset(stmt);
        p_sqlite3_clear_bindings(stmt);
    }
    p_sqlite3_finalize(stmt);
    if (ok) ok = sqliteExec(db, "COMMIT;"); else sqliteExec(db, "ROLLBACK;");
    p_sqlite3_close(db);
    return ok;
}

bool loadStreamsFromSqlite() {
    if (!g_sqliteReady && !initSqliteStorage()) return false;
    std::lock_guard<std::mutex> lock(sqlite_mutex);
    sqlite3* db = nullptr;
    if (p_sqlite3_open(kSqliteDbFile.c_str(), &db) != SQLITE_OK || db == nullptr) {
        if (db) p_sqlite3_close(db);
        return false;
    }
    const char* sql =
        "SELECT id,streamerId,streamerName,title,description,status,startTime,endTime,viewerCount,streamKey,viewers "
        "FROM live_streams ORDER BY rowid ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (p_sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK || stmt == nullptr) {
        p_sqlite3_close(db);
        return false;
    }
    liveStreams.clear();
    while (true) {
        int rc = p_sqlite3_step(stmt);
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) {
            p_sqlite3_finalize(stmt);
            p_sqlite3_close(db);
            return false;
        }
        LiveStream s;
        auto c0 = p_sqlite3_column_text(stmt, 0); auto c1 = p_sqlite3_column_text(stmt, 1);
        auto c2 = p_sqlite3_column_text(stmt, 2); auto c3 = p_sqlite3_column_text(stmt, 3);
        auto c4 = p_sqlite3_column_text(stmt, 4); auto c5 = p_sqlite3_column_text(stmt, 5);
        auto c6 = p_sqlite3_column_text(stmt, 6); auto c7 = p_sqlite3_column_text(stmt, 7);
        auto c8 = p_sqlite3_column_text(stmt, 8); auto c9 = p_sqlite3_column_text(stmt, 9);
        auto c10 = p_sqlite3_column_text(stmt, 10);
        s.id = c0 ? reinterpret_cast<const char*>(c0) : "";
        s.streamerId = c1 ? reinterpret_cast<const char*>(c1) : "";
        s.streamerName = c2 ? reinterpret_cast<const char*>(c2) : "";
        s.title = c3 ? reinterpret_cast<const char*>(c3) : "";
        s.description = c4 ? reinterpret_cast<const char*>(c4) : "";
        s.status = c5 ? reinterpret_cast<const char*>(c5) : "";
        s.startTime = c6 ? reinterpret_cast<const char*>(c6) : "";
        s.endTime = c7 ? reinterpret_cast<const char*>(c7) : "";
        s.viewerCount = parseIntSafe(c8 ? reinterpret_cast<const char*>(c8) : "0", 0);
        s.streamKey = c9 ? reinterpret_cast<const char*>(c9) : "";
        s.viewers = splitString(c10 ? reinterpret_cast<const char*>(c10) : "");
        if (!s.id.empty()) liveStreams[s.id] = s;
    }
    p_sqlite3_finalize(stmt);
    p_sqlite3_close(db);
    return true;
}

bool persistRecordingsToSqliteUnlocked() {
    if (!g_sqliteReady && !initSqliteStorage()) return false;
    std::lock_guard<std::mutex> lock(sqlite_mutex);
    sqlite3* db = nullptr;
    if (p_sqlite3_open(kSqliteDbFile.c_str(), &db) != SQLITE_OK || db == nullptr) {
        if (db) p_sqlite3_close(db);
        return false;
    }
    if (!sqliteExec(db, "BEGIN TRANSACTION;") || !sqliteExec(db, "DELETE FROM recordings;")) {
        sqliteExec(db, "ROLLBACK;");
        p_sqlite3_close(db);
        return false;
    }
    const char* sql =
        "INSERT INTO recordings(id,userId,userName,filename,quality,captureType,status,startTime,endTime,duration) "
        "VALUES(?,?,?,?,?,?,?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    if (p_sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK || stmt == nullptr) {
        sqliteExec(db, "ROLLBACK;");
        p_sqlite3_close(db);
        return false;
    }
    bool ok = true;
    for (const auto& pair : screenRecordings) {
        const auto& r = pair.second;
        if (!sqliteBindText(stmt, 1, r.id) ||
            !sqliteBindText(stmt, 2, r.userId) ||
            !sqliteBindText(stmt, 3, r.userName) ||
            !sqliteBindText(stmt, 4, r.filename) ||
            !sqliteBindText(stmt, 5, r.quality) ||
            !sqliteBindText(stmt, 6, r.captureType) ||
            !sqliteBindText(stmt, 7, r.status) ||
            !sqliteBindText(stmt, 8, r.startTime) ||
            !sqliteBindText(stmt, 9, r.endTime) ||
            !sqliteBindText(stmt, 10, std::to_string(r.duration))) {
            ok = false;
            break;
        }
        if (p_sqlite3_step(stmt) != SQLITE_DONE) {
            ok = false;
            break;
        }
        p_sqlite3_reset(stmt);
        p_sqlite3_clear_bindings(stmt);
    }
    p_sqlite3_finalize(stmt);
    if (ok) ok = sqliteExec(db, "COMMIT;"); else sqliteExec(db, "ROLLBACK;");
    p_sqlite3_close(db);
    return ok;
}

bool loadRecordingsFromSqlite() {
    if (!g_sqliteReady && !initSqliteStorage()) return false;
    std::lock_guard<std::mutex> lock(sqlite_mutex);
    sqlite3* db = nullptr;
    if (p_sqlite3_open(kSqliteDbFile.c_str(), &db) != SQLITE_OK || db == nullptr) {
        if (db) p_sqlite3_close(db);
        return false;
    }
    const char* sql =
        "SELECT id,userId,userName,filename,quality,captureType,status,startTime,endTime,duration "
        "FROM recordings ORDER BY rowid ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (p_sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK || stmt == nullptr) {
        p_sqlite3_close(db);
        return false;
    }
    screenRecordings.clear();
    while (true) {
        int rc = p_sqlite3_step(stmt);
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) {
            p_sqlite3_finalize(stmt);
            p_sqlite3_close(db);
            return false;
        }
        ScreenRecording r;
        auto c0 = p_sqlite3_column_text(stmt, 0); auto c1 = p_sqlite3_column_text(stmt, 1);
        auto c2 = p_sqlite3_column_text(stmt, 2); auto c3 = p_sqlite3_column_text(stmt, 3);
        auto c4 = p_sqlite3_column_text(stmt, 4); auto c5 = p_sqlite3_column_text(stmt, 5);
        auto c6 = p_sqlite3_column_text(stmt, 6); auto c7 = p_sqlite3_column_text(stmt, 7);
        auto c8 = p_sqlite3_column_text(stmt, 8); auto c9 = p_sqlite3_column_text(stmt, 9);
        r.id = c0 ? reinterpret_cast<const char*>(c0) : "";
        r.userId = c1 ? reinterpret_cast<const char*>(c1) : "";
        r.userName = c2 ? reinterpret_cast<const char*>(c2) : "";
        r.filename = c3 ? reinterpret_cast<const char*>(c3) : "";
        r.quality = c4 ? reinterpret_cast<const char*>(c4) : "";
        r.captureType = c5 ? reinterpret_cast<const char*>(c5) : "";
        r.status = c6 ? reinterpret_cast<const char*>(c6) : "";
        r.startTime = c7 ? reinterpret_cast<const char*>(c7) : "";
        r.endTime = c8 ? reinterpret_cast<const char*>(c8) : "";
        r.duration = parseIntSafe(c9 ? reinterpret_cast<const char*>(c9) : "0", 0);
        if (!r.id.empty()) screenRecordings[r.id] = r;
    }
    p_sqlite3_finalize(stmt);
    p_sqlite3_close(db);
    return true;
}

void persistMessagesToDiskUnlocked() {
    if (!storageModeIsJsonlOnly()) {
        if (persistMessagesToSqliteUnlocked()) {
            return;
        }
        if (storageModeIsSqliteOnly()) {
            return;
        }
    }
    std::vector<std::string> lines;
    lines.reserve(messages.size());
    for (const auto& msg : messages) {
        lines.push_back(msg.toJSON());
    }
    writeLinesToFile(kMessagesFile, lines);
}

void persistCallsToDiskUnlocked() {
    if (!storageModeIsJsonlOnly()) {
        if (persistCallsToSqliteUnlocked()) {
            return;
        }
        if (storageModeIsSqliteOnly()) {
            return;
        }
    }
    std::vector<std::string> lines;
    lines.reserve(callSessions.size());
    for (const auto& pair : callSessions) {
        lines.push_back(pair.second.toJSON());
    }
    writeLinesToFile(kCallsFile, lines);
}

void persistStreamsToDiskUnlocked() {
    if (!storageModeIsJsonlOnly()) {
        if (persistStreamsToSqliteUnlocked()) {
            return;
        }
        if (storageModeIsSqliteOnly()) {
            return;
        }
    }
    std::vector<std::string> lines;
    lines.reserve(liveStreams.size());
    for (const auto& pair : liveStreams) {
        lines.push_back(pair.second.toJSON());
    }
    writeLinesToFile(kStreamsFile, lines);
}

void persistRecordingsToDiskUnlocked() {
    if (!storageModeIsJsonlOnly()) {
        if (persistRecordingsToSqliteUnlocked()) {
            return;
        }
        if (storageModeIsSqliteOnly()) {
            return;
        }
    }
    std::vector<std::string> lines;
    lines.reserve(screenRecordings.size());
    for (const auto& pair : screenRecordings) {
        lines.push_back(pair.second.toJSON());
    }
    writeLinesToFile(kRecordingsFile, lines);
}

void loadMessagesFromDisk() {
    if (!storageModeIsJsonlOnly()) {
        if (loadMessagesFromSqlite()) {
            if (!messages.empty() || storageModeIsSqliteOnly()) {
                return;
            }
            // If sqlite is empty but legacy JSONL exists, migrate it.
            auto legacyLines = readLinesFromFile(kMessagesFile);
            if (legacyLines.empty()) {
                return;
            }
            messages.clear();
            for (const auto& line : legacyLines) {
                Message msg;
                msg.id = extractJSONValue(line, "id");
                msg.username = extractJSONValue(line, "username");
                msg.content = extractJSONValue(line, "content");
                msg.timestamp = extractJSONValue(line, "timestamp");
                msg.roomId = extractJSONValue(line, "roomId");
                if (!msg.id.empty()) {
                    messages.push_back(msg);
                }
            }
            if (!messages.empty()) {
                persistMessagesToSqliteUnlocked();
            }
            return;
        }
        if (storageModeIsSqliteOnly()) {
            messages.clear();
            return;
        }
    }

    messages.clear();
    auto lines = readLinesFromFile(kMessagesFile);
    for (const auto& line : lines) {
        Message msg;
        msg.id = extractJSONValue(line, "id");
        msg.username = extractJSONValue(line, "username");
        msg.content = extractJSONValue(line, "content");
        msg.timestamp = extractJSONValue(line, "timestamp");
        msg.roomId = extractJSONValue(line, "roomId");
        if (!msg.id.empty()) {
            messages.push_back(msg);
        }
    }
}

void loadCallsFromDisk() {
    if (!storageModeIsJsonlOnly()) {
        if (loadCallsFromSqlite()) {
            if (!callSessions.empty() || storageModeIsSqliteOnly()) {
                return;
            }
            auto legacyLines = readLinesFromFile(kCallsFile);
            if (legacyLines.empty()) {
                return;
            }
            callSessions.clear();
            for (const auto& line : legacyLines) {
                CallSession call;
                call.id = extractJSONValue(line, "id");
                call.type = extractJSONValue(line, "type");
                call.callerId = extractJSONValue(line, "callerId");
                call.callerName = extractJSONValue(line, "callerName");
                call.receiverId = extractJSONValue(line, "receiverId");
                call.receiverName = extractJSONValue(line, "receiverName");
                call.status = extractJSONValue(line, "status");
                call.startTime = extractJSONValue(line, "startTime");
                call.endTime = extractJSONValue(line, "endTime");
                if (!call.id.empty()) {
                    callSessions[call.id] = call;
                }
            }
            if (!callSessions.empty()) {
                persistCallsToSqliteUnlocked();
            }
            return;
        }
        if (storageModeIsSqliteOnly()) {
            callSessions.clear();
            return;
        }
    }

    callSessions.clear();
    auto lines = readLinesFromFile(kCallsFile);
    for (const auto& line : lines) {
        CallSession call;
        call.id = extractJSONValue(line, "id");
        call.type = extractJSONValue(line, "type");
        call.callerId = extractJSONValue(line, "callerId");
        call.callerName = extractJSONValue(line, "callerName");
        call.receiverId = extractJSONValue(line, "receiverId");
        call.receiverName = extractJSONValue(line, "receiverName");
        call.status = extractJSONValue(line, "status");
        call.startTime = extractJSONValue(line, "startTime");
        call.endTime = extractJSONValue(line, "endTime");
        if (!call.id.empty()) {
            callSessions[call.id] = call;
        }
    }
}

void loadStreamsFromDisk() {
    if (!storageModeIsJsonlOnly()) {
        if (loadStreamsFromSqlite()) {
            if (!liveStreams.empty() || storageModeIsSqliteOnly()) {
                return;
            }
            auto legacyLines = readLinesFromFile(kStreamsFile);
            if (legacyLines.empty()) {
                return;
            }
            liveStreams.clear();
            for (const auto& line : legacyLines) {
                LiveStream stream;
                stream.id = extractJSONValue(line, "id");
                stream.streamerId = extractJSONValue(line, "streamerId");
                stream.streamerName = extractJSONValue(line, "streamerName");
                stream.title = extractJSONValue(line, "title");
                stream.description = extractJSONValue(line, "description");
                stream.status = extractJSONValue(line, "status");
                stream.startTime = extractJSONValue(line, "startTime");
                stream.endTime = extractJSONValue(line, "endTime");
                stream.streamKey = extractJSONValue(line, "streamKey");
                stream.viewerCount = extractJSONIntValue(line, "viewerCount", 0);
                if (!stream.id.empty()) {
                    liveStreams[stream.id] = stream;
                }
            }
            if (!liveStreams.empty()) {
                persistStreamsToSqliteUnlocked();
            }
            return;
        }
        if (storageModeIsSqliteOnly()) {
            liveStreams.clear();
            return;
        }
    }

    liveStreams.clear();
    auto lines = readLinesFromFile(kStreamsFile);
    for (const auto& line : lines) {
        LiveStream stream;
        stream.id = extractJSONValue(line, "id");
        stream.streamerId = extractJSONValue(line, "streamerId");
        stream.streamerName = extractJSONValue(line, "streamerName");
        stream.title = extractJSONValue(line, "title");
        stream.description = extractJSONValue(line, "description");
        stream.status = extractJSONValue(line, "status");
        stream.startTime = extractJSONValue(line, "startTime");
        stream.endTime = extractJSONValue(line, "endTime");
        stream.streamKey = extractJSONValue(line, "streamKey");
        stream.viewerCount = extractJSONIntValue(line, "viewerCount", 0);
        if (!stream.id.empty()) {
            liveStreams[stream.id] = stream;
        }
    }
}

void loadRecordingsFromDisk() {
    if (!storageModeIsJsonlOnly()) {
        if (loadRecordingsFromSqlite()) {
            if (!screenRecordings.empty() || storageModeIsSqliteOnly()) {
                return;
            }
            auto legacyLines = readLinesFromFile(kRecordingsFile);
            if (legacyLines.empty()) {
                return;
            }
            screenRecordings.clear();
            for (const auto& line : legacyLines) {
                ScreenRecording rec;
                rec.id = extractJSONValue(line, "id");
                rec.userId = extractJSONValue(line, "userId");
                rec.userName = extractJSONValue(line, "userName");
                rec.filename = extractJSONValue(line, "filename");
                rec.quality = extractJSONValue(line, "quality");
                rec.captureType = extractJSONValue(line, "captureType");
                rec.status = extractJSONValue(line, "status");
                rec.startTime = extractJSONValue(line, "startTime");
                rec.endTime = extractJSONValue(line, "endTime");
                rec.duration = extractJSONIntValue(line, "duration", 0);
                if (!rec.id.empty()) {
                    screenRecordings[rec.id] = rec;
                }
            }
            if (!screenRecordings.empty()) {
                persistRecordingsToSqliteUnlocked();
            }
            return;
        }
        if (storageModeIsSqliteOnly()) {
            screenRecordings.clear();
            return;
        }
    }

    screenRecordings.clear();
    auto lines = readLinesFromFile(kRecordingsFile);
    for (const auto& line : lines) {
        ScreenRecording rec;
        rec.id = extractJSONValue(line, "id");
        rec.userId = extractJSONValue(line, "userId");
        rec.userName = extractJSONValue(line, "userName");
        rec.filename = extractJSONValue(line, "filename");
        rec.quality = extractJSONValue(line, "quality");
        rec.captureType = extractJSONValue(line, "captureType");
        rec.status = extractJSONValue(line, "status");
        rec.startTime = extractJSONValue(line, "startTime");
        rec.endTime = extractJSONValue(line, "endTime");
        rec.duration = extractJSONIntValue(line, "duration", 0);
        if (!rec.id.empty()) {
            screenRecordings[rec.id] = rec;
        }
    }
}

void loadPersistedData() {
    ensureDataDirectory();
    if (!storageModeIsJsonlOnly()) {
        initSqliteStorage();
    }
    loadMessagesFromDisk();
    loadCallsFromDisk();
    loadStreamsFromDisk();
    loadRecordingsFromDisk();
}

int getEnvInt(const char* name, int defaultValue) {
    const char* raw = std::getenv(name);
    if (!raw || !*raw) return defaultValue;
    try {
        return std::stoi(raw);
    } catch (...) {
        return defaultValue;
    }
}

std::string getEnvString(const char* name, const std::string& defaultValue = "") {
    const char* raw = std::getenv(name);
    if (!raw) return defaultValue;
    return std::string(raw);
}

std::string getEnvStringWithFallback(const char* primary, const char* fallback, const std::string& defaultValue = "") {
    std::string primaryValue = getEnvString(primary, "");
    if (!primaryValue.empty()) {
        return primaryValue;
    }
    return getEnvString(fallback, defaultValue);
}

int getEnvIntWithFallback(const char* primary, const char* fallback, int defaultValue) {
    const std::string primaryRaw = getEnvString(primary, "");
    if (!primaryRaw.empty()) {
        try {
            return std::stoi(primaryRaw);
        } catch (...) {
            return defaultValue;
        }
    }
    return getEnvInt(fallback, defaultValue);
}

std::string extractBearerToken(const std::string& authHeader) {
    const std::string prefix = "Bearer ";
    if (authHeader.size() < prefix.size()) {
        return "";
    }
    if (authHeader.rfind(prefix, 0) != 0) {
        return "";
    }
    return authHeader.substr(prefix.size());
}

std::string escapeJSONString(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string getHeaderValue(const std::map<std::string, std::string>& headers, const std::string& key) {
    const std::string keyLower = toLower(key);
    for (const auto& h : headers) {
        if (toLower(h.first) == keyLower) {
            return h.second;
        }
    }
    return "";
}

std::string getPathOnly(const std::string& path) {
    size_t queryPos = path.find('?');
    return (queryPos == std::string::npos) ? path : path.substr(0, queryPos);
}

std::string urlDecode(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+' ) {
            out.push_back(' ');
            continue;
        }
        if (value[i] == '%' && i + 2 < value.size()) {
            std::string hex = value.substr(i + 1, 2);
            char ch = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
            out.push_back(ch);
            i += 2;
            continue;
        }
        out.push_back(value[i]);
    }
    return out;
}

std::string getQueryParam(const std::string& path, const std::string& key) {
    size_t queryPos = path.find('?');
    if (queryPos == std::string::npos || queryPos + 1 >= path.size()) {
        return "";
    }

    std::string query = path.substr(queryPos + 1);
    std::stringstream ss(query);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
        size_t eq = pair.find('=');
        std::string k = (eq == std::string::npos) ? pair : pair.substr(0, eq);
        std::string v = (eq == std::string::npos) ? "" : pair.substr(eq + 1);
        if (k == key) {
            return urlDecode(v);
        }
    }
    return "";
}

bool sendAll(int socket, const char* data, size_t length) {
    size_t totalSent = 0;
    while (totalSent < length) {
        #ifdef _WIN32
        int sent = send(socket, data + totalSent, static_cast<int>(length - totalSent), 0);
        #else
        int sent = write(socket, data + totalSent, length - totalSent);
        #endif
        if (sent <= 0) {
            return false;
        }
        totalSent += static_cast<size_t>(sent);
    }
    return true;
}

bool recvAll(int socket, char* data, size_t length) {
    size_t totalRead = 0;
    while (totalRead < length) {
        #ifdef _WIN32
        int readLen = recv(socket, data + totalRead, static_cast<int>(length - totalRead), 0);
        #else
        int readLen = read(socket, data + totalRead, length - totalRead);
        #endif
        if (readLen <= 0) {
            return false;
        }
        totalRead += static_cast<size_t>(readLen);
    }
    return true;
}

void closeClientSocket(int socket) {
    #ifdef _WIN32
    closesocket(socket);
    #else
    close(socket);
    #endif
}

int connectTcpSocket(const std::string& host, int port) {
    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    addrinfo* result = nullptr;
    const std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result) != 0) {
        return -1;
    }

    int sock = -1;
    for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        int candidate = static_cast<int>(socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol));
        if (candidate < 0) {
            continue;
        }
        if (connect(candidate, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == 0) {
            sock = candidate;
            break;
        }
        closeClientSocket(candidate);
    }

    freeaddrinfo(result);
    return sock;
}

bool readRedisLine(int socket, std::string& outLine) {
    outLine.clear();
    while (true) {
        char ch = 0;
        if (!recvAll(socket, &ch, 1)) {
            return false;
        }
        if (ch == '\r') {
            char lf = 0;
            if (!recvAll(socket, &lf, 1)) {
                return false;
            }
            if (lf != '\n') {
                return false;
            }
            return true;
        }
        outLine.push_back(ch);
        if (outLine.size() > (1024 * 1024)) {
            return false;
        }
    }
}

bool readRedisArrayReply(int socket, std::vector<std::string>& items) {
    items.clear();

    char prefix = 0;
    if (!recvAll(socket, &prefix, 1)) {
        return false;
    }
    if (prefix == '-') {
        std::string err;
        if (readRedisLine(socket, err)) {
            std::cerr << "[event-bus] redis error: " << err << std::endl;
        }
        return false;
    }
    if (prefix != '*') {
        std::string line;
        (void)readRedisLine(socket, line);
        return false;
    }

    std::string countLine;
    if (!readRedisLine(socket, countLine)) {
        return false;
    }
    int count = 0;
    try {
        count = std::stoi(countLine);
    } catch (...) {
        return false;
    }
    if (count < 0) {
        return false;
    }

    items.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        char tokenType = 0;
        if (!recvAll(socket, &tokenType, 1)) {
            return false;
        }

        if (tokenType == '$') {
            std::string lenLine;
            if (!readRedisLine(socket, lenLine)) {
                return false;
            }
            int len = 0;
            try {
                len = std::stoi(lenLine);
            } catch (...) {
                return false;
            }
            if (len < 0) {
                items.push_back("");
                continue;
            }

            std::string value(static_cast<size_t>(len), '\0');
            if (len > 0 && !recvAll(socket, value.data(), static_cast<size_t>(len))) {
                return false;
            }
            char crlf[2];
            if (!recvAll(socket, crlf, 2)) {
                return false;
            }
            if (crlf[0] != '\r' || crlf[1] != '\n') {
                return false;
            }
            items.push_back(value);
            continue;
        }

        std::string line;
        if (!readRedisLine(socket, line)) {
            return false;
        }
        if (tokenType == '-') {
            std::cerr << "[event-bus] redis error: " << line << std::endl;
            return false;
        }
        items.push_back(line);
    }

    return true;
}

bool sendRedisCommand(int socket, const std::vector<std::string>& args) {
    std::stringstream ss;
    ss << "*" << args.size() << "\r\n";
    for (const auto& arg : args) {
        ss << "$" << arg.size() << "\r\n";
        ss << arg << "\r\n";
    }
    const std::string cmd = ss.str();
    return sendAll(socket, cmd.data(), cmd.size());
}

bool readRedisGenericReply(int socket) {
    char prefix = 0;
    if (!recvAll(socket, &prefix, 1)) {
        return false;
    }

    std::string line;
    if (!readRedisLine(socket, line)) {
        return false;
    }
    if (prefix == '-') {
        std::cerr << "[event-bus] redis error: " << line << std::endl;
        return false;
    }
    return true;
}

std::array<uint8_t, 20> sha1(const std::string& input) {
    auto leftrotate = [](uint32_t value, int bits) {
        return static_cast<uint32_t>((value << bits) | (value >> (32 - bits)));
    };

    std::vector<uint8_t> bytes(input.begin(), input.end());
    uint64_t bitLen = static_cast<uint64_t>(bytes.size()) * 8;

    bytes.push_back(0x80);
    while ((bytes.size() % 64) != 56) {
        bytes.push_back(0x00);
    }

    for (int i = 7; i >= 0; --i) {
        bytes.push_back(static_cast<uint8_t>((bitLen >> (i * 8)) & 0xff));
    }

    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    for (size_t chunk = 0; chunk < bytes.size(); chunk += 64) {
        uint32_t w[80] = {0};
        for (int i = 0; i < 16; ++i) {
            size_t offset = chunk + (i * 4);
            w[i] = (static_cast<uint32_t>(bytes[offset]) << 24) |
                   (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
                   (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
                   static_cast<uint32_t>(bytes[offset + 3]);
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = leftrotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        uint32_t a = h0;
        uint32_t b = h1;
        uint32_t c = h2;
        uint32_t d = h3;
        uint32_t e = h4;

        for (int i = 0; i < 80; ++i) {
            uint32_t f;
            uint32_t k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32_t temp = leftrotate(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = leftrotate(b, 30);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    return {
        static_cast<uint8_t>((h0 >> 24) & 0xff), static_cast<uint8_t>((h0 >> 16) & 0xff),
        static_cast<uint8_t>((h0 >> 8) & 0xff), static_cast<uint8_t>(h0 & 0xff),
        static_cast<uint8_t>((h1 >> 24) & 0xff), static_cast<uint8_t>((h1 >> 16) & 0xff),
        static_cast<uint8_t>((h1 >> 8) & 0xff), static_cast<uint8_t>(h1 & 0xff),
        static_cast<uint8_t>((h2 >> 24) & 0xff), static_cast<uint8_t>((h2 >> 16) & 0xff),
        static_cast<uint8_t>((h2 >> 8) & 0xff), static_cast<uint8_t>(h2 & 0xff),
        static_cast<uint8_t>((h3 >> 24) & 0xff), static_cast<uint8_t>((h3 >> 16) & 0xff),
        static_cast<uint8_t>((h3 >> 8) & 0xff), static_cast<uint8_t>(h3 & 0xff),
        static_cast<uint8_t>((h4 >> 24) & 0xff), static_cast<uint8_t>((h4 >> 16) & 0xff),
        static_cast<uint8_t>((h4 >> 8) & 0xff), static_cast<uint8_t>(h4 & 0xff)
    };
}

std::string base64Encode(const uint8_t* data, size_t len) {
    static const char* kBase64Chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t triple = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) triple |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) triple |= static_cast<uint32_t>(data[i + 2]);

        out.push_back(kBase64Chars[(triple >> 18) & 0x3f]);
        out.push_back(kBase64Chars[(triple >> 12) & 0x3f]);
        out.push_back((i + 1 < len) ? kBase64Chars[(triple >> 6) & 0x3f] : '=');
        out.push_back((i + 2 < len) ? kBase64Chars[triple & 0x3f] : '=');
    }
    return out;
}

std::string createWebSocketAcceptKey(const std::string& clientKey) {
    const std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = clientKey + magic;
    auto digest = sha1(combined);
    return base64Encode(digest.data(), digest.size());
}

bool sendWebSocketFrame(int socket, uint8_t opcode, const std::string& payload) {
    std::string frame;
    frame.push_back(static_cast<char>(0x80 | (opcode & 0x0f)));

    const uint64_t payloadLen = payload.size();
    if (payloadLen <= 125) {
        frame.push_back(static_cast<char>(payloadLen));
    } else if (payloadLen <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<char>((payloadLen >> 8) & 0xff));
        frame.push_back(static_cast<char>(payloadLen & 0xff));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<char>((payloadLen >> (i * 8)) & 0xff));
        }
    }

    frame += payload;
    return sendAll(socket, frame.data(), frame.size());
}

bool readWebSocketFrame(int socket, uint8_t& opcode, std::string& payload) {
    char header[2];
    if (!recvAll(socket, header, 2)) {
        return false;
    }

    uint8_t b0 = static_cast<uint8_t>(header[0]);
    uint8_t b1 = static_cast<uint8_t>(header[1]);
    opcode = static_cast<uint8_t>(b0 & 0x0f);
    bool masked = (b1 & 0x80) != 0;
    uint64_t payloadLen = b1 & 0x7f;

    if (payloadLen == 126) {
        char ext[2];
        if (!recvAll(socket, ext, 2)) return false;
        payloadLen = (static_cast<uint8_t>(ext[0]) << 8) | static_cast<uint8_t>(ext[1]);
    } else if (payloadLen == 127) {
        char ext[8];
        if (!recvAll(socket, ext, 8)) return false;
        payloadLen = 0;
        for (int i = 0; i < 8; ++i) {
            payloadLen = (payloadLen << 8) | static_cast<uint8_t>(ext[i]);
        }
    }

    if (payloadLen > (1024 * 1024)) {
        return false;
    }

    uint8_t mask[4] = {0, 0, 0, 0};
    if (masked) {
        if (!recvAll(socket, reinterpret_cast<char*>(mask), 4)) return false;
    }

    payload.assign(payloadLen, '\0');
    if (payloadLen > 0 && !recvAll(socket, payload.data(), static_cast<size_t>(payloadLen))) {
        return false;
    }

    if (masked) {
        for (size_t i = 0; i < payload.size(); ++i) {
            payload[i] = static_cast<char>(payload[i] ^ mask[i % 4]);
        }
    }
    return true;
}

void removeWebSocketClient(int socket) {
    std::lock_guard<std::mutex> lock(ws_mutex);
    wsClients.erase(
        std::remove_if(wsClients.begin(), wsClients.end(), [socket](const WebSocketClient& c) {
            return c.socket == socket;
        }),
        wsClients.end()
    );
}

std::string buildEventFramePayload(const std::string& type, const std::string& payloadJson, const std::string& roomId = "") {
    std::stringstream ss;
    ss << "{"
       << "\"type\":\"" << escapeJSONString(type) << "\","
       << "\"roomId\":\"" << escapeJSONString(roomId) << "\","
       << "\"timestamp\":\"" << escapeJSONString(getCurrentTimestamp()) << "\","
       << "\"payload\":" << (payloadJson.empty() ? "{}" : payloadJson)
       << "}";
    return ss.str();
}

void broadcastFrameLocal(const std::string& framePayload, const std::string& roomId = "") {
    std::vector<WebSocketClient> snapshot;
    {
        std::lock_guard<std::mutex> lock(ws_mutex);
        snapshot = wsClients;
    }

    std::vector<int> deadSockets;
    for (const auto& client : snapshot) {
        if (!roomId.empty() && !client.roomId.empty() && client.roomId != roomId) {
            continue;
        }
        if (!sendWebSocketFrame(client.socket, 0x1, framePayload)) {
            deadSockets.push_back(client.socket);
        }
    }

    if (!deadSockets.empty()) {
        std::lock_guard<std::mutex> lock(ws_mutex);
        wsClients.erase(
            std::remove_if(wsClients.begin(), wsClients.end(), [&deadSockets](const WebSocketClient& c) {
                return std::find(deadSockets.begin(), deadSockets.end(), c.socket) != deadSockets.end();
            }),
            wsClients.end()
        );
    }
}

void broadcastEventLocal(const std::string& type, const std::string& payloadJson, const std::string& roomId = "") {
    const std::string framePayload = buildEventFramePayload(type, payloadJson, roomId);
    broadcastFrameLocal(framePayload, roomId);
}

bool publishEventToRedis(const std::string& framePayload) {
    const std::string host = detectRedisHost();
    const int port = detectRedisPort();
    const std::string password = detectRedisPassword();
    const std::string channel = detectEventBusChannel();

    int redisSocket = connectTcpSocket(host, port);
    if (redisSocket < 0) {
        return false;
    }

    bool ok = true;
    if (!password.empty()) {
        ok = sendRedisCommand(redisSocket, {"AUTH", password}) && readRedisGenericReply(redisSocket);
    }
    if (ok) {
        ok = sendRedisCommand(redisSocket, {"PUBLISH", channel, framePayload}) && readRedisGenericReply(redisSocket);
    }

    closeClientSocket(redisSocket);
    return ok;
}

void runRedisSubscriberLoop() {
    const std::string host = detectRedisHost();
    const int port = detectRedisPort();
    const std::string password = detectRedisPassword();
    const std::string channel = detectEventBusChannel();

    while (true) {
        int redisSocket = connectTcpSocket(host, port);
        if (redisSocket < 0) {
            if (!g_eventBusRedisWarned.exchange(true)) {
                std::cerr << "[event-bus] redis connect failed (" << host << ":" << port << "), retrying..." << std::endl;
            }
            g_eventBusRedisReady.store(false);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        bool ready = true;
        if (!password.empty()) {
            ready = sendRedisCommand(redisSocket, {"AUTH", password}) && readRedisGenericReply(redisSocket);
        }
        if (ready) {
            ready = sendRedisCommand(redisSocket, {"SUBSCRIBE", channel});
        }

        if (!ready) {
            closeClientSocket(redisSocket);
            g_eventBusRedisReady.store(false);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        std::vector<std::string> reply;
        if (!readRedisArrayReply(redisSocket, reply)) {
            closeClientSocket(redisSocket);
            g_eventBusRedisReady.store(false);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        g_eventBusRedisWarned.store(false);
        g_eventBusRedisReady.store(true);
        g_eventBusRedisPublishWarned.store(false);

        while (true) {
            std::vector<std::string> messageParts;
            if (!readRedisArrayReply(redisSocket, messageParts)) {
                break;
            }
            if (messageParts.size() >= 3 && messageParts[0] == "message") {
                const std::string& framePayload = messageParts[2];
                const std::string roomId = extractJSONValue(framePayload, "roomId");
                broadcastFrameLocal(framePayload, roomId);
            }
        }

        closeClientSocket(redisSocket);
        g_eventBusRedisReady.store(false);
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void ensureRedisEventBusThreadStarted() {
    if (!eventBusModeIsRedis()) {
        return;
    }
    bool expected = false;
    if (!g_eventBusRedisStarted.compare_exchange_strong(expected, true)) {
        return;
    }
    std::thread subscriberThread(runRedisSubscriberLoop);
    subscriberThread.detach();
}

void dispatchEvent(const std::string& type, const std::string& payloadJson, const std::string& roomId = "") {
    if (eventBusModeIsRedis()) {
        ensureRedisEventBusThreadStarted();
        const std::string framePayload = buildEventFramePayload(type, payloadJson, roomId);
        if (publishEventToRedis(framePayload)) {
            return;
        }
        if (!g_eventBusRedisPublishWarned.exchange(true)) {
            std::cerr << "[event-bus] redis publish failed, using local fallback" << std::endl;
        }
        broadcastFrameLocal(framePayload, roomId);
        return;
    }

    broadcastEventLocal(type, payloadJson, roomId);
}

void broadcastEvent(const std::string& type, const std::string& payloadJson, const std::string& roomId = "") {
    dispatchEvent(type, payloadJson, roomId);
}

// ============================================================================
// HTTP STRUCTURES
// ============================================================================

struct HTTPRequest {
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HTTPResponse {
    int status = 200;
    std::string statusText = "OK";
    std::map<std::string, std::string> headers;
    std::string body;
    
    std::string toString() const {
        std::stringstream ss;
        ss << "HTTP/1.1 " << status << " " << statusText << "\r\n";
        
        for (const auto& header : headers) {
            ss << header.first << ": " << header.second << "\r\n";
        }
        
        ss << "Content-Length: " << body.length() << "\r\n";
        ss << "Connection: close\r\n";
        ss << "\r\n";
        ss << body;
        
        return ss.str();
    }
};

HTTPRequest parseRequest(const std::string& requestStr) {
    HTTPRequest req;
    std::istringstream stream(requestStr);
    std::string line;
    
    if (std::getline(stream, line)) {
        line.erase(line.find_last_not_of("\r\n") + 1);
        std::istringstream lineStream(line);
        lineStream >> req.method >> req.path >> req.version;
    }
    
    while (std::getline(stream, line) && line != "\r") {
        line.erase(line.find_last_not_of("\r\n") + 1);
        if (line.empty()) break;
        
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            req.headers[key] = value;
        }
    }
    
    std::string bodyContent;
    while (std::getline(stream, line)) {
        bodyContent += line;
    }
    req.body = bodyContent;
    
    return req;
}

// ============================================================================
// API HANDLERS
// ============================================================================

HTTPResponse handleChatAPI(const HTTPRequest& req) {
    HTTPResponse res;
    const std::string routePath = getPathOnly(req.path);
    res.headers["Content-Type"] = "application/json";
    res.headers["Access-Control-Allow-Origin"] = "*";
    
    // GET /api/chat/messages
    if (req.method == "GET" && routePath == "/api/chat/messages") {
        std::string roomFilter = getQueryParam(req.path, "roomId");
        std::lock_guard<std::mutex> lock(messages_mutex);
        
        std::stringstream ss;
        ss << "[";
        bool first = true;
        for (const auto& msg : messages) {
            if (!roomFilter.empty() && msg.roomId != roomFilter) {
                continue;
            }
            if (!first) ss << ",";
            ss << msg.toJSON();
            first = false;
        }
        ss << "]";
        
        res.body = ss.str();
        return res;
    }
    
    // POST /api/chat/messages
    if (req.method == "POST" && routePath == "/api/chat/messages") {
        if (!hasJSONKey(req.body, "username") || !hasJSONKey(req.body, "content")) {
            res.status = 400;
            res.statusText = "Bad Request";
            res.body = "{\"error\":\"Missing required fields: username and content\"}";
            return res;
        }
        
        std::lock_guard<std::mutex> lock(messages_mutex);
        
        Message msg;
        msg.id = generateId("msg_");
        msg.username = extractJSONValue(req.body, "username");
        msg.content = extractJSONValue(req.body, "content");
        msg.roomId = extractJSONValue(req.body, "roomId");
        if (msg.roomId.empty()) {
            msg.roomId = "global";
        }
        msg.timestamp = getCurrentTimestamp();
        
        messages.push_back(msg);
        persistMessagesToDiskUnlocked();
        broadcastEvent("chat.message.created", msg.toJSON(), msg.roomId);
        
        res.status = 201;
        res.statusText = "Created";
        res.body = msg.toJSON();
        return res;
    }
    
    res.status = 404;
    res.statusText = "Not Found";
    res.body = "{\"error\":\"Chat endpoint not found\"}";
    return res;
}

HTTPResponse handleVoiceCallAPI(const HTTPRequest& req) {
    HTTPResponse res;
    res.headers["Content-Type"] = "application/json";
    res.headers["Access-Control-Allow-Origin"] = "*";
    
    // POST /api/voice/call - Initiate voice call
    if (req.method == "POST" && req.path == "/api/voice/call") {
        std::lock_guard<std::mutex> lock(calls_mutex);
        
        CallSession call;
        call.id = generateId("voice_");
        call.type = "voice";
        call.callerId = extractJSONValue(req.body, "callerId");
        call.callerName = extractJSONValue(req.body, "callerName");
        call.receiverId = extractJSONValue(req.body, "receiverId");
        call.receiverName = extractJSONValue(req.body, "receiverName");
        call.status = "ringing";
        call.startTime = getCurrentTimestamp();
        
        callSessions[call.id] = call;
        persistCallsToDiskUnlocked();
        broadcastEvent("voice.call.created", call.toJSON());
        
        res.status = 201;
        res.statusText = "Created";
        res.body = call.toJSON();
        return res;
    }
    
    // POST /api/voice/answer/:callId
    if (req.method == "POST" && req.path.find("/api/voice/answer/") == 0) {
        std::string callId = req.path.substr(18);
        
        std::lock_guard<std::mutex> lock(calls_mutex);
        
        auto it = callSessions.find(callId);
        if (it != callSessions.end()) {
            it->second.status = "active";
            it->second.answer = extractJSONValue(req.body, "answer");
            persistCallsToDiskUnlocked();
            broadcastEvent("voice.call.answered", it->second.toJSON());
            
            res.body = it->second.toJSON();
            return res;
        }
        
        res.status = 404;
        res.statusText = "Not Found";
        res.body = "{\"error\":\"Call not found\"}";
        return res;
    }
    
    // POST /api/voice/end/:callId
    if (req.method == "POST" && req.path.find("/api/voice/end/") == 0) {
        std::string callId = req.path.substr(15);
        
        std::lock_guard<std::mutex> lock(calls_mutex);
        
        auto it = callSessions.find(callId);
        if (it != callSessions.end()) {
            it->second.status = "ended";
            it->second.endTime = getCurrentTimestamp();
            persistCallsToDiskUnlocked();
            broadcastEvent("voice.call.ended", it->second.toJSON());
            
            res.body = it->second.toJSON();
            return res;
        }
        
        res.status = 404;
        res.statusText = "Not Found";
        res.body = "{\"error\":\"Call not found\"}";
        return res;
    }
    
    // GET /api/voice/active - Get all active calls
    if (req.method == "GET" && req.path == "/api/voice/active") {
        std::lock_guard<std::mutex> lock(calls_mutex);
        
        std::stringstream ss;
        ss << "[";
        bool first = true;
        for (const auto& pair : callSessions) {
            if (pair.second.type == "voice" && pair.second.status != "ended") {
                if (!first) ss << ",";
                ss << pair.second.toJSON();
                first = false;
            }
        }
        ss << "]";
        
        res.body = ss.str();
        return res;
    }
    
    res.status = 404;
    res.statusText = "Not Found";
    res.body = "{\"error\":\"Voice call endpoint not found\"}";
    return res;
}

HTTPResponse handleVideoCallAPI(const HTTPRequest& req) {
    HTTPResponse res;
    res.headers["Content-Type"] = "application/json";
    res.headers["Access-Control-Allow-Origin"] = "*";
    
    // POST /api/video/call - Initiate video call
    if (req.method == "POST" && req.path == "/api/video/call") {
        std::lock_guard<std::mutex> lock(calls_mutex);
        
        CallSession call;
        call.id = generateId("video_");
        call.type = "video";
        call.callerId = extractJSONValue(req.body, "callerId");
        call.callerName = extractJSONValue(req.body, "callerName");
        call.receiverId = extractJSONValue(req.body, "receiverId");
        call.receiverName = extractJSONValue(req.body, "receiverName");
        call.offer = extractJSONValue(req.body, "offer");
        call.status = "ringing";
        call.startTime = getCurrentTimestamp();
        
        callSessions[call.id] = call;
        persistCallsToDiskUnlocked();
        broadcastEvent("video.call.created", call.toJSON());
        
        res.status = 201;
        res.statusText = "Created";
        res.body = call.toJSON();
        return res;
    }
    
    // POST /api/video/answer/:callId
    if (req.method == "POST" && req.path.find("/api/video/answer/") == 0) {
        std::string callId = req.path.substr(18);
        
        std::lock_guard<std::mutex> lock(calls_mutex);
        
        auto it = callSessions.find(callId);
        if (it != callSessions.end()) {
            it->second.status = "active";
            it->second.answer = extractJSONValue(req.body, "answer");
            persistCallsToDiskUnlocked();
            broadcastEvent("video.call.answered", it->second.toJSON());
            
            res.body = it->second.toJSON();
            return res;
        }
        
        res.status = 404;
        res.statusText = "Not Found";
        res.body = "{\"error\":\"Call not found\"}";
        return res;
    }
    
    // POST /api/video/ice/:callId - Add ICE candidate
    if (req.method == "POST" && req.path.find("/api/video/ice/") == 0) {
        std::string callId = req.path.substr(15);
        
        std::lock_guard<std::mutex> lock(calls_mutex);
        
        auto it = callSessions.find(callId);
        if (it != callSessions.end()) {
            std::string candidate = extractJSONValue(req.body, "candidate");
            it->second.iceCandidates.push_back(candidate);
            broadcastEvent("video.call.ice", "{\"callId\":\"" + escapeJSONString(callId) + "\",\"candidate\":\"" + escapeJSONString(candidate) + "\"}");
            
            res.body = "{\"status\":\"ICE candidate added\"}";
            return res;
        }
        
        res.status = 404;
        res.statusText = "Not Found";
        res.body = "{\"error\":\"Call not found\"}";
        return res;
    }
    
    // POST /api/video/end/:callId
    if (req.method == "POST" && req.path.find("/api/video/end/") == 0) {
        std::string callId = req.path.substr(15);
        
        std::lock_guard<std::mutex> lock(calls_mutex);
        
        auto it = callSessions.find(callId);
        if (it != callSessions.end()) {
            it->second.status = "ended";
            it->second.endTime = getCurrentTimestamp();
            persistCallsToDiskUnlocked();
            broadcastEvent("video.call.ended", it->second.toJSON());
            
            res.body = it->second.toJSON();
            return res;
        }
        
        res.status = 404;
        res.statusText = "Not Found";
        res.body = "{\"error\":\"Call not found\"}";
        return res;
    }
    
    // GET /api/video/active
    if (req.method == "GET" && req.path == "/api/video/active") {
        std::lock_guard<std::mutex> lock(calls_mutex);
        
        std::stringstream ss;
        ss << "[";
        bool first = true;
        for (const auto& pair : callSessions) {
            if (pair.second.type == "video" && pair.second.status != "ended") {
                if (!first) ss << ",";
                ss << pair.second.toJSON();
                first = false;
            }
        }
        ss << "]";
        
        res.body = ss.str();
        return res;
    }
    
    res.status = 404;
    res.statusText = "Not Found";
    res.body = "{\"error\":\"Video call endpoint not found\"}";
    return res;
}

HTTPResponse handleLiveStreamAPI(const HTTPRequest& req) {
    HTTPResponse res;
    res.headers["Content-Type"] = "application/json";
    res.headers["Access-Control-Allow-Origin"] = "*";
    
    // POST /api/stream/start - Start live stream
    if (req.method == "POST" && req.path == "/api/stream/start") {
        std::lock_guard<std::mutex> lock(streams_mutex);
        
        LiveStream stream;
        stream.id = generateId("stream_");
        stream.streamerId = extractJSONValue(req.body, "streamerId");
        stream.streamerName = extractJSONValue(req.body, "streamerName");
        stream.title = extractJSONValue(req.body, "title");
        stream.description = extractJSONValue(req.body, "description");
        stream.streamKey = generateId("key_");
        stream.status = "live";
        stream.startTime = getCurrentTimestamp();
        stream.viewerCount = 0;
        
        liveStreams[stream.id] = stream;
        persistStreamsToDiskUnlocked();
        broadcastEvent("stream.started", stream.toJSON());
        
        res.status = 201;
        res.statusText = "Created";
        res.body = stream.toJSON();
        return res;
    }
    
    // GET /api/stream/live - Get all live streams
    if (req.method == "GET" && req.path == "/api/stream/live") {
        std::lock_guard<std::mutex> lock(streams_mutex);
        
        std::stringstream ss;
        ss << "[";
        bool first = true;
        for (const auto& pair : liveStreams) {
            if (pair.second.status == "live") {
                if (!first) ss << ",";
                ss << pair.second.toJSON();
                first = false;
            }
        }
        ss << "]";
        
        res.body = ss.str();
        return res;
    }
    
    // POST /api/stream/join/:streamId - Join a stream
    if (req.method == "POST" && req.path.find("/api/stream/join/") == 0) {
        std::string streamId = req.path.substr(17);
        
        std::lock_guard<std::mutex> lock(streams_mutex);
        
        auto it = liveStreams.find(streamId);
        if (it != liveStreams.end() && it->second.status == "live") {
            std::string viewerId = extractJSONValue(req.body, "viewerId");
            
            // Check if viewer is already in the stream
            auto& viewers = it->second.viewers;
            if (std::find(viewers.begin(), viewers.end(), viewerId) == viewers.end()) {
                // Only add if not already watching
                viewers.push_back(viewerId);
                it->second.viewerCount = viewers.size();
            }
            persistStreamsToDiskUnlocked();
            broadcastEvent("stream.viewer.joined", it->second.toJSON());
            
            res.body = it->second.toJSON();
            return res;
        }
        
        res.status = 404;
        res.statusText = "Not Found";
        res.body = "{\"error\":\"Stream not found or not live\"}";
        return res;
    }
    
    // POST /api/stream/leave/:streamId - Leave a stream
    if (req.method == "POST" && req.path.find("/api/stream/leave/") == 0) {
        std::string streamId = req.path.substr(18);
        
        std::lock_guard<std::mutex> lock(streams_mutex);
        
        auto it = liveStreams.find(streamId);
        if (it != liveStreams.end()) {
            std::string viewerId = extractJSONValue(req.body, "viewerId");
            auto& viewers = it->second.viewers;
            viewers.erase(std::remove(viewers.begin(), viewers.end(), viewerId), viewers.end());
            it->second.viewerCount = viewers.size();
            persistStreamsToDiskUnlocked();
            broadcastEvent("stream.viewer.left", it->second.toJSON());
            
            res.body = it->second.toJSON();
            return res;
        }
        
        res.status = 404;
        res.statusText = "Not Found";
        res.body = "{\"error\":\"Stream not found\"}";
        return res;
    }
    
    // POST /api/stream/end/:streamId - End stream
    if (req.method == "POST" && req.path.find("/api/stream/end/") == 0) {
        std::string streamId = req.path.substr(16);
        
        std::lock_guard<std::mutex> lock(streams_mutex);
        
        auto it = liveStreams.find(streamId);
        if (it != liveStreams.end()) {
            it->second.status = "ended";
            it->second.endTime = getCurrentTimestamp();
            persistStreamsToDiskUnlocked();
            broadcastEvent("stream.ended", it->second.toJSON());
            
            res.body = it->second.toJSON();
            return res;
        }
        
        res.status = 404;
        res.statusText = "Not Found";
        res.body = "{\"error\":\"Stream not found\"}";
        return res;
    }
    
    // GET /api/stream/:streamId - Get stream details
    if (req.method == "GET" && req.path.find("/api/stream/") == 0) {
        std::string streamId = req.path.substr(12);
        
        std::lock_guard<std::mutex> lock(streams_mutex);
        
        auto it = liveStreams.find(streamId);
        if (it != liveStreams.end()) {
            res.body = it->second.toJSON();
            return res;
        }
        
        res.status = 404;
        res.statusText = "Not Found";
        res.body = "{\"error\":\"Stream not found\"}";
        return res;
    }
    
    res.status = 404;
    res.statusText = "Not Found";
    res.body = "{\"error\":\"Live stream endpoint not found\"}";
    return res;
}

HTTPResponse handleScreenRecordingAPI(const HTTPRequest& req) {
    HTTPResponse res;
    res.headers["Content-Type"] = "application/json";
    res.headers["Access-Control-Allow-Origin"] = "*";
    
    // POST /api/recording/start - Start screen recording
    if (req.method == "POST" && req.path == "/api/recording/start") {
        std::lock_guard<std::mutex> lock(recordings_mutex);
        
        ScreenRecording recording;
        recording.id = generateId("rec_");
        recording.userId = extractJSONValue(req.body, "userId");
        recording.userName = extractJSONValue(req.body, "userName");
        recording.filename = extractJSONValue(req.body, "filename");
        recording.quality = extractJSONValue(req.body, "quality");
        recording.captureType = extractJSONValue(req.body, "captureType");
        recording.status = "recording";
        recording.startTime = getCurrentTimestamp();
        recording.duration = 0;
        
        screenRecordings[recording.id] = recording;
        persistRecordingsToDiskUnlocked();
        broadcastEvent("recording.started", recording.toJSON());
        
        res.status = 201;
        res.statusText = "Created";
        res.body = recording.toJSON();
        return res;
    }
    
    // POST /api/recording/stop/:recordingId - Stop recording
    if (req.method == "POST" && req.path.find("/api/recording/stop/") == 0) {
        std::string prefix = "/api/recording/stop/";
        std::string recordingId = req.path.substr(prefix.length());
        
        // Remove any trailing characters (query params, spaces, etc.)
        size_t queryPos = recordingId.find('?');
        if (queryPos != std::string::npos) {
            recordingId = recordingId.substr(0, queryPos);
        }
        
        // Trim whitespace
        recordingId.erase(recordingId.find_last_not_of(" \t\r\n") + 1);
        recordingId.erase(0, recordingId.find_first_not_of(" \t\r\n"));
        
        std::lock_guard<std::mutex> lock(recordings_mutex);
        
        auto it = screenRecordings.find(recordingId);
        if (it != screenRecordings.end()) {
            it->second.status = "stopped";
            it->second.endTime = getCurrentTimestamp();
            
            // Extract duration from request if provided
            std::string durationStr = extractJSONValue(req.body, "duration");
            if (!durationStr.empty()) {
                it->second.duration = std::stoi(durationStr);
            }
            persistRecordingsToDiskUnlocked();
            broadcastEvent("recording.stopped", it->second.toJSON());
            
            res.body = it->second.toJSON();
            return res;
        }
        
        res.status = 404;
        res.statusText = "Not Found";
        res.body = "{\"error\":\"Recording not found\"}";
        return res;
    }
    
    // GET /api/recording/list - Get all recordings
    if (req.method == "GET" && req.path == "/api/recording/list") {
        std::lock_guard<std::mutex> lock(recordings_mutex);
        
        std::stringstream ss;
        ss << "[";
        bool first = true;
        for (const auto& pair : screenRecordings) {
            if (!first) ss << ",";
            ss << pair.second.toJSON();
            first = false;
        }
        ss << "]";
        
        res.body = ss.str();
        return res;
    }
    
    // GET /api/recording/:recordingId - Get recording details
    if (req.method == "GET" && req.path.find("/api/recording/") == 0 && 
        req.path.find("/list") == std::string::npos &&
        req.path.find("/start") == std::string::npos &&
        req.path.find("/stop") == std::string::npos) {
        std::string recordingId = req.path.substr(15);
        
        std::lock_guard<std::mutex> lock(recordings_mutex);
        
        auto it = screenRecordings.find(recordingId);
        if (it != screenRecordings.end()) {
            res.body = it->second.toJSON();
            return res;
        }
        
        res.status = 404;
        res.statusText = "Not Found";
        res.body = "{\"error\":\"Recording not found\"}";
        return res;
    }
    
    res.status = 404;
    res.statusText = "Not Found";
    res.body = "{\"error\":\"Screen recording endpoint not found\"}";
    return res;
}

// ============================================================================
// MAIN REQUEST HANDLER
// ============================================================================

std::string readFileContent(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool isWebSocketUpgradeRequest(const HTTPRequest& req) {
    if (req.method != "GET") return false;
    if (getPathOnly(req.path) != "/ws") return false;

    std::string upgrade = toLower(getHeaderValue(req.headers, "Upgrade"));
    std::string connection = toLower(getHeaderValue(req.headers, "Connection"));
    return upgrade == "websocket" && connection.find("upgrade") != std::string::npos;
}

bool isWebSocketTokenValid(const HTTPRequest& req) {
    static const std::string requiredToken = getEnvStringWithFallback("GIGACHAD_WS_TOKEN", "MEDIA_WS_TOKEN", "");
    if (requiredToken.empty()) {
        return true;
    }

    std::string provided = getQueryParam(req.path, "token");
    if (provided.empty()) {
        provided = getHeaderValue(req.headers, "X-WS-Token");
    }
    return provided == requiredToken;
}

bool consumeWebSocketRateLimit(const std::string& clientIp) {
    static const int windowSeconds = getEnvIntWithFallback("GIGACHAD_WS_RATE_WINDOW_SEC", "MEDIA_WS_RATE_WINDOW_SEC", 60);
    static const int maxAttempts = getEnvIntWithFallback("GIGACHAD_WS_RATE_MAX_ATTEMPTS", "MEDIA_WS_RATE_MAX_ATTEMPTS", 20);

    const auto now = std::chrono::steady_clock::now();
    const auto cutoff = now - std::chrono::seconds(windowSeconds);

    std::lock_guard<std::mutex> lock(ws_rate_mutex);
    auto& attempts = wsConnectionAttempts[clientIp];
    while (!attempts.empty() && attempts.front() < cutoff) {
        attempts.pop_front();
    }

    if (static_cast<int>(attempts.size()) >= maxAttempts) {
        return false;
    }

    attempts.push_back(now);
    return true;
}

void sendHttpErrorResponse(int clientSocket, int code, const std::string& status, const std::string& message) {
    std::stringstream ss;
    ss << "HTTP/1.1 " << code << " " << status << "\r\n"
       << "Content-Type: application/json\r\n"
       << "Connection: close\r\n"
       << "Content-Length: " << message.size() << "\r\n"
       << "\r\n"
       << message;
    std::string payload = ss.str();
    sendAll(clientSocket, payload.c_str(), payload.size());
}

bool isPublicApiPath(const std::string& routePath) {
    return routePath == "/api" || routePath == "/api/health";
}

bool isApiTokenProtectionEnabled() {
    static const std::string baseToken = getEnvStringWithFallback("GIGACHAD_API_TOKEN", "MEDIA_API_TOKEN", "");
    static const std::string moderatorToken = getEnvStringWithFallback("GIGACHAD_MOD_TOKEN", "MEDIA_MOD_TOKEN", "");
    static const std::string adminToken = getEnvStringWithFallback("GIGACHAD_ADMIN_TOKEN", "MEDIA_ADMIN_TOKEN", "");
    return !baseToken.empty() || !moderatorToken.empty() || !adminToken.empty();
}

std::string extractApiTokenFromRequest(const HTTPRequest& req) {
    std::string token = getHeaderValue(req.headers, "X-API-Token");
    if (token.empty()) {
        token = extractBearerToken(getHeaderValue(req.headers, "Authorization"));
    }
    return token;
}

bool isHttpApiAuthorized(const std::string& routePath, const std::string& token) {
    if (routePath.find("/api/") != 0) {
        return true;
    }
    if (isPublicApiPath(routePath)) {
        return true;
    }

    static const std::string baseToken = getEnvStringWithFallback("GIGACHAD_API_TOKEN", "MEDIA_API_TOKEN", "");
    static const std::string moderatorToken = getEnvStringWithFallback("GIGACHAD_MOD_TOKEN", "MEDIA_MOD_TOKEN", "");
    static const std::string adminToken = getEnvStringWithFallback("GIGACHAD_ADMIN_TOKEN", "MEDIA_ADMIN_TOKEN", "");

    if (!isApiTokenProtectionEnabled()) {
        return true;
    }
    return token == baseToken || token == moderatorToken || token == adminToken;
}

std::string resolveRoleFromToken(const std::string& token) {
    static const std::string baseToken = getEnvStringWithFallback("GIGACHAD_API_TOKEN", "MEDIA_API_TOKEN", "");
    static const std::string moderatorToken = getEnvStringWithFallback("GIGACHAD_MOD_TOKEN", "MEDIA_MOD_TOKEN", "");
    static const std::string adminToken = getEnvStringWithFallback("GIGACHAD_ADMIN_TOKEN", "MEDIA_ADMIN_TOKEN", "");

    if (!adminToken.empty() && token == adminToken) {
        return "admin";
    }
    if (!moderatorToken.empty() && token == moderatorToken) {
        return "moderator";
    }

    if (!baseToken.empty() && token == baseToken) {
        // Backward compatible behavior: if dedicated role tokens are not configured,
        // base token retains full access as before role controls were introduced.
        if (adminToken.empty() && moderatorToken.empty()) {
            return "admin";
        }
        return "user";
    }

    return "anonymous";
}

int roleRank(const std::string& role) {
    if (role == "admin") return 3;
    if (role == "moderator") return 2;
    if (role == "user") return 1;
    return 0;
}

std::string requiredRoleForRoute(const std::string& method, const std::string& routePath) {
    // High-impact moderation actions.
    if (method == "POST" && routePath.find("/api/stream/end/") == 0) {
        return "moderator";
    }
    if (method == "POST" && routePath.find("/api/recording/stop/") == 0) {
        return "moderator";
    }
    if (method == "GET" && routePath == "/api/recording/list") {
        return "moderator";
    }
    if (method == "GET" && routePath.find("/api/recording/") == 0 && routePath != "/api/recording/list") {
        return "moderator";
    }
    return "user";
}

bool isRoleAuthorized(const std::string& role, const std::string& requiredRole) {
    return roleRank(role) >= roleRank(requiredRole);
}

bool performWebSocketHandshake(int clientSocket, const HTTPRequest& req) {
    std::string clientKey = getHeaderValue(req.headers, "Sec-WebSocket-Key");
    if (clientKey.empty()) {
        return false;
    }

    const std::string acceptKey = createWebSocketAcceptKey(clientKey);
    std::stringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << acceptKey << "\r\n"
             << "\r\n";

    std::string responseText = response.str();
    return sendAll(clientSocket, responseText.c_str(), responseText.size());
}

void handleWebSocketClient(int clientSocket, const HTTPRequest& req, const std::string& clientIp) {
    if (!isWebSocketTokenValid(req)) {
        sendHttpErrorResponse(clientSocket, 401, "Unauthorized", "{\"error\":\"Invalid websocket token\"}");
        closeClientSocket(clientSocket);
        return;
    }

    if (!consumeWebSocketRateLimit(clientIp)) {
        sendHttpErrorResponse(clientSocket, 429, "Too Many Requests", "{\"error\":\"WebSocket rate limit exceeded\"}");
        closeClientSocket(clientSocket);
        return;
    }

    if (!performWebSocketHandshake(clientSocket, req)) {
        closeClientSocket(clientSocket);
        return;
    }

    WebSocketClient client;
    client.socket = clientSocket;
    client.roomId = getQueryParam(req.path, "room");
    client.userId = getQueryParam(req.path, "user");

    {
        std::lock_guard<std::mutex> lock(ws_mutex);
        wsClients.push_back(client);
    }

    broadcastEvent(
        "presence.joined",
        "{\"user\":\"" + escapeJSONString(client.userId) + "\",\"roomId\":\"" + escapeJSONString(client.roomId) + "\"}",
        client.roomId
    );

    sendWebSocketFrame(
        clientSocket,
        0x1,
        "{\"type\":\"connected\",\"roomId\":\"" + escapeJSONString(client.roomId) + "\",\"timestamp\":\"" + escapeJSONString(getCurrentTimestamp()) + "\"}"
    );

    while (true) {
        uint8_t opcode = 0;
        std::string payload;
        if (!readWebSocketFrame(clientSocket, opcode, payload)) {
            break;
        }

        if (opcode == 0x8) { // close
            break;
        }
        if (opcode == 0x9) { // ping
            if (!sendWebSocketFrame(clientSocket, 0xA, payload)) { // pong
                break;
            }
            continue;
        }
        if (opcode == 0x1 && payload == "ping") {
            if (!sendWebSocketFrame(clientSocket, 0x1, "{\"type\":\"pong\"}")) {
                break;
            }
        }
    }

    removeWebSocketClient(clientSocket);
    broadcastEvent(
        "presence.left",
        "{\"user\":\"" + escapeJSONString(client.userId) + "\",\"roomId\":\"" + escapeJSONString(client.roomId) + "\"}",
        client.roomId
    );
    closeClientSocket(clientSocket);
}

HTTPResponse handleRequest(const HTTPRequest& req) {
    HTTPResponse res;
    const std::string routePath = getPathOnly(req.path);
    res.headers["Content-Type"] = "application/json";
    res.headers["Access-Control-Allow-Origin"] = "*";
    res.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Token, X-WS-Token";
    res.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";

    if (req.method == "OPTIONS") {
        res.status = 204;
        res.statusText = "No Content";
        res.body = "";
        return res;
    }
    
    // Serve static files
    if (req.method == "GET" && routePath == "/") {
        std::string content = readFileContent("public/index.html");
        if (!content.empty()) {
            res.headers["Content-Type"] = "text/html";
            res.body = content;
        } else {
            res.body = "<h1>Media Server API v2.0</h1><p>Web interface not found. API is running at /api/*</p>";
            res.headers["Content-Type"] = "text/html";
        }
        return res;
    }
    
    if (req.method == "GET" && routePath == "/styles.css") {
        std::string content = readFileContent("public/styles.css");
        if (!content.empty()) {
            res.headers["Content-Type"] = "text/css";
            res.body = content;
        }
        return res;
    }
    
    if (req.method == "GET" && routePath == "/app.js") {
        std::string content = readFileContent("public/app.js");
        if (!content.empty()) {
            res.headers["Content-Type"] = "application/javascript";
            res.body = content;
        }
        return res;
    }
    
    // API Documentation endpoint
    if (req.method == "GET" && routePath == "/api") {
        res.body = R"JSON({
  "message": "Media Server API",
  "version": "2.0.0",
  "features": ["chat", "voice-calls", "video-calls", "live-streaming"],
  "endpoints": {
    "Chat": {
      "GET /api/chat/messages": "Get all messages",
      "POST /api/chat/messages": "Send a message"
    },
    "Voice Calls": {
      "POST /api/voice/call": "Initiate voice call",
      "POST /api/voice/answer/:callId": "Answer voice call",
      "POST /api/voice/end/:callId": "End voice call",
      "GET /api/voice/active": "Get active voice calls"
    },
    "Video Calls": {
      "POST /api/video/call": "Initiate video call",
      "POST /api/video/answer/:callId": "Answer video call",
      "POST /api/video/ice/:callId": "Add ICE candidate",
      "POST /api/video/end/:callId": "End video call",
      "GET /api/video/active": "Get active video calls"
    },
    "Live Streaming": {
      "POST /api/stream/start": "Start live stream",
      "GET /api/stream/live": "Get all live streams",
      "POST /api/stream/join/:streamId": "Join stream as viewer",
      "POST /api/stream/leave/:streamId": "Leave stream",
      "POST /api/stream/end/:streamId": "End stream",
      "GET /api/stream/:streamId": "Get stream details"
    },
    "Health": {
      "GET /api/health": "Server health check"
    },
    "Auth": {
      "Header Authorization": "Bearer <token>",
      "Header X-API-Token": "<token>",
      "Env token names": "GIGACHAD_API_TOKEN or MEDIA_API_TOKEN",
      "Role tokens (optional)": [
        "GIGACHAD_MOD_TOKEN / MEDIA_MOD_TOKEN",
        "GIGACHAD_ADMIN_TOKEN / MEDIA_ADMIN_TOKEN"
      ]
    },
    "Realtime": {
      "GET /ws?room={roomId}&user={userId}&token={token}": "WebSocket event stream"
    }
  }
})JSON";
        return res;
    }
    
    // Health check
    if (req.method == "GET" && routePath == "/api/health") {
        size_t wsCount = 0;
        {
            std::lock_guard<std::mutex> lock(ws_mutex);
            wsCount = wsClients.size();
        }
        const std::string eventBusMode = detectEventBusMode();
        std::stringstream ss;
        ss << "{"
           << "\"status\":\"healthy\","
           << "\"service\":\"Media Server\","
           << "\"timestamp\":\"" << getCurrentTimestamp() << "\","
           << "\"eventBusMode\":\"" << eventBusMode << "\","
           << "\"eventBusRedisReady\":" << (g_eventBusRedisReady.load() ? "true" : "false") << ","
           << "\"stats\":{"
           << "\"messages\":" << messages.size() << ","
           << "\"activeCalls\":" << callSessions.size() << ","
           << "\"liveStreams\":" << liveStreams.size() << ","
           << "\"wsClients\":" << wsCount
           << "}}";
        res.body = ss.str();
        return res;
    }

    const std::string apiToken = extractApiTokenFromRequest(req);
    if (!isHttpApiAuthorized(routePath, apiToken)) {
        res.status = 401;
        res.statusText = "Unauthorized";
        res.body = "{\"error\":\"Invalid API token\"}";
        return res;
    }

    const std::string role = resolveRoleFromToken(apiToken);
    const std::string requiredRole = requiredRoleForRoute(req.method, routePath);
    if (isApiTokenProtectionEnabled() &&
        !isPublicApiPath(routePath) &&
        routePath.find("/api/") == 0 &&
        !isRoleAuthorized(role, requiredRole)) {
        res.status = 403;
        res.statusText = "Forbidden";
        res.body = "{\"error\":\"Insufficient role permissions\"}";
        return res;
    }
    
    // Route to specific handlers
    if (routePath.find("/api/chat/") == 0) {
        return handleChatAPI(req);
    }
    
    if (routePath.find("/api/voice/") == 0) {
        return handleVoiceCallAPI(req);
    }
    
    if (routePath.find("/api/video/") == 0) {
        return handleVideoCallAPI(req);
    }
    
    if (routePath.find("/api/stream/") == 0) {
        return handleLiveStreamAPI(req);
    }
    
    if (routePath.find("/api/recording/") == 0) {
        return handleScreenRecordingAPI(req);
    }
    
    // 404
    res.status = 404;
    res.statusText = "Not Found";
    res.body = "{\"error\":\"Endpoint not found\"}";
    return res;
}

// ============================================================================
// SERVER
// ============================================================================

void handleClient(int clientSocket, const std::string& clientIp) {
    char buffer[8192] = {0};
    
    #ifdef _WIN32
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    #else
    int bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
    #endif
    
    if (bytesRead > 0) {
        std::string requestStr(buffer, bytesRead);
        HTTPRequest req = parseRequest(requestStr);

        if (isWebSocketUpgradeRequest(req)) {
            std::cout << req.method << " " << req.path << " - 101" << std::endl;
            handleWebSocketClient(clientSocket, req, clientIp);
            return;
        }

        HTTPResponse res = handleRequest(req);
        
        std::string response = res.toString();

        sendAll(clientSocket, response.c_str(), response.length());
        closeClientSocket(clientSocket);
        
        std::cout << req.method << " " << req.path << " - " << res.status << std::endl;
    } else {
        closeClientSocket(clientSocket);
    }
}

int main() {
    #ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }
    #endif

    loadPersistedData();
    const int serverPort = detectServerPort();
    
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }
    
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(static_cast<uint16_t>(serverPort));
    
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        #ifdef _WIN32
        closesocket(serverSocket);
        WSACleanup();
        #else
        close(serverSocket);
        #endif
        return 1;
    }
    
    if (listen(serverSocket, 20) < 0) {
        std::cerr << "Listen failed" << std::endl;
        #ifdef _WIN32
        closesocket(serverSocket);
        WSACleanup();
        #else
        close(serverSocket);
        #endif
        return 1;
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "  🚀 MEDIA SERVER API v2.0" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Server: http://localhost:" << serverPort << std::endl;
    std::cout << "Storage: mode=" << detectStorageMode() << " | "
              << (g_sqliteReady ? "sqlite(messages,calls,streams,recordings)+jsonl-fallback" : "jsonl")
              << std::endl;
    std::cout << "EventBus: mode=" << detectEventBusMode() << std::endl;
    if (eventBusModeIsRedis()) {
        std::cout << "EventBus Redis: " << detectRedisHost() << ":" << detectRedisPort()
                  << " channel=" << detectEventBusChannel() << std::endl;
    }
    std::cout << "Loaded from disk: "
              << "messages=" << messages.size()
              << ", calls=" << callSessions.size()
              << ", streams=" << liveStreams.size()
              << ", recordings=" << screenRecordings.size()
              << std::endl;

    ensureRedisEventBusThreadStarted();
    std::cout << "\n📡 Available Services:" << std::endl;
    std::cout << "  💬 Chat Messages" << std::endl;
    std::cout << "  🎤 Voice Calls (WebRTC Signaling)" << std::endl;
    std::cout << "  📹 Video Calls (WebRTC Signaling)" << std::endl;
    std::cout << "  📺 Live Streaming" << std::endl;
    std::cout << "\n⚡ Endpoints:" << std::endl;
    std::cout << "  GET  / - API Documentation" << std::endl;
    std::cout << "  GET  /api/health - Health Check" << std::endl;
    std::cout << "\nPress Ctrl+C to stop" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    while (true) {
        sockaddr_in clientAddr;
        int addrLen = sizeof(clientAddr);
        
        #ifdef _WIN32
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &addrLen);
        #else
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, (socklen_t*)&addrLen);
        #endif
        
        if (clientSocket < 0) {
            continue;
        }

        char ipBuffer[64] = {0};
        #ifdef _WIN32
        const char* ipText = inet_ntoa(clientAddr.sin_addr);
        #else
        const char* ipText = inet_ntop(AF_INET, &(clientAddr.sin_addr), ipBuffer, sizeof(ipBuffer));
        #endif
        std::string clientIp = ipText ? ipText : "unknown";

        std::thread clientThread(handleClient, clientSocket, clientIp);
        clientThread.detach();
    }
    
    #ifdef _WIN32
    closesocket(serverSocket);
    WSACleanup();
    #else
    close(serverSocket);
    #endif
    
    return 0;
}
