// Complete Media Server with MongoDB Integration
// Supports: Chat, Voice Calls, Video Calls, Live Streaming
// Compile: g++ -std=c++17 media_server.cpp -o media_server.exe -lws2_32 -lbcrypt -O2 -static

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")
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
#include <unordered_set>
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
    std::string tenantId;
    
    std::string toJSON() const {
        return "{\"id\":\"" + escapeJSON(id) + 
               "\",\"username\":\"" + escapeJSON(username) + 
               "\",\"content\":\"" + escapeJSON(content) + 
               "\",\"timestamp\":\"" + escapeJSON(timestamp) + 
               "\",\"roomId\":\"" + escapeJSON(roomId) +
               "\",\"tenantId\":\"" + escapeJSON(tenantId) + "\"}";
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
    std::string tenantId;
};

std::vector<WebSocketClient> wsClients;
std::mutex ws_mutex;
std::unordered_map<std::string, std::deque<std::chrono::steady_clock::time_point>> wsConnectionAttempts;
std::mutex ws_rate_mutex;
std::unordered_map<std::string, std::deque<std::chrono::steady_clock::time_point>> passkeyAttempts;
std::mutex passkey_rate_mutex;
struct PasskeyFailureState {
    int failures = 0;
    std::chrono::steady_clock::time_point lockUntil{};
};
std::unordered_map<std::string, std::deque<std::chrono::steady_clock::time_point>> passkeyFailureAttempts;
std::mutex passkey_failure_mutex;
std::mutex auth_audit_mutex;

const std::string kDataDir = "data";
const std::string kMessagesFile = "data/messages.jsonl";
const std::string kCallsFile = "data/calls.jsonl";
const std::string kStreamsFile = "data/streams.jsonl";
const std::string kRecordingsFile = "data/recordings.jsonl";
const std::string kPasskeysFile = "data/passkeys.jsonl";
const std::string kAuthAuditFile = "data/auth_audit.jsonl";
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

bool eventBusDebugEnabled() {
    static const bool enabled = []() {
        const char* primary = std::getenv("GIGACHAD_EVENT_BUS_DEBUG");
        const char* fallback = std::getenv("MEDIA_EVENT_BUS_DEBUG");
        std::string raw = primary && *primary ? primary : (fallback && *fallback ? fallback : "");
        std::transform(raw.begin(), raw.end(), raw.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return raw == "1" || raw == "true" || raw == "yes";
    }();
    return enabled;
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
std::unordered_set<std::string> g_revokedRefreshTokenJti;
std::unordered_set<std::string> g_revokedAccessTokenJti;
std::mutex g_auth_mutex;

struct PasskeyCredential {
    std::string credentialId;
    std::string publicKey;
    int signCount = 0;
    std::string role;
    std::string tenantId;
    std::string createdAt;
};

struct PasskeyChallengeState {
    std::string challenge;
    std::string username;
    std::string role;
    std::string tenantId;
    std::string rpId;
    std::string flow; // register | login
    int expiresAt = 0;
};

std::map<std::string, std::vector<PasskeyCredential>> g_passkeyCredentialsByUser;
std::unordered_map<std::string, PasskeyChallengeState> g_passkeyChallenges;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================
std::string escapeJSONString(const std::string& s);
std::string trimWhitespace(const std::string& input);
std::string toLower(std::string s);
std::string getHeaderValue(const std::map<std::string, std::string>& headers, const std::string& key);

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

std::string normalizeTenantId(const std::string& rawTenant) {
    std::string tenant = rawTenant;
    auto isWs = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!tenant.empty() && isWs(static_cast<unsigned char>(tenant.front()))) {
        tenant.erase(tenant.begin());
    }
    while (!tenant.empty() && isWs(static_cast<unsigned char>(tenant.back()))) {
        tenant.pop_back();
    }
    if (tenant.empty()) {
        return "default";
    }
    return tenant;
}

std::string normalizeUsername(const std::string& rawUser) {
    return toLower(trimWhitespace(rawUser));
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
        "roomId TEXT,"
        "tenantId TEXT DEFAULT 'default'"
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
    const std::string createRevokedTokensSql =
        "CREATE TABLE IF NOT EXISTS revoked_tokens ("
        "jti TEXT PRIMARY KEY,"
        "tokenType TEXT,"
        "exp INTEGER,"
        "revokedAt TEXT"
        ");";
    const std::string createPasskeyCredentialsSql =
        "CREATE TABLE IF NOT EXISTS passkey_credentials ("
        "username TEXT,"
        "credentialId TEXT,"
        "publicKey TEXT,"
        "signCount INTEGER DEFAULT 0,"
        "role TEXT,"
        "tenantId TEXT,"
        "createdAt TEXT,"
        "PRIMARY KEY(username, credentialId)"
        ");";
    bool ok = sqliteExec(db, createMessagesSql) &&
              sqliteExec(db, createCallsSql) &&
              sqliteExec(db, createStreamsSql) &&
              sqliteExec(db, createRecordingsSql) &&
              sqliteExec(db, createRevokedTokensSql) &&
              sqliteExec(db, createPasskeyCredentialsSql);
    if (ok) {
        sqliteExec(db, "ALTER TABLE messages ADD COLUMN tenantId TEXT DEFAULT 'default';");
    }
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

    const char* insertSql = "INSERT INTO messages(id, username, content, timestamp, roomId, tenantId) VALUES(?, ?, ?, ?, ?, ?);";
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
            p_sqlite3_bind_text(stmt, 5, msg.roomId.c_str(), -1, transient) != SQLITE_OK ||
            p_sqlite3_bind_text(stmt, 6, msg.tenantId.c_str(), -1, transient) != SQLITE_OK) {
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

    const char* querySql = "SELECT id, username, content, timestamp, roomId, COALESCE(tenantId, 'default') FROM messages ORDER BY rowid ASC;";
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
        const unsigned char* c5 = p_sqlite3_column_text(stmt, 5);
        msg.id = c0 ? reinterpret_cast<const char*>(c0) : "";
        msg.username = c1 ? reinterpret_cast<const char*>(c1) : "";
        msg.content = c2 ? reinterpret_cast<const char*>(c2) : "";
        msg.timestamp = c3 ? reinterpret_cast<const char*>(c3) : "";
        msg.roomId = c4 ? reinterpret_cast<const char*>(c4) : "";
        msg.tenantId = normalizeTenantId(c5 ? reinterpret_cast<const char*>(c5) : "");
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
                msg.tenantId = normalizeTenantId(extractJSONValue(line, "tenantId"));
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
        msg.tenantId = normalizeTenantId(extractJSONValue(line, "tenantId"));
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

bool persistPasskeyCredentialsSnapshotToSqlite(const std::map<std::string, std::vector<PasskeyCredential>>& snapshot) {
    if (!g_sqliteReady && !initSqliteStorage()) return false;

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
    if (!sqliteExec(db, "DELETE FROM passkey_credentials;")) {
        sqliteExec(db, "ROLLBACK;");
        p_sqlite3_close(db);
        return false;
    }

    const char* insertSql =
        "INSERT INTO passkey_credentials(username, credentialId, publicKey, signCount, role, tenantId, createdAt) "
        "VALUES(?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (p_sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr) != SQLITE_OK || stmt == nullptr) {
        sqliteExec(db, "ROLLBACK;");
        p_sqlite3_close(db);
        return false;
    }

    bool ok = true;
    auto transient = reinterpret_cast<void(*)(void*)>(-1);
    for (const auto& kv : snapshot) {
        const std::string& username = kv.first;
        for (const auto& cred : kv.second) {
            std::string signCountText = std::to_string(cred.signCount);
            if (p_sqlite3_bind_text(stmt, 1, username.c_str(), -1, transient) != SQLITE_OK ||
                p_sqlite3_bind_text(stmt, 2, cred.credentialId.c_str(), -1, transient) != SQLITE_OK ||
                p_sqlite3_bind_text(stmt, 3, cred.publicKey.c_str(), -1, transient) != SQLITE_OK ||
                p_sqlite3_bind_text(stmt, 4, signCountText.c_str(), -1, transient) != SQLITE_OK ||
                p_sqlite3_bind_text(stmt, 5, cred.role.c_str(), -1, transient) != SQLITE_OK ||
                p_sqlite3_bind_text(stmt, 6, cred.tenantId.c_str(), -1, transient) != SQLITE_OK ||
                p_sqlite3_bind_text(stmt, 7, cred.createdAt.c_str(), -1, transient) != SQLITE_OK) {
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
        if (!ok) break;
    }

    p_sqlite3_finalize(stmt);
    if (ok) ok = sqliteExec(db, "COMMIT;");
    else sqliteExec(db, "ROLLBACK;");
    p_sqlite3_close(db);
    return ok;
}

bool persistPasskeyCredentialsToSqlite() {
    std::map<std::string, std::vector<PasskeyCredential>> snapshot;
    {
        std::lock_guard<std::mutex> lock(g_auth_mutex);
        snapshot = g_passkeyCredentialsByUser;
    }
    return persistPasskeyCredentialsSnapshotToSqlite(snapshot);
}

bool persistPasskeyCredentialsSnapshotToDisk(const std::map<std::string, std::vector<PasskeyCredential>>& snapshot) {
    std::vector<std::string> lines;
    for (const auto& kv : snapshot) {
        const std::string& username = kv.first;
        for (const auto& cred : kv.second) {
            lines.push_back(
                "{"
                "\"username\":\"" + escapeJSONString(username) + "\","
                "\"credentialId\":\"" + escapeJSONString(cred.credentialId) + "\","
                "\"publicKey\":\"" + escapeJSONString(cred.publicKey) + "\","
                "\"signCount\":" + std::to_string(std::max(0, cred.signCount)) + ","
                "\"role\":\"" + escapeJSONString(cred.role) + "\","
                "\"tenantId\":\"" + escapeJSONString(normalizeTenantId(cred.tenantId)) + "\","
                "\"createdAt\":\"" + escapeJSONString(cred.createdAt) + "\""
                "}"
            );
        }
    }
    return writeLinesToFile(kPasskeysFile, lines);
}

bool persistPasskeyCredentialsToDisk() {
    std::map<std::string, std::vector<PasskeyCredential>> snapshot;
    {
        std::lock_guard<std::mutex> lock(g_auth_mutex);
        snapshot = g_passkeyCredentialsByUser;
    }
    return persistPasskeyCredentialsSnapshotToDisk(snapshot);
}

void loadPasskeyCredentialsFromSqlite() {
    if (!g_sqliteReady && !initSqliteStorage()) return;

    std::lock_guard<std::mutex> lock(sqlite_mutex);
    sqlite3* db = nullptr;
    if (p_sqlite3_open(kSqliteDbFile.c_str(), &db) != SQLITE_OK || db == nullptr) {
        if (db) p_sqlite3_close(db);
        return;
    }

    const char* sql =
        "SELECT username, credentialId, publicKey, COALESCE(signCount, 0), COALESCE(role, 'user'), "
        "COALESCE(tenantId, 'default'), COALESCE(createdAt, '') "
        "FROM passkey_credentials ORDER BY rowid ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (p_sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK || stmt == nullptr) {
        p_sqlite3_close(db);
        return;
    }

    std::map<std::string, std::vector<PasskeyCredential>> loaded;
    while (true) {
        int rc = p_sqlite3_step(stmt);
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) break;

        const unsigned char* c0 = p_sqlite3_column_text(stmt, 0);
        const unsigned char* c1 = p_sqlite3_column_text(stmt, 1);
        const unsigned char* c2 = p_sqlite3_column_text(stmt, 2);
        const unsigned char* c3 = p_sqlite3_column_text(stmt, 3);
        const unsigned char* c4 = p_sqlite3_column_text(stmt, 4);
        const unsigned char* c5 = p_sqlite3_column_text(stmt, 5);
        const unsigned char* c6 = p_sqlite3_column_text(stmt, 6);

        std::string username = normalizeUsername(c0 ? reinterpret_cast<const char*>(c0) : "");
        std::string credentialId = c1 ? reinterpret_cast<const char*>(c1) : "";
        if (username.empty() || credentialId.empty()) {
            continue;
        }

        PasskeyCredential cred;
        cred.credentialId = credentialId;
        cred.publicKey = c2 ? reinterpret_cast<const char*>(c2) : "";
        cred.signCount = std::max(0, c3 ? std::atoi(reinterpret_cast<const char*>(c3)) : 0);
        cred.role = c4 ? reinterpret_cast<const char*>(c4) : "user";
        cred.tenantId = normalizeTenantId(c5 ? reinterpret_cast<const char*>(c5) : "default");
        cred.createdAt = c6 ? reinterpret_cast<const char*>(c6) : "";
        loaded[normalizeUsername(username)].push_back(cred);
    }

    p_sqlite3_finalize(stmt);
    p_sqlite3_close(db);

    {
        std::lock_guard<std::mutex> authLock(g_auth_mutex);
        g_passkeyCredentialsByUser = std::move(loaded);
    }
}

void loadPasskeyCredentialsFromDisk() {
    std::map<std::string, std::vector<PasskeyCredential>> loaded;
    auto lines = readLinesFromFile(kPasskeysFile);
    for (const auto& line : lines) {
        const std::string username = normalizeUsername(extractJSONValue(line, "username"));
        const std::string credentialId = trimWhitespace(extractJSONValue(line, "credentialId"));
        if (username.empty() || credentialId.empty()) {
            continue;
        }

        PasskeyCredential cred;
        cred.credentialId = credentialId;
        cred.publicKey = extractJSONValue(line, "publicKey");
        cred.signCount = std::max(0, extractJSONIntValue(line, "signCount", 0));
        cred.role = toLower(trimWhitespace(extractJSONValue(line, "role")));
        if (cred.role != "admin" && cred.role != "moderator" && cred.role != "user") {
            cred.role = "user";
        }
        cred.tenantId = normalizeTenantId(extractJSONValue(line, "tenantId"));
        cred.createdAt = extractJSONValue(line, "createdAt");
        loaded[normalizeUsername(username)].push_back(cred);
    }
    {
        std::lock_guard<std::mutex> authLock(g_auth_mutex);
        g_passkeyCredentialsByUser = std::move(loaded);
    }
}

bool persistRevokedTokenJtiToSqlite(const std::string& tokenType, const std::string& jti, int exp) {
    if (jti.empty()) return false;
    if (!g_sqliteReady && !initSqliteStorage()) return false;

    std::lock_guard<std::mutex> lock(sqlite_mutex);
    sqlite3* db = nullptr;
    if (p_sqlite3_open(kSqliteDbFile.c_str(), &db) != SQLITE_OK || db == nullptr) {
        if (db) p_sqlite3_close(db);
        return false;
    }

    const char* sql = "INSERT OR REPLACE INTO revoked_tokens(jti, tokenType, exp, revokedAt) VALUES(?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (p_sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK || stmt == nullptr) {
        p_sqlite3_close(db);
        return false;
    }

    auto transient = reinterpret_cast<void(*)(void*)>(-1);
    bool ok = p_sqlite3_bind_text(stmt, 1, jti.c_str(), -1, transient) == SQLITE_OK &&
              p_sqlite3_bind_text(stmt, 2, tokenType.c_str(), -1, transient) == SQLITE_OK &&
              p_sqlite3_bind_text(stmt, 3, std::to_string(exp).c_str(), -1, transient) == SQLITE_OK &&
              p_sqlite3_bind_text(stmt, 4, getCurrentTimestamp().c_str(), -1, transient) == SQLITE_OK;
    if (ok) {
        ok = p_sqlite3_step(stmt) == SQLITE_DONE;
    }
    p_sqlite3_finalize(stmt);
    p_sqlite3_close(db);
    return ok;
}

void loadRevokedTokenJtiFromSqlite() {
    if (!g_sqliteReady && !initSqliteStorage()) return;

    std::lock_guard<std::mutex> lock(sqlite_mutex);
    sqlite3* db = nullptr;
    if (p_sqlite3_open(kSqliteDbFile.c_str(), &db) != SQLITE_OK || db == nullptr) {
        if (db) p_sqlite3_close(db);
        return;
    }

    const int now = static_cast<int>(std::time(nullptr));
    sqliteExec(db, "DELETE FROM revoked_tokens WHERE exp IS NOT NULL AND exp > 0 AND exp < " + std::to_string(now) + ";");

    const char* sql = "SELECT jti, tokenType FROM revoked_tokens;";
    sqlite3_stmt* stmt = nullptr;
    if (p_sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK || stmt == nullptr) {
        p_sqlite3_close(db);
        return;
    }

    {
        std::lock_guard<std::mutex> authLock(g_auth_mutex);
        g_revokedAccessTokenJti.clear();
        g_revokedRefreshTokenJti.clear();
        while (true) {
            int rc = p_sqlite3_step(stmt);
            if (rc == SQLITE_DONE) break;
            if (rc != SQLITE_ROW) break;
            const unsigned char* c0 = p_sqlite3_column_text(stmt, 0);
            const unsigned char* c1 = p_sqlite3_column_text(stmt, 1);
            std::string jti = c0 ? reinterpret_cast<const char*>(c0) : "";
            std::string type = c1 ? reinterpret_cast<const char*>(c1) : "";
            if (jti.empty()) continue;
            if (type == "refresh") g_revokedRefreshTokenJti.insert(jti);
            else g_revokedAccessTokenJti.insert(jti);
        }
    }

    p_sqlite3_finalize(stmt);
    p_sqlite3_close(db);
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
    loadRevokedTokenJtiFromSqlite();
    loadPasskeyCredentialsFromSqlite();
    bool hasPasskeys = false;
    {
        std::lock_guard<std::mutex> lock(g_auth_mutex);
        hasPasskeys = !g_passkeyCredentialsByUser.empty();
    }
    if (!hasPasskeys) {
        loadPasskeyCredentialsFromDisk();
        persistPasskeyCredentialsToSqlite();
    }
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

std::string base64Encode(const uint8_t* data, size_t len);
std::string escapeJSONString(const std::string& s);
int nowEpochSec();

struct AuthContext {
    bool valid = false;
    std::string tokenType; // access or refresh
    std::string subject;
    std::string role;
    std::string tenantId;
    std::string jti;
    int exp = 0;
};

struct JwtSigningKey {
    std::string kid;
    std::string secret;
};

std::string trimWhitespace(const std::string& input) {
    std::string out = input;
    auto isWs = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!out.empty() && isWs(static_cast<unsigned char>(out.front()))) out.erase(out.begin());
    while (!out.empty() && isWs(static_cast<unsigned char>(out.back()))) out.pop_back();
    return out;
}

bool jwtEnabled() {
    auto keys = []() {
        std::vector<JwtSigningKey> out;
        std::string rawList = trimWhitespace(getEnvStringWithFallback("GIGACHAD_JWT_KEYS", "MEDIA_JWT_KEYS", ""));
        if (!rawList.empty()) {
            for (char& c : rawList) {
                if (c == ';') c = ',';
            }
            std::stringstream ss(rawList);
            std::string item;
            while (std::getline(ss, item, ',')) {
                item = trimWhitespace(item);
                if (item.empty()) continue;
                size_t pos = item.find(':');
                JwtSigningKey k;
                if (pos == std::string::npos) {
                    k.kid = "";
                    k.secret = trimWhitespace(item);
                } else {
                    k.kid = trimWhitespace(item.substr(0, pos));
                    k.secret = trimWhitespace(item.substr(pos + 1));
                }
                if (!k.secret.empty()) out.push_back(k);
            }
        }
        if (out.empty()) {
            std::string fallbackSecret = trimWhitespace(getEnvStringWithFallback("GIGACHAD_JWT_SECRET", "MEDIA_JWT_SECRET", ""));
            if (!fallbackSecret.empty()) {
                out.push_back(JwtSigningKey{"default", fallbackSecret});
            }
        }
        return out;
    }();
    return !keys.empty();
}

const std::vector<JwtSigningKey>& jwtSigningKeys() {
    static const std::vector<JwtSigningKey> keys = []() {
        std::vector<JwtSigningKey> out;
        std::string rawList = trimWhitespace(getEnvStringWithFallback("GIGACHAD_JWT_KEYS", "MEDIA_JWT_KEYS", ""));
        if (!rawList.empty()) {
            for (char& c : rawList) {
                if (c == ';') c = ',';
            }
            std::stringstream ss(rawList);
            std::string item;
            while (std::getline(ss, item, ',')) {
                item = trimWhitespace(item);
                if (item.empty()) continue;
                size_t pos = item.find(':');
                JwtSigningKey k;
                if (pos == std::string::npos) {
                    k.kid = "";
                    k.secret = trimWhitespace(item);
                } else {
                    k.kid = trimWhitespace(item.substr(0, pos));
                    k.secret = trimWhitespace(item.substr(pos + 1));
                }
                if (!k.secret.empty()) out.push_back(k);
            }
        }
        if (out.empty()) {
            std::string fallbackSecret = trimWhitespace(getEnvStringWithFallback("GIGACHAD_JWT_SECRET", "MEDIA_JWT_SECRET", ""));
            if (!fallbackSecret.empty()) {
                out.push_back(JwtSigningKey{"default", fallbackSecret});
            }
        }
        return out;
    }();
    return keys;
}

std::string jwtActiveKid() {
    static const std::string activeKid = trimWhitespace(getEnvStringWithFallback("GIGACHAD_JWT_ACTIVE_KID", "MEDIA_JWT_ACTIVE_KID", ""));
    return activeKid;
}

JwtSigningKey jwtActiveSigningKey() {
    const auto& keys = jwtSigningKeys();
    if (keys.empty()) return JwtSigningKey{};
    const std::string active = jwtActiveKid();
    if (!active.empty()) {
        for (const auto& k : keys) {
            if (k.kid == active) return k;
        }
    }
    return keys.front();
}

int jwtAccessTtlSec() {
    static const int ttl = getEnvIntWithFallback("GIGACHAD_JWT_ACCESS_TTL_SEC", "MEDIA_JWT_ACCESS_TTL_SEC", 900);
    return ttl > 0 ? ttl : 900;
}

int jwtRefreshTtlSec() {
    static const int ttl = getEnvIntWithFallback("GIGACHAD_JWT_REFRESH_TTL_SEC", "MEDIA_JWT_REFRESH_TTL_SEC", 604800);
    return ttl > 0 ? ttl : 604800;
}

std::string base64UrlFromBase64(std::string b64) {
    for (char& c : b64) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    while (!b64.empty() && b64.back() == '=') b64.pop_back();
    return b64;
}

std::string base64UrlEncode(const uint8_t* data, size_t len) {
    return base64UrlFromBase64(base64Encode(data, len));
}

int base64Value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

std::string base64UrlDecodeToString(std::string in) {
    for (char& c : in) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    while ((in.size() % 4) != 0) in.push_back('=');

    std::string out;
    out.reserve((in.size() / 4) * 3);
    for (size_t i = 0; i + 3 < in.size(); i += 4) {
        int v0 = base64Value(in[i]);
        int v1 = base64Value(in[i + 1]);
        int v2 = (in[i + 2] == '=') ? 0 : base64Value(in[i + 2]);
        int v3 = (in[i + 3] == '=') ? 0 : base64Value(in[i + 3]);
        if (v0 < 0 || v1 < 0 || (in[i + 2] != '=' && v2 < 0) || (in[i + 3] != '=' && v3 < 0)) {
            return "";
        }
        uint32_t triple = (static_cast<uint32_t>(v0) << 18) |
                          (static_cast<uint32_t>(v1) << 12) |
                          (static_cast<uint32_t>(v2) << 6) |
                          static_cast<uint32_t>(v3);
        out.push_back(static_cast<char>((triple >> 16) & 0xff));
        if (in[i + 2] != '=') out.push_back(static_cast<char>((triple >> 8) & 0xff));
        if (in[i + 3] != '=') out.push_back(static_cast<char>(triple & 0xff));
    }
    return out;
}

std::array<uint8_t, 32> sha256(const std::string& input) {
    static const uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    auto rotr = [](uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); };

    std::vector<uint8_t> bytes(input.begin(), input.end());
    uint64_t bitLen = static_cast<uint64_t>(bytes.size()) * 8;
    bytes.push_back(0x80);
    while ((bytes.size() % 64) != 56) bytes.push_back(0x00);
    for (int i = 7; i >= 0; --i) bytes.push_back(static_cast<uint8_t>((bitLen >> (i * 8)) & 0xff));

    uint32_t h0 = 0x6a09e667, h1 = 0xbb67ae85, h2 = 0x3c6ef372, h3 = 0xa54ff53a;
    uint32_t h4 = 0x510e527f, h5 = 0x9b05688c, h6 = 0x1f83d9ab, h7 = 0x5be0cd19;

    for (size_t chunk = 0; chunk < bytes.size(); chunk += 64) {
        uint32_t w[64] = {0};
        for (int i = 0; i < 16; ++i) {
            size_t j = chunk + static_cast<size_t>(i * 4);
            w[i] = (static_cast<uint32_t>(bytes[j]) << 24) |
                   (static_cast<uint32_t>(bytes[j + 1]) << 16) |
                   (static_cast<uint32_t>(bytes[j + 2]) << 8) |
                   static_cast<uint32_t>(bytes[j + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4, f = h5, g = h6, h = h7;
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = h + S1 + ch + k[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;
            h = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }

        h0 += a; h1 += b; h2 += c; h3 += d;
        h4 += e; h5 += f; h6 += g; h7 += h;
    }

    return {
        static_cast<uint8_t>((h0 >> 24) & 0xff), static_cast<uint8_t>((h0 >> 16) & 0xff), static_cast<uint8_t>((h0 >> 8) & 0xff), static_cast<uint8_t>(h0 & 0xff),
        static_cast<uint8_t>((h1 >> 24) & 0xff), static_cast<uint8_t>((h1 >> 16) & 0xff), static_cast<uint8_t>((h1 >> 8) & 0xff), static_cast<uint8_t>(h1 & 0xff),
        static_cast<uint8_t>((h2 >> 24) & 0xff), static_cast<uint8_t>((h2 >> 16) & 0xff), static_cast<uint8_t>((h2 >> 8) & 0xff), static_cast<uint8_t>(h2 & 0xff),
        static_cast<uint8_t>((h3 >> 24) & 0xff), static_cast<uint8_t>((h3 >> 16) & 0xff), static_cast<uint8_t>((h3 >> 8) & 0xff), static_cast<uint8_t>(h3 & 0xff),
        static_cast<uint8_t>((h4 >> 24) & 0xff), static_cast<uint8_t>((h4 >> 16) & 0xff), static_cast<uint8_t>((h4 >> 8) & 0xff), static_cast<uint8_t>(h4 & 0xff),
        static_cast<uint8_t>((h5 >> 24) & 0xff), static_cast<uint8_t>((h5 >> 16) & 0xff), static_cast<uint8_t>((h5 >> 8) & 0xff), static_cast<uint8_t>(h5 & 0xff),
        static_cast<uint8_t>((h6 >> 24) & 0xff), static_cast<uint8_t>((h6 >> 16) & 0xff), static_cast<uint8_t>((h6 >> 8) & 0xff), static_cast<uint8_t>(h6 & 0xff),
        static_cast<uint8_t>((h7 >> 24) & 0xff), static_cast<uint8_t>((h7 >> 16) & 0xff), static_cast<uint8_t>((h7 >> 8) & 0xff), static_cast<uint8_t>(h7 & 0xff)
    };
}

std::array<uint8_t, 32> hmacSha256(const std::string& key, const std::string& msg) {
    std::string k = key;
    if (k.size() > 64) {
        auto hashed = sha256(k);
        k.assign(reinterpret_cast<const char*>(hashed.data()), hashed.size());
    }
    if (k.size() < 64) k.append(64 - k.size(), '\0');

    std::string oKeyPad(64, '\0');
    std::string iKeyPad(64, '\0');
    for (size_t i = 0; i < 64; ++i) {
        unsigned char kc = static_cast<unsigned char>(k[i]);
        oKeyPad[i] = static_cast<char>(kc ^ 0x5c);
        iKeyPad[i] = static_cast<char>(kc ^ 0x36);
    }
    auto inner = sha256(iKeyPad + msg);
    std::string innerStr(reinterpret_cast<const char*>(inner.data()), inner.size());
    return sha256(oKeyPad + innerStr);
}

std::vector<std::string> splitByChar(const std::string& input, char sep) {
    std::vector<std::string> out;
    std::stringstream ss(input);
    std::string part;
    while (std::getline(ss, part, sep)) out.push_back(part);
    return out;
}

int nowEpochSec() {
    return static_cast<int>(std::time(nullptr));
}

std::string buildJwtToken(const std::string& tokenType, const std::string& subject, const std::string& role, const std::string& tenantId, int ttlSec) {
    const JwtSigningKey key = jwtActiveSigningKey();
    if (key.secret.empty()) {
        return "";
    }
    const int now = nowEpochSec();
    const int exp = now + ttlSec;
    const std::string jti = generateId(tokenType + "_");
    std::string headerJson = "{\"alg\":\"HS256\",\"typ\":\"JWT\"";
    if (!key.kid.empty()) {
        headerJson += ",\"kid\":\"" + escapeJSONString(key.kid) + "\"";
    }
    headerJson += "}";
    const std::string payloadJson =
        "{\"sub\":\"" + escapeJSONString(subject) +
        "\",\"role\":\"" + escapeJSONString(role) +
        "\",\"tenantId\":\"" + escapeJSONString(normalizeTenantId(tenantId)) +
        "\",\"type\":\"" + escapeJSONString(tokenType) +
        "\",\"jti\":\"" + escapeJSONString(jti) +
        "\",\"iat\":" + std::to_string(now) +
        ",\"exp\":" + std::to_string(exp) + "}";

    const std::string headerB64 = base64UrlEncode(reinterpret_cast<const uint8_t*>(headerJson.data()), headerJson.size());
    const std::string payloadB64 = base64UrlEncode(reinterpret_cast<const uint8_t*>(payloadJson.data()), payloadJson.size());
    const std::string signingInput = headerB64 + "." + payloadB64;
    const auto sig = hmacSha256(key.secret, signingInput);
    const std::string sigB64 = base64UrlEncode(sig.data(), sig.size());
    return signingInput + "." + sigB64;
}

bool tryParseAndValidateJwt(const std::string& token, AuthContext& outCtx) {
    outCtx = AuthContext{};
    if (!jwtEnabled()) return false;
    auto parts = splitByChar(token, '.');
    if (parts.size() != 3) return false;

    const std::string signingInput = parts[0] + "." + parts[1];

    const std::string headerJson = base64UrlDecodeToString(parts[0]);
    if (headerJson.empty()) return false;
    const std::string tokenKid = extractJSONValue(headerJson, "kid");

    bool signatureOk = false;
    const auto& keys = jwtSigningKeys();
    for (const auto& key : keys) {
        if (!tokenKid.empty() && key.kid != tokenKid) {
            continue;
        }
        const auto expectedSig = hmacSha256(key.secret, signingInput);
        const std::string expectedSigB64 = base64UrlEncode(expectedSig.data(), expectedSig.size());
        if (expectedSigB64 == parts[2]) {
            signatureOk = true;
            break;
        }
    }
    if (!signatureOk) return false;

    const std::string payloadJson = base64UrlDecodeToString(parts[1]);
    if (payloadJson.empty()) return false;

    outCtx.subject = extractJSONValue(payloadJson, "sub");
    outCtx.role = extractJSONValue(payloadJson, "role");
    outCtx.tenantId = normalizeTenantId(extractJSONValue(payloadJson, "tenantId"));
    outCtx.tokenType = extractJSONValue(payloadJson, "type");
    outCtx.jti = extractJSONValue(payloadJson, "jti");
    outCtx.exp = extractJSONIntValue(payloadJson, "exp", 0);

    if (outCtx.subject.empty() || outCtx.role.empty() || outCtx.tokenType.empty() || outCtx.jti.empty() || outCtx.exp <= 0) {
        return false;
    }
    if (outCtx.exp < nowEpochSec()) return false;

    {
        std::lock_guard<std::mutex> lock(g_auth_mutex);
        if (outCtx.tokenType == "refresh" && g_revokedRefreshTokenJti.find(outCtx.jti) != g_revokedRefreshTokenJti.end()) {
            return false;
        }
        if (outCtx.tokenType == "access" && g_revokedAccessTokenJti.find(outCtx.jti) != g_revokedAccessTokenJti.end()) {
            return false;
        }
    }

    outCtx.valid = true;
    return true;
}

int passkeyChallengeTtlSec() {
    static const int ttl = getEnvIntWithFallback("GIGACHAD_PASSKEY_CHALLENGE_TTL_SEC", "MEDIA_PASSKEY_CHALLENGE_TTL_SEC", 120);
    return ttl > 0 ? ttl : 120;
}

bool passkeyStrictMetadataEnabled() {
    static const int strictValue = getEnvIntWithFallback("GIGACHAD_PASSKEY_STRICT_METADATA", "MEDIA_PASSKEY_STRICT_METADATA", 1);
    return strictValue != 0;
}

bool passkeyCounterStrictEnabled() {
    static const int strictValue = getEnvIntWithFallback("GIGACHAD_PASSKEY_COUNTER_STRICT", "MEDIA_PASSKEY_COUNTER_STRICT", 1);
    return strictValue != 0;
}

bool validatePasskeyCoseCredentialMaterial(const std::string& credentialKeyMaterial, std::string& errorReason);

std::string passkeySignatureMode() {
    static const std::string mode = []() {
        std::string raw = toLower(trimWhitespace(getEnvStringWithFallback(
            "GIGACHAD_PASSKEY_SIGNATURE_MODE",
            "MEDIA_PASSKEY_SIGNATURE_MODE",
            "hmac"
        )));
        if (raw == "es256") return std::string("es256");
        return std::string("hmac");
    }();
    return mode;
}

bool validatePasskeyCredentialMaterial(const std::string& credentialKeyMaterial, std::string& errorReason) {
    if (credentialKeyMaterial.empty()) {
        errorReason = "Credential key is missing";
        return false;
    }
    if (passkeySignatureMode() == "hmac") {
        return true;
    }
    return validatePasskeyCoseCredentialMaterial(credentialKeyMaterial, errorReason);
}

std::string passkeyRpId() {
    static const std::string rpId = trimWhitespace(getEnvStringWithFallback("GIGACHAD_PASSKEY_RP_ID", "MEDIA_PASSKEY_RP_ID", ""));
    return rpId;
}

std::vector<std::string> splitCsvTrimmed(const std::string& raw) {
    std::vector<std::string> out;
    std::stringstream ss(raw);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string normalized = trimWhitespace(item);
        if (!normalized.empty()) {
            out.push_back(normalized);
        }
    }
    return out;
}

const std::vector<std::string>& passkeyAllowedOrigins() {
    static const std::vector<std::string> origins = []() {
        std::string raw = trimWhitespace(getEnvStringWithFallback("GIGACHAD_PASSKEY_ALLOWED_ORIGINS", "MEDIA_PASSKEY_ALLOWED_ORIGINS", ""));
        if (raw.empty()) {
            return std::vector<std::string>{};
        }
        return splitCsvTrimmed(raw);
    }();
    return origins;
}

bool isPasskeyOriginAllowed(const std::string& origin) {
    const auto& allowed = passkeyAllowedOrigins();
    if (allowed.empty()) {
        return true;
    }
    for (const auto& candidate : allowed) {
        if (candidate == origin) {
            return true;
        }
    }
    return false;
}

std::string firstIpFromCsv(const std::string& value) {
    size_t comma = value.find(',');
    if (comma == std::string::npos) return trimWhitespace(value);
    return trimWhitespace(value.substr(0, comma));
}

std::string resolveClientIpFromHeaders(const std::map<std::string, std::string>& headers) {
    std::string ip = firstIpFromCsv(getHeaderValue(headers, "X-Forwarded-For"));
    if (!ip.empty()) return ip;
    ip = trimWhitespace(getHeaderValue(headers, "X-Real-IP"));
    if (!ip.empty()) return ip;
    return "unknown";
}

bool consumePasskeyRateLimit(const std::string& clientIp, const std::string& usernameRaw) {
    static const int windowSeconds = getEnvIntWithFallback("GIGACHAD_PASSKEY_RATE_WINDOW_SEC", "MEDIA_PASSKEY_RATE_WINDOW_SEC", 60);
    static const int maxAttempts = getEnvIntWithFallback("GIGACHAD_PASSKEY_RATE_MAX_ATTEMPTS", "MEDIA_PASSKEY_RATE_MAX_ATTEMPTS", 12);
    const std::string username = toLower(trimWhitespace(usernameRaw));
    const std::string key = clientIp + "|" + (username.empty() ? "-" : username);

    const auto now = std::chrono::steady_clock::now();
    const auto cutoff = now - std::chrono::seconds(windowSeconds);

    std::lock_guard<std::mutex> lock(passkey_rate_mutex);
    auto& attempts = passkeyAttempts[key];
    while (!attempts.empty() && attempts.front() < cutoff) {
        attempts.pop_front();
    }
    if (static_cast<int>(attempts.size()) >= maxAttempts) {
        return false;
    }
    attempts.push_back(now);
    return true;
}

int passkeyLockoutThreshold() {
    static const int threshold = getEnvIntWithFallback("GIGACHAD_PASSKEY_LOCKOUT_THRESHOLD", "MEDIA_PASSKEY_LOCKOUT_THRESHOLD", 5);
    return threshold > 0 ? threshold : 5;
}

int passkeyLockoutDurationSec() {
    static const int secs = getEnvIntWithFallback("GIGACHAD_PASSKEY_LOCKOUT_SEC", "MEDIA_PASSKEY_LOCKOUT_SEC", 120);
    return secs > 0 ? secs : 120;
}

std::string passkeyFailureKey(const std::string& clientIpRaw, const std::string& usernameRaw) {
    const std::string clientIp = trimWhitespace(clientIpRaw.empty() ? "unknown" : clientIpRaw);
    const std::string username = toLower(trimWhitespace(usernameRaw));
    return clientIp + "|" + (username.empty() ? "-" : username);
}

bool isPasskeyTemporarilyLocked(const std::string& key, int& retryAfterSecOut) {
    retryAfterSecOut = 0;
    const auto now = std::chrono::steady_clock::now();
    const int threshold = passkeyLockoutThreshold();
    const int durationSec = passkeyLockoutDurationSec();
    std::lock_guard<std::mutex> lock(passkey_failure_mutex);
    auto& attempts = passkeyFailureAttempts[key];
    const auto cutoff = now - std::chrono::seconds(durationSec);
    while (!attempts.empty() && attempts.front() < cutoff) {
        attempts.pop_front();
    }
    if (static_cast<int>(attempts.size()) < threshold) {
        return false;
    }
    const auto unlockAt = attempts.front() + std::chrono::seconds(durationSec);
    const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(unlockAt - now).count();
    retryAfterSecOut = remaining > 0 ? static_cast<int>(remaining) : 1;
    return true;
}

void recordPasskeyFailure(const std::string& key) {
    const auto now = std::chrono::steady_clock::now();
    const int durationSec = passkeyLockoutDurationSec();
    std::lock_guard<std::mutex> lock(passkey_failure_mutex);
    auto& attempts = passkeyFailureAttempts[key];
    const auto cutoff = now - std::chrono::seconds(durationSec);
    while (!attempts.empty() && attempts.front() < cutoff) {
        attempts.pop_front();
    }
    attempts.push_back(now);
}

void clearPasskeyFailures(const std::string& key) {
    std::lock_guard<std::mutex> lock(passkey_failure_mutex);
    passkeyFailureAttempts.erase(key);
}

void appendAuthAuditEvent(
    const std::string& eventType,
    const std::string& routePath,
    const std::string& username,
    const std::string& tenantId,
    const std::string& clientIp,
    const std::string& outcome,
    const std::string& detail
) {
    std::lock_guard<std::mutex> lock(auth_audit_mutex);
    std::ofstream out(kAuthAuditFile, std::ios::app);
    if (!out.is_open()) {
        return;
    }
    out
        << "{"
        << "\"timestamp\":\"" << escapeJSONString(getCurrentTimestamp()) << "\","
        << "\"eventType\":\"" << escapeJSONString(eventType) << "\","
        << "\"route\":\"" << escapeJSONString(routePath) << "\","
        << "\"username\":\"" << escapeJSONString(username) << "\","
        << "\"tenantId\":\"" << escapeJSONString(tenantId) << "\","
        << "\"clientIp\":\"" << escapeJSONString(clientIp) << "\","
        << "\"outcome\":\"" << escapeJSONString(outcome) << "\","
        << "\"detail\":\"" << escapeJSONString(detail) << "\""
        << "}\n";
}

std::vector<uint8_t> base64UrlDecodeToBytes(const std::string& in) {
    std::string decoded = base64UrlDecodeToString(in);
    if (decoded.empty() && !in.empty()) {
        return {};
    }
    return std::vector<uint8_t>(decoded.begin(), decoded.end());
}

std::string decodePossiblyBase64UrlJson(const std::string& maybeB64OrJson) {
    if (maybeB64OrJson.empty()) return "";
    if (!maybeB64OrJson.empty() && maybeB64OrJson.front() == '{') {
        return maybeB64OrJson;
    }
    return base64UrlDecodeToString(maybeB64OrJson);
}

bool constantTimeEqual(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
}

struct ParsedCosePublicKey {
    int kty = 0;
    int alg = 0;
    bool hasAlg = false;
    int crv = 0;
    std::vector<uint8_t> x;
    std::vector<uint8_t> y;
};

bool readCborLength(const std::vector<uint8_t>& in, size_t& offset, uint8_t ai, uint64_t& outLen) {
    if (ai <= 23) {
        outLen = ai;
        return true;
    }
    if (ai == 24) {
        if (offset + 1 > in.size()) return false;
        outLen = in[offset++];
        return true;
    }
    if (ai == 25) {
        if (offset + 2 > in.size()) return false;
        outLen = (static_cast<uint64_t>(in[offset]) << 8) | static_cast<uint64_t>(in[offset + 1]);
        offset += 2;
        return true;
    }
    if (ai == 26) {
        if (offset + 4 > in.size()) return false;
        outLen = (static_cast<uint64_t>(in[offset]) << 24) |
                 (static_cast<uint64_t>(in[offset + 1]) << 16) |
                 (static_cast<uint64_t>(in[offset + 2]) << 8) |
                 static_cast<uint64_t>(in[offset + 3]);
        offset += 4;
        return true;
    }
    if (ai == 27) {
        if (offset + 8 > in.size()) return false;
        outLen = 0;
        for (int i = 0; i < 8; ++i) {
            outLen = (outLen << 8) | static_cast<uint64_t>(in[offset + i]);
        }
        offset += 8;
        return true;
    }
    return false;
}

bool readCborSignedInt(const std::vector<uint8_t>& in, size_t& offset, int64_t& outValue) {
    if (offset >= in.size()) return false;
    const uint8_t initial = in[offset++];
    const uint8_t major = static_cast<uint8_t>(initial >> 5);
    const uint8_t ai = static_cast<uint8_t>(initial & 0x1F);
    if (major != 0 && major != 1) return false;
    uint64_t val = 0;
    if (!readCborLength(in, offset, ai, val)) return false;
    if (major == 0) {
        if (val > static_cast<uint64_t>(INT64_MAX)) return false;
        outValue = static_cast<int64_t>(val);
        return true;
    }
    if (val > static_cast<uint64_t>(INT64_MAX)) return false;
    outValue = -1 - static_cast<int64_t>(val);
    return true;
}

bool readCborByteString(const std::vector<uint8_t>& in, size_t& offset, std::vector<uint8_t>& outBytes) {
    if (offset >= in.size()) return false;
    const uint8_t initial = in[offset++];
    const uint8_t major = static_cast<uint8_t>(initial >> 5);
    const uint8_t ai = static_cast<uint8_t>(initial & 0x1F);
    if (major != 2) return false;
    uint64_t len = 0;
    if (!readCborLength(in, offset, ai, len)) return false;
    if (offset + len > in.size()) return false;
    outBytes.assign(in.begin() + static_cast<std::ptrdiff_t>(offset), in.begin() + static_cast<std::ptrdiff_t>(offset + len));
    offset += static_cast<size_t>(len);
    return true;
}

bool skipCborItem(const std::vector<uint8_t>& in, size_t& offset) {
    if (offset >= in.size()) return false;
    const uint8_t initial = in[offset++];
    const uint8_t major = static_cast<uint8_t>(initial >> 5);
    const uint8_t ai = static_cast<uint8_t>(initial & 0x1F);

    uint64_t len = 0;
    switch (major) {
        case 0:
        case 1:
            return readCborLength(in, offset, ai, len);
        case 2:
        case 3:
            if (!readCborLength(in, offset, ai, len)) return false;
            if (offset + len > in.size()) return false;
            offset += static_cast<size_t>(len);
            return true;
        case 4:
            if (!readCborLength(in, offset, ai, len)) return false;
            for (uint64_t i = 0; i < len; ++i) {
                if (!skipCborItem(in, offset)) return false;
            }
            return true;
        case 5:
            if (!readCborLength(in, offset, ai, len)) return false;
            for (uint64_t i = 0; i < len; ++i) {
                if (!skipCborItem(in, offset)) return false;
                if (!skipCborItem(in, offset)) return false;
            }
            return true;
        case 6:
            if (!readCborLength(in, offset, ai, len)) return false;
            return skipCborItem(in, offset);
        case 7:
            if (ai <= 23) return true;
            if (ai == 24) return offset + 1 <= in.size() ? (++offset, true) : false;
            if (ai == 25) return offset + 2 <= in.size() ? (offset += 2, true) : false;
            if (ai == 26) return offset + 4 <= in.size() ? (offset += 4, true) : false;
            if (ai == 27) return offset + 8 <= in.size() ? (offset += 8, true) : false;
            return false;
        default:
            return false;
    }
}

bool parseCosePublicKey(const std::vector<uint8_t>& cbor, ParsedCosePublicKey& outKey, std::string& errorReason) {
    size_t offset = 0;
    if (offset >= cbor.size()) {
        errorReason = "COSE publicKey is empty";
        return false;
    }

    const uint8_t initial = cbor[offset++];
    const uint8_t major = static_cast<uint8_t>(initial >> 5);
    const uint8_t ai = static_cast<uint8_t>(initial & 0x1F);
    if (major != 5) {
        errorReason = "COSE publicKey must be CBOR map";
        return false;
    }

    uint64_t mapLen = 0;
    if (!readCborLength(cbor, offset, ai, mapLen)) {
        errorReason = "Invalid COSE map length";
        return false;
    }

    for (uint64_t i = 0; i < mapLen; ++i) {
        int64_t key = 0;
        if (!readCborSignedInt(cbor, offset, key)) {
            errorReason = "Invalid COSE map key";
            return false;
        }
        if (key == 1 || key == 3 || key == -1) {
            int64_t val = 0;
            if (!readCborSignedInt(cbor, offset, val)) {
                errorReason = "Invalid COSE integer parameter";
                return false;
            }
            if (key == 1) outKey.kty = static_cast<int>(val);
            if (key == 3) {
                outKey.alg = static_cast<int>(val);
                outKey.hasAlg = true;
            }
            if (key == -1) outKey.crv = static_cast<int>(val);
            continue;
        }
        if (key == -2 || key == -3) {
            std::vector<uint8_t> bytes;
            if (!readCborByteString(cbor, offset, bytes)) {
                errorReason = "Invalid COSE byte-string parameter";
                return false;
            }
            if (key == -2) outKey.x = std::move(bytes);
            else outKey.y = std::move(bytes);
            continue;
        }
        if (!skipCborItem(cbor, offset)) {
            errorReason = "Invalid COSE extra parameter";
            return false;
        }
    }

    if (offset != cbor.size()) {
        errorReason = "Trailing bytes in COSE publicKey";
        return false;
    }
    return true;
}

bool validatePasskeyCoseCredentialMaterial(const std::string& credentialKeyMaterial, std::string& errorReason) {
    const std::vector<uint8_t> coseBytes = base64UrlDecodeToBytes(credentialKeyMaterial);
    if (coseBytes.empty()) {
        errorReason = "COSE publicKey must be base64url encoded";
        return false;
    }
    ParsedCosePublicKey coseKey;
    if (!parseCosePublicKey(coseBytes, coseKey, errorReason)) {
        return false;
    }
    if (!coseKey.hasAlg) {
        errorReason = "COSE publicKey missing alg field";
        return false;
    }
    if (coseKey.alg != -7 && coseKey.alg != -8) {
        errorReason = "COSE alg must be ES256(-7) or EdDSA(-8)";
        return false;
    }
    if (coseKey.alg == -7) {
        if (coseKey.kty != 2 || coseKey.crv != 1 || coseKey.x.size() != 32 || coseKey.y.size() != 32) {
            errorReason = "Invalid ES256 COSE key parameters";
            return false;
        }
    } else if (coseKey.alg == -8) {
        if (coseKey.kty != 1 || coseKey.crv != 6 || coseKey.x.size() != 32) {
            errorReason = "Invalid EdDSA COSE key parameters";
            return false;
        }
    }
    return true;
}

bool parseDerLength(const std::vector<uint8_t>& der, size_t& offset, size_t& outLen) {
    if (offset >= der.size()) return false;
    const uint8_t first = der[offset++];
    if ((first & 0x80) == 0) {
        outLen = first;
        return true;
    }
    const size_t numBytes = static_cast<size_t>(first & 0x7F);
    if (numBytes == 0 || numBytes > sizeof(size_t) || offset + numBytes > der.size()) {
        return false;
    }
    size_t value = 0;
    for (size_t i = 0; i < numBytes; ++i) {
        value = (value << 8) | static_cast<size_t>(der[offset + i]);
    }
    offset += numBytes;
    outLen = value;
    return true;
}

bool normalizeDerIntegerToFixedWidth(const uint8_t* data, size_t len, size_t fixedLen, std::vector<uint8_t>& out, std::string& errorReason) {
    while (len > 0 && *data == 0x00) {
        ++data;
        --len;
    }
    if (len > fixedLen) {
        errorReason = "DER integer too large";
        return false;
    }
    out.assign(fixedLen, 0);
    if (len > 0) {
        std::memcpy(out.data() + (fixedLen - len), data, len);
    }
    return true;
}

bool parseEcdsaDerSignatureToRawRs(
    const std::vector<uint8_t>& derSig,
    size_t componentLen,
    std::vector<uint8_t>& rawSigOut,
    std::string& errorReason
) {
    if (derSig.size() == componentLen * 2) {
        rawSigOut = derSig;
        return true;
    }
    if (derSig.size() < 8 || derSig[0] != 0x30) {
        errorReason = "ECDSA signature must be DER sequence";
        return false;
    }

    size_t offset = 1;
    size_t seqLen = 0;
    if (!parseDerLength(derSig, offset, seqLen) || offset + seqLen != derSig.size()) {
        errorReason = "Invalid ECDSA DER sequence length";
        return false;
    }
    if (offset >= derSig.size() || derSig[offset++] != 0x02) {
        errorReason = "ECDSA DER missing R integer";
        return false;
    }
    size_t rLen = 0;
    if (!parseDerLength(derSig, offset, rLen) || offset + rLen > derSig.size()) {
        errorReason = "Invalid ECDSA DER R length";
        return false;
    }
    const uint8_t* rPtr = derSig.data() + offset;
    offset += rLen;

    if (offset >= derSig.size() || derSig[offset++] != 0x02) {
        errorReason = "ECDSA DER missing S integer";
        return false;
    }
    size_t sLen = 0;
    if (!parseDerLength(derSig, offset, sLen) || offset + sLen != derSig.size()) {
        errorReason = "Invalid ECDSA DER S length";
        return false;
    }
    const uint8_t* sPtr = derSig.data() + offset;

    std::vector<uint8_t> rNorm;
    std::vector<uint8_t> sNorm;
    if (!normalizeDerIntegerToFixedWidth(rPtr, rLen, componentLen, rNorm, errorReason)) return false;
    if (!normalizeDerIntegerToFixedWidth(sPtr, sLen, componentLen, sNorm, errorReason)) return false;

    rawSigOut.reserve(componentLen * 2);
    rawSigOut.insert(rawSigOut.end(), rNorm.begin(), rNorm.end());
    rawSigOut.insert(rawSigOut.end(), sNorm.begin(), sNorm.end());
    return true;
}

#ifdef _WIN32
bool verifyEs256SignatureWindows(
    const ParsedCosePublicKey& coseKey,
    const std::vector<uint8_t>& signingData,
    const std::vector<uint8_t>& derSignature,
    std::string& errorReason
) {
    if (coseKey.x.size() != 32 || coseKey.y.size() != 32) {
        errorReason = "ES256 key must contain 32-byte x and y coordinates";
        return false;
    }

    std::vector<uint8_t> rawSig;
    if (!parseEcdsaDerSignatureToRawRs(derSignature, 32, rawSig, errorReason)) {
        return false;
    }

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
    std::vector<uint8_t> blob(sizeof(BCRYPT_ECCKEY_BLOB) + 64, 0);
    auto* header = reinterpret_cast<BCRYPT_ECCKEY_BLOB*>(blob.data());
    header->dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
    header->cbKey = 32;
    std::memcpy(blob.data() + sizeof(BCRYPT_ECCKEY_BLOB), coseKey.x.data(), 32);
    std::memcpy(blob.data() + sizeof(BCRYPT_ECCKEY_BLOB) + 32, coseKey.y.data(), 32);

    NTSTATUS status = BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0);
    if (status < 0 || alg == nullptr) {
        errorReason = "Failed to open ES256 crypto provider";
        return false;
    }
    status = BCryptImportKeyPair(
        alg,
        nullptr,
        BCRYPT_ECCPUBLIC_BLOB,
        &key,
        blob.data(),
        static_cast<ULONG>(blob.size()),
        0
    );
    if (status < 0 || key == nullptr) {
        BCryptCloseAlgorithmProvider(alg, 0);
        errorReason = "Failed to import ES256 COSE public key";
        return false;
    }

    const std::string signingInput(reinterpret_cast<const char*>(signingData.data()), signingData.size());
    const auto digest = sha256(signingInput);
    status = BCryptVerifySignature(
        key,
        nullptr,
        const_cast<PUCHAR>(reinterpret_cast<const UCHAR*>(digest.data())),
        static_cast<ULONG>(digest.size()),
        const_cast<PUCHAR>(reinterpret_cast<const UCHAR*>(rawSig.data())),
        static_cast<ULONG>(rawSig.size()),
        0
    );

    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (status < 0) {
        errorReason = "ES256 signature verification failed";
        return false;
    }
    return true;
}
#endif

bool verifyPasskeyCoseSignature(
    const ParsedCosePublicKey& coseKey,
    const std::vector<uint8_t>& signingData,
    const std::vector<uint8_t>& signature,
    std::string& errorReason
) {
    if (coseKey.alg == -7) {
#ifdef _WIN32
        return verifyEs256SignatureWindows(coseKey, signingData, signature, errorReason);
#else
        errorReason = "ES256 verification is only enabled on Windows in this build";
        return false;
#endif
    }
    if (coseKey.alg == -8) {
        errorReason = "EdDSA COSE verification is not enabled in this build";
        return false;
    }
    errorReason = "Unsupported COSE algorithm";
    return false;
}

bool validatePasskeyClientData(
    const std::string& clientDataJsonB64,
    const std::string& expectedType,
    const std::string& expectedChallenge,
    const std::string& expectedOrigin,
    std::string& errorReason
) {
    std::string clientDataJson = decodePossiblyBase64UrlJson(clientDataJsonB64);
    if (clientDataJson.empty()) {
        errorReason = "Invalid clientDataJSON";
        return false;
    }

    const std::string gotType = trimWhitespace(extractJSONValue(clientDataJson, "type"));
    const std::string gotChallenge = trimWhitespace(extractJSONValue(clientDataJson, "challenge"));
    const std::string gotOrigin = trimWhitespace(extractJSONValue(clientDataJson, "origin"));
    if (gotType != expectedType) {
        errorReason = "clientDataJSON.type mismatch";
        return false;
    }
    if (gotChallenge != expectedChallenge) {
        errorReason = "clientDataJSON.challenge mismatch";
        return false;
    }
    if (!expectedOrigin.empty() && gotOrigin != expectedOrigin) {
        errorReason = "clientDataJSON.origin mismatch";
        return false;
    }
    return true;
}

bool validatePasskeyAuthenticatorData(
    const std::string& authenticatorDataB64,
    const std::string& rpId,
    int& signCountOut,
    std::string& errorReason
) {
    const std::vector<uint8_t> authData = base64UrlDecodeToBytes(authenticatorDataB64);
    if (authData.size() < 37) {
        errorReason = "Invalid authenticatorData";
        return false;
    }

    const auto rpHash = sha256(rpId);
    for (size_t i = 0; i < rpHash.size(); ++i) {
        if (authData[i] != rpHash[i]) {
            errorReason = "authenticatorData.rpIdHash mismatch";
            return false;
        }
    }

    const uint8_t flags = authData[32];
    if ((flags & 0x01) == 0) {
        errorReason = "authenticatorData user presence flag missing";
        return false;
    }

    signCountOut = (static_cast<int>(authData[33]) << 24) |
                   (static_cast<int>(authData[34]) << 16) |
                   (static_cast<int>(authData[35]) << 8) |
                   static_cast<int>(authData[36]);
    if (signCountOut < 0) signCountOut = 0;
    return true;
}

bool validatePasskeySignatureHmac(
    const std::string& credentialKey,
    const std::string& authenticatorDataB64,
    const std::string& clientDataJsonB64,
    const std::string& signatureB64,
    std::string& errorReason
) {
    if (credentialKey.empty()) {
        errorReason = "Credential key is missing";
        return false;
    }

    const std::vector<uint8_t> authData = base64UrlDecodeToBytes(authenticatorDataB64);
    if (authData.empty()) {
        errorReason = "Invalid authenticatorData for signature validation";
        return false;
    }
    const std::string clientDataJson = decodePossiblyBase64UrlJson(clientDataJsonB64);
    if (clientDataJson.empty()) {
        errorReason = "Invalid clientDataJSON for signature validation";
        return false;
    }
    const std::vector<uint8_t> providedSig = base64UrlDecodeToBytes(signatureB64);
    if (providedSig.empty()) {
        errorReason = "Invalid signature encoding";
        return false;
    }

    const auto clientDataHash = sha256(clientDataJson);
    std::string signingMessage;
    signingMessage.reserve(authData.size() + clientDataHash.size());
    signingMessage.append(reinterpret_cast<const char*>(authData.data()), authData.size());
    signingMessage.append(reinterpret_cast<const char*>(clientDataHash.data()), clientDataHash.size());

    const auto expectedSig = hmacSha256(credentialKey, signingMessage);
    std::vector<uint8_t> expectedSigBytes(expectedSig.begin(), expectedSig.end());
    if (!constantTimeEqual(expectedSigBytes, providedSig)) {
        errorReason = "Signature verification failed";
        return false;
    }
    return true;
}

bool validatePasskeySignatureEs256(
    const std::string& credentialPublicKey,
    const std::string& authenticatorDataB64,
    const std::string& clientDataJsonB64,
    const std::string& signatureB64,
    std::string& errorReason
) {
    if (!validatePasskeyCredentialMaterial(credentialPublicKey, errorReason)) {
        return false;
    }

    const std::vector<uint8_t> authData = base64UrlDecodeToBytes(authenticatorDataB64);
    if (authData.empty()) {
        errorReason = "Invalid authenticatorData for signature validation";
        return false;
    }
    const std::string clientDataJson = decodePossiblyBase64UrlJson(clientDataJsonB64);
    if (clientDataJson.empty()) {
        errorReason = "Invalid clientDataJSON for signature validation";
        return false;
    }
    const std::vector<uint8_t> providedSig = base64UrlDecodeToBytes(signatureB64);
    if (providedSig.empty()) {
        errorReason = "Invalid signature encoding";
        return false;
    }

    const std::vector<uint8_t> coseBytes = base64UrlDecodeToBytes(credentialPublicKey);
    ParsedCosePublicKey coseKey;
    if (!parseCosePublicKey(coseBytes, coseKey, errorReason)) {
        return false;
    }

    const auto clientDataHash = sha256(clientDataJson);
    std::vector<uint8_t> signingMessage;
    signingMessage.reserve(authData.size() + clientDataHash.size());
    signingMessage.insert(signingMessage.end(), authData.begin(), authData.end());
    signingMessage.insert(signingMessage.end(), clientDataHash.begin(), clientDataHash.end());

    return verifyPasskeyCoseSignature(coseKey, signingMessage, providedSig, errorReason);
}

bool validatePasskeySignature(
    const std::string& credentialKeyMaterial,
    const std::string& authenticatorDataB64,
    const std::string& clientDataJsonB64,
    const std::string& signatureB64,
    std::string& errorReason
) {
    if (passkeySignatureMode() == "es256") {
        return validatePasskeySignatureEs256(
            credentialKeyMaterial,
            authenticatorDataB64,
            clientDataJsonB64,
            signatureB64,
            errorReason
        );
    }
    return validatePasskeySignatureHmac(
        credentialKeyMaterial,
        authenticatorDataB64,
        clientDataJsonB64,
        signatureB64,
        errorReason
    );
}

std::string generatePasskeyChallenge() {
    std::array<uint8_t, 24> bytes{};
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> dis(0, 255);
    for (auto& b : bytes) {
        b = static_cast<uint8_t>(dis(gen));
    }
    return base64UrlEncode(bytes.data(), bytes.size());
}

void cleanupExpiredPasskeyChallengesLocked(int now) {
    for (auto it = g_passkeyChallenges.begin(); it != g_passkeyChallenges.end();) {
        if (it->second.expiresAt <= now) {
            it = g_passkeyChallenges.erase(it);
        } else {
            ++it;
        }
    }
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

std::string buildEventFramePayload(const std::string& type, const std::string& payloadJson, const std::string& roomId = "", const std::string& tenantId = "default") {
    std::stringstream ss;
    ss << "{"
       << "\"type\":\"" << escapeJSONString(type) << "\","
       << "\"roomId\":\"" << escapeJSONString(roomId) << "\","
       << "\"tenantId\":\"" << escapeJSONString(normalizeTenantId(tenantId)) << "\","
       << "\"timestamp\":\"" << escapeJSONString(getCurrentTimestamp()) << "\","
       << "\"payload\":" << (payloadJson.empty() ? "{}" : payloadJson)
       << "}";
    return ss.str();
}

void broadcastFrameLocal(const std::string& framePayload, const std::string& roomId = "", const std::string& tenantId = "default") {
    std::vector<WebSocketClient> snapshot;
    {
        std::lock_guard<std::mutex> lock(ws_mutex);
        snapshot = wsClients;
    }

    std::vector<int> deadSockets;
    for (const auto& client : snapshot) {
        const std::string clientTenant = normalizeTenantId(client.tenantId);
        const std::string eventTenant = normalizeTenantId(tenantId);
        if (clientTenant != eventTenant) {
            if (eventBusDebugEnabled()) {
                std::cerr << "[event-bus-debug] skip socket=" << client.socket
                          << " reason=tenant-mismatch clientTenant=" << clientTenant
                          << " eventTenant=" << eventTenant << std::endl;
            }
            continue;
        }
        if (!roomId.empty() && !client.roomId.empty() && client.roomId != roomId) {
            if (eventBusDebugEnabled()) {
                std::cerr << "[event-bus-debug] skip socket=" << client.socket
                          << " reason=room-mismatch clientRoom=" << client.roomId
                          << " eventRoom=" << roomId << std::endl;
            }
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

void broadcastEventLocal(const std::string& type, const std::string& payloadJson, const std::string& roomId = "", const std::string& tenantId = "default") {
    const std::string framePayload = buildEventFramePayload(type, payloadJson, roomId, tenantId);
    broadcastFrameLocal(framePayload, roomId, tenantId);
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
                const std::string tenantId = normalizeTenantId(extractJSONValue(framePayload, "tenantId"));
                if (eventBusDebugEnabled()) {
                    std::cerr << "[event-bus-debug] redis message room=" << roomId
                              << " tenant=" << tenantId
                              << " type=" << extractJSONValue(framePayload, "type") << std::endl;
                }
                broadcastFrameLocal(framePayload, roomId, tenantId);
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

void dispatchEvent(const std::string& type, const std::string& payloadJson, const std::string& roomId = "", const std::string& tenantId = "default") {
    if (eventBusModeIsRedis()) {
        ensureRedisEventBusThreadStarted();
        const std::string framePayload = buildEventFramePayload(type, payloadJson, roomId, tenantId);
        if (publishEventToRedis(framePayload)) {
            return;
        }
        if (!g_eventBusRedisPublishWarned.exchange(true)) {
            std::cerr << "[event-bus] redis publish failed, using local fallback" << std::endl;
        }
        broadcastFrameLocal(framePayload, roomId, tenantId);
        return;
    }

    broadcastEventLocal(type, payloadJson, roomId, tenantId);
}

void broadcastEvent(const std::string& type, const std::string& payloadJson, const std::string& roomId = "", const std::string& tenantId = "default") {
    dispatchEvent(type, payloadJson, roomId, tenantId);
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

std::string extractApiTokenFromRequest(const HTTPRequest& req);
std::string resolveTenantFromRequest(const HTTPRequest& req);

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
        const std::string tenantFilter = resolveTenantFromRequest(req);
        std::lock_guard<std::mutex> lock(messages_mutex);
        
        std::stringstream ss;
        ss << "[";
        bool first = true;
        for (const auto& msg : messages) {
            if (normalizeTenantId(msg.tenantId) != tenantFilter) {
                continue;
            }
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
        std::string tenantId = resolveTenantFromRequest(req);
        if (!jwtEnabled() && tenantId == "default") {
            tenantId = normalizeTenantId(extractJSONValue(req.body, "tenantId"));
        }
        msg.tenantId = normalizeTenantId(tenantId);
        msg.timestamp = getCurrentTimestamp();
        
        messages.push_back(msg);
        persistMessagesToDiskUnlocked();
        broadcastEvent("chat.message.created", msg.toJSON(), msg.roomId, msg.tenantId);
        
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

HTTPResponse handleAuthAPI(const HTTPRequest& req) {
    HTTPResponse res;
    const std::string routePath = getPathOnly(req.path);
    res.headers["Content-Type"] = "application/json";
    res.headers["Access-Control-Allow-Origin"] = "*";

    if (!jwtEnabled()) {
        res.status = 400;
        res.statusText = "Bad Request";
        res.body = "{\"error\":\"JWT auth is disabled. Set GIGACHAD_JWT_SECRET or MEDIA_JWT_SECRET.\"}";
        return res;
    }

    const bool isPasskeyRoute = routePath.find("/api/auth/passkey/") == 0;
    std::string passkeyUsername = normalizeUsername(extractJSONValue(req.body, "username"));
    if (passkeyUsername.empty()) passkeyUsername = normalizeUsername(extractJSONValue(req.body, "user"));
    std::string passkeyTenant = normalizeTenantId(extractJSONValue(req.body, "tenantId"));
    const std::string passkeyClientIp = resolveClientIpFromHeaders(req.headers);
    const std::string passkeyFailKey = passkeyFailureKey(passkeyClientIp, passkeyUsername);
    auto passkeyFail = [&](int httpStatus, const std::string& statusText, const std::string& err, const std::string& detail) {
        if (isPasskeyRoute && routePath == "/api/auth/passkey/login") {
            recordPasskeyFailure(passkeyFailKey);
            appendAuthAuditEvent("passkey.login", routePath, passkeyUsername, passkeyTenant, passkeyClientIp, "failure", detail);
        }
        res.status = httpStatus;
        res.statusText = statusText;
        res.body = "{\"error\":\"" + escapeJSONString(err) + "\"}";
        return res;
    };

    if (isPasskeyRoute) {
        if (!consumePasskeyRateLimit(passkeyClientIp, passkeyUsername)) {
            return passkeyFail(429, "Too Many Requests", "Passkey rate limit exceeded", "rate_limit");
        }
        if (routePath == "/api/auth/passkey/login") {
            int retryAfterSec = 0;
            if (isPasskeyTemporarilyLocked(passkeyFailKey, retryAfterSec)) {
                res.headers["Retry-After"] = std::to_string(retryAfterSec);
                appendAuthAuditEvent("passkey.login", routePath, passkeyUsername, passkeyTenant, passkeyClientIp, "blocked", "lockout");
                return passkeyFail(429, "Too Many Requests", "Passkey temporarily locked due to repeated failures", "lockout");
            }
        }
    }

    if (req.method == "POST" && routePath == "/api/auth/passkey/challenge") {
        std::string username = normalizeUsername(extractJSONValue(req.body, "username"));
        if (username.empty()) {
            username = normalizeUsername(extractJSONValue(req.body, "user"));
        }
        if (username.empty()) {
            res.status = 400;
            res.statusText = "Bad Request";
            res.body = "{\"error\":\"Missing username\"}";
            return res;
        }

        std::string flow = toLower(trimWhitespace(extractJSONValue(req.body, "flow")));
        if (flow != "register" && flow != "login") {
            flow = "login";
        }

        std::string role = toLower(trimWhitespace(extractJSONValue(req.body, "role")));
        if (role != "admin" && role != "moderator" && role != "user") {
            role = "user";
        }
        const std::string tenantId = normalizeTenantId(extractJSONValue(req.body, "tenantId"));
        const std::string rpId = !passkeyRpId().empty() ? passkeyRpId() : trimWhitespace(extractJSONValue(req.body, "rpId"));
        std::string origin = trimWhitespace(extractJSONValue(req.body, "origin"));
        if (origin.empty()) {
            origin = trimWhitespace(getHeaderValue(req.headers, "Origin"));
        }
        if (!origin.empty() && !isPasskeyOriginAllowed(origin)) {
            res.status = 403;
            res.statusText = "Forbidden";
            res.body = "{\"error\":\"Origin is not allowed for passkey flow\"}";
            return res;
        }

        const std::string challengeId = generateId("pk_ch_");
        const std::string challenge = generatePasskeyChallenge();
        const int ttlSec = passkeyChallengeTtlSec();
        const int expiresAt = nowEpochSec() + ttlSec;

        {
            std::lock_guard<std::mutex> lock(g_auth_mutex);
            cleanupExpiredPasskeyChallengesLocked(nowEpochSec());
            g_passkeyChallenges[challengeId] = PasskeyChallengeState{
                challenge,
                username,
                role,
                tenantId,
                rpId,
                flow,
                expiresAt
            };
        }
        appendAuthAuditEvent("passkey.challenge", routePath, username, tenantId, passkeyClientIp, "success", flow);

        res.body =
            "{"
            "\"challengeId\":\"" + escapeJSONString(challengeId) + "\","
            "\"challenge\":\"" + escapeJSONString(challenge) + "\","
            "\"username\":\"" + escapeJSONString(username) + "\","
            "\"flow\":\"" + escapeJSONString(flow) + "\","
            "\"tenantId\":\"" + escapeJSONString(tenantId) + "\","
            "\"rpId\":\"" + escapeJSONString(rpId) + "\","
            "\"strictMetadata\":" + std::string(passkeyStrictMetadataEnabled() ? "true" : "false") + ","
            "\"expiresIn\":" + std::to_string(ttlSec) +
            "}";
        return res;
    }

    if (req.method == "POST" && routePath == "/api/auth/passkey/register") {
        const std::string challengeId = trimWhitespace(extractJSONValue(req.body, "challengeId"));
        const std::string challenge = trimWhitespace(extractJSONValue(req.body, "challenge"));
        std::string username = normalizeUsername(extractJSONValue(req.body, "username"));
        if (username.empty()) username = normalizeUsername(extractJSONValue(req.body, "user"));
        const std::string credentialId = trimWhitespace(extractJSONValue(req.body, "credentialId"));
        const std::string publicKey = trimWhitespace(extractJSONValue(req.body, "publicKey"));
        int signCount = extractJSONIntValue(req.body, "signCount", 0);
        if (signCount < 0) signCount = 0;
        std::string role = toLower(trimWhitespace(extractJSONValue(req.body, "role")));
        if (role != "admin" && role != "moderator" && role != "user") {
            role = "user";
        }
        std::string tenantId = normalizeTenantId(extractJSONValue(req.body, "tenantId"));
        const std::string reqRpId = trimWhitespace(extractJSONValue(req.body, "rpId"));
        std::string reqOrigin = trimWhitespace(extractJSONValue(req.body, "origin"));
        if (reqOrigin.empty()) reqOrigin = trimWhitespace(getHeaderValue(req.headers, "Origin"));
        const std::string clientDataType = trimWhitespace(extractJSONValue(req.body, "clientDataType"));
        const std::string clientDataJsonB64 = trimWhitespace(extractJSONValue(req.body, "clientDataJSON"));
        const std::string authenticatorDataB64 = trimWhitespace(extractJSONValue(req.body, "authenticatorData"));
        int authDataSignCount = 0;
        std::string passkeyValidationError;

        if (challengeId.empty() || challenge.empty() || username.empty() || credentialId.empty() || publicKey.empty()) {
            res.status = 400;
            res.statusText = "Bad Request";
            res.body = "{\"error\":\"Missing required fields\"}";
            return res;
        }
        if (!validatePasskeyCredentialMaterial(publicKey, passkeyValidationError)) {
            res.status = 400;
            res.statusText = "Bad Request";
            res.body = "{\"error\":\"" + escapeJSONString(passkeyValidationError) + "\"}";
            return res;
        }
        if (passkeyStrictMetadataEnabled()) {
            if (reqRpId.empty() || reqOrigin.empty() || clientDataType.empty() ||
                clientDataJsonB64.empty() || authenticatorDataB64.empty()) {
                res.status = 400;
                res.statusText = "Bad Request";
                res.body = "{\"error\":\"Missing strict passkey metadata: rpId/origin/clientDataType/clientDataJSON/authenticatorData\"}";
                return res;
            }
            if (clientDataType != "webauthn.create") {
                res.status = 400;
                res.statusText = "Bad Request";
                res.body = "{\"error\":\"Invalid clientDataType for register\"}";
                return res;
            }
            if (!isPasskeyOriginAllowed(reqOrigin)) {
                res.status = 403;
                res.statusText = "Forbidden";
                res.body = "{\"error\":\"Origin is not allowed for passkey flow\"}";
                return res;
            }
            if (!validatePasskeyClientData(clientDataJsonB64, "webauthn.create", challenge, reqOrigin, passkeyValidationError)) {
                res.status = 401;
                res.statusText = "Unauthorized";
                res.body = "{\"error\":\"" + escapeJSONString(passkeyValidationError) + "\"}";
                return res;
            }
            if (!validatePasskeyAuthenticatorData(authenticatorDataB64, reqRpId, authDataSignCount, passkeyValidationError)) {
                res.status = 401;
                res.statusText = "Unauthorized";
                res.body = "{\"error\":\"" + escapeJSONString(passkeyValidationError) + "\"}";
                return res;
            }
            if (signCount < authDataSignCount) {
                signCount = authDataSignCount;
            }
        }

        const int now = nowEpochSec();
        bool credentialChanged = false;
        {
            std::lock_guard<std::mutex> lock(g_auth_mutex);
            cleanupExpiredPasskeyChallengesLocked(now);
            auto it = g_passkeyChallenges.find(challengeId);
            if (it == g_passkeyChallenges.end()) {
                res.status = 401;
                res.statusText = "Unauthorized";
                res.body = "{\"error\":\"Invalid or expired challenge\"}";
                return res;
            }
            const PasskeyChallengeState challengeState = it->second;
            if (challengeState.flow != "register" ||
                challengeState.challenge != challenge ||
                normalizeUsername(challengeState.username) != username ||
                challengeState.expiresAt <= now) {
                g_passkeyChallenges.erase(it);
                res.status = 401;
                res.statusText = "Unauthorized";
                res.body = "{\"error\":\"Challenge verification failed\"}";
                return res;
            }
            if (passkeyStrictMetadataEnabled()) {
                if (!challengeState.rpId.empty() && challengeState.rpId != reqRpId) {
                    g_passkeyChallenges.erase(it);
                    res.status = 401;
                    res.statusText = "Unauthorized";
                    res.body = "{\"error\":\"rpId mismatch\"}";
                    return res;
                }
            }
            g_passkeyChallenges.erase(it);

            if (tenantId == "default") {
                tenantId = normalizeTenantId(challengeState.tenantId);
            }
            if (role == "user" && challengeState.role == "admin") {
                role = "admin";
            } else if (role == "user" && challengeState.role == "moderator") {
                role = "moderator";
            }

            auto& creds = g_passkeyCredentialsByUser[normalizeUsername(username)];
            auto credIt = std::find_if(creds.begin(), creds.end(), [&](const PasskeyCredential& c) {
                return c.credentialId == credentialId &&
                       normalizeTenantId(c.tenantId) == normalizeTenantId(tenantId);
            });
            if (credIt == creds.end()) {
                PasskeyCredential cred;
                cred.credentialId = credentialId;
                cred.publicKey = publicKey;
                cred.signCount = signCount;
                cred.role = role;
                cred.tenantId = tenantId;
                cred.createdAt = getCurrentTimestamp();
                creds.push_back(cred);
                credentialChanged = true;
            } else {
                credIt->publicKey = publicKey;
                credIt->signCount = std::max(credIt->signCount, signCount);
                credIt->role = role;
                credIt->tenantId = tenantId;
                credentialChanged = true;
            }
        }
        if (credentialChanged) {
            persistPasskeyCredentialsToSqlite();
            persistPasskeyCredentialsToDisk();
        }
        appendAuthAuditEvent("passkey.register", routePath, username, tenantId, passkeyClientIp, "success", "ok");

        res.body =
            "{"
            "\"status\":\"registered\","
            "\"username\":\"" + escapeJSONString(username) + "\","
            "\"credentialId\":\"" + escapeJSONString(credentialId) + "\","
            "\"role\":\"" + escapeJSONString(role) + "\","
            "\"tenantId\":\"" + escapeJSONString(tenantId) + "\","
            "\"rpId\":\"" + escapeJSONString(reqRpId) + "\","
            "\"origin\":\"" + escapeJSONString(reqOrigin) + "\","
            "\"note\":\"phase2_clientdata_authdata_checks_no_signature_verification\""
            "}";
        return res;
    }

    if (req.method == "POST" && routePath == "/api/auth/passkey/login") {
        const std::string challengeId = trimWhitespace(extractJSONValue(req.body, "challengeId"));
        const std::string challenge = trimWhitespace(extractJSONValue(req.body, "challenge"));
        std::string username = normalizeUsername(extractJSONValue(req.body, "username"));
        if (username.empty()) username = normalizeUsername(extractJSONValue(req.body, "user"));
        const std::string credentialId = trimWhitespace(extractJSONValue(req.body, "credentialId"));
        int signCount = extractJSONIntValue(req.body, "signCount", -1);
        const std::string reqRpId = trimWhitespace(extractJSONValue(req.body, "rpId"));
        std::string reqOrigin = trimWhitespace(extractJSONValue(req.body, "origin"));
        if (reqOrigin.empty()) reqOrigin = trimWhitespace(getHeaderValue(req.headers, "Origin"));
        const std::string clientDataType = trimWhitespace(extractJSONValue(req.body, "clientDataType"));
        const std::string clientDataJsonB64 = trimWhitespace(extractJSONValue(req.body, "clientDataJSON"));
        const std::string authenticatorDataB64 = trimWhitespace(extractJSONValue(req.body, "authenticatorData"));
        const std::string signatureB64 = trimWhitespace(extractJSONValue(req.body, "signature"));
        int authDataSignCount = 0;
        std::string passkeyValidationError;
        passkeyUsername = username;
        passkeyTenant = normalizeTenantId(extractJSONValue(req.body, "tenantId"));

        if (challengeId.empty() || challenge.empty() || username.empty() || credentialId.empty()) {
            return passkeyFail(400, "Bad Request", "Missing required fields", "missing_required");
        }
        if (passkeyStrictMetadataEnabled()) {
            if (reqRpId.empty() || reqOrigin.empty() || clientDataType.empty() ||
                clientDataJsonB64.empty() || authenticatorDataB64.empty() || signatureB64.empty()) {
                return passkeyFail(400, "Bad Request", "Missing strict passkey metadata: rpId/origin/clientDataType/clientDataJSON/authenticatorData/signature", "missing_strict_metadata");
            }
            if (clientDataType != "webauthn.get") {
                return passkeyFail(400, "Bad Request", "Invalid clientDataType for login", "invalid_client_data_type");
            }
            if (!isPasskeyOriginAllowed(reqOrigin)) {
                return passkeyFail(403, "Forbidden", "Origin is not allowed for passkey flow", "origin_not_allowed");
            }
            if (!validatePasskeyClientData(clientDataJsonB64, "webauthn.get", challenge, reqOrigin, passkeyValidationError)) {
                return passkeyFail(401, "Unauthorized", passkeyValidationError, "client_data_invalid");
            }
            if (!validatePasskeyAuthenticatorData(authenticatorDataB64, reqRpId, authDataSignCount, passkeyValidationError)) {
                return passkeyFail(401, "Unauthorized", passkeyValidationError, "authenticator_data_invalid");
            }
            if (signCount < 0 || signCount < authDataSignCount) {
                signCount = authDataSignCount;
            }
        }

        PasskeyCredential selected;
        bool found = false;
        bool signCountChanged = false;
        std::string expectedTenantForLogin = "default";
        const int now = nowEpochSec();
        {
            std::lock_guard<std::mutex> lock(g_auth_mutex);
            cleanupExpiredPasskeyChallengesLocked(now);
            auto challengeIt = g_passkeyChallenges.find(challengeId);
            if (challengeIt == g_passkeyChallenges.end()) {
                return passkeyFail(401, "Unauthorized", "Invalid or expired challenge", "challenge_missing_or_expired");
            }
            const PasskeyChallengeState challengeState = challengeIt->second;
            if (challengeState.flow != "login" ||
                challengeState.challenge != challenge ||
                normalizeUsername(challengeState.username) != username ||
                challengeState.expiresAt <= now) {
                g_passkeyChallenges.erase(challengeIt);
                return passkeyFail(401, "Unauthorized", "Challenge verification failed", "challenge_verify_failed");
            }
            if (passkeyStrictMetadataEnabled()) {
                if (!challengeState.rpId.empty() && challengeState.rpId != reqRpId) {
                    g_passkeyChallenges.erase(challengeIt);
                    return passkeyFail(401, "Unauthorized", "rpId mismatch", "rp_id_mismatch");
                }
            }
            g_passkeyChallenges.erase(challengeIt);

            const std::string lookupTenant = normalizeTenantId(challengeState.tenantId);
            expectedTenantForLogin = lookupTenant;
            auto userCredsIt = g_passkeyCredentialsByUser.find(normalizeUsername(username));
            if (userCredsIt != g_passkeyCredentialsByUser.end()) {
                auto& creds = userCredsIt->second;
                auto credIt = std::find_if(creds.begin(), creds.end(), [&](const PasskeyCredential& c) {
                    return c.credentialId == credentialId &&
                           normalizeTenantId(c.tenantId) == lookupTenant;
                });
                if (credIt != creds.end()) {
                    if (passkeyStrictMetadataEnabled()) {
                        if (!validatePasskeySignature(
                                credIt->publicKey,
                                authenticatorDataB64,
                                clientDataJsonB64,
                                signatureB64,
                                passkeyValidationError)) {
                            return passkeyFail(401, "Unauthorized", passkeyValidationError, "signature_invalid");
                        }
                        if (passkeyCounterStrictEnabled()) {
                            // Detect potential cloned authenticator or replay when counters are active.
                            if (authDataSignCount > 0 && credIt->signCount > 0 && authDataSignCount <= credIt->signCount) {
                                return passkeyFail(401, "Unauthorized", "Passkey signCount rollback/replay detected", "counter_rollback");
                            }
                        }
                        signCount = authDataSignCount;
                    }
                    if (signCount < 0) {
                        signCount = credIt->signCount + 1;
                    }
                    if (signCount <= credIt->signCount) {
                        signCount = credIt->signCount + 1;
                    }
                    credIt->signCount = signCount;
                    selected = *credIt;
                    found = true;
                    signCountChanged = true;
                }
            }
        }
        if (signCountChanged) {
            persistPasskeyCredentialsToSqlite();
            persistPasskeyCredentialsToDisk();
        }

        if (!found) {
            return passkeyFail(401, "Unauthorized", "Passkey credential not found", "credential_not_found");
        }
        if (normalizeTenantId(selected.tenantId) != normalizeTenantId(expectedTenantForLogin)) {
            return passkeyFail(401, "Unauthorized", "Passkey tenant isolation check failed", "tenant_mismatch");
        }

        const std::string accessToken = buildJwtToken("access", username, selected.role, selected.tenantId, jwtAccessTtlSec());
        const std::string refreshToken = buildJwtToken("refresh", username, selected.role, selected.tenantId, jwtRefreshTtlSec());
        clearPasskeyFailures(passkeyFailKey);
        appendAuthAuditEvent("passkey.login", routePath, username, selected.tenantId, passkeyClientIp, "success", "ok");
        res.body =
            "{"
            "\"tokenType\":\"Bearer\","
            "\"accessToken\":\"" + escapeJSONString(accessToken) + "\","
            "\"refreshToken\":\"" + escapeJSONString(refreshToken) + "\","
            "\"expiresIn\":" + std::to_string(jwtAccessTtlSec()) + ","
            "\"role\":\"" + escapeJSONString(selected.role) + "\","
            "\"tenantId\":\"" + escapeJSONString(selected.tenantId) + "\","
            "\"username\":\"" + escapeJSONString(username) + "\""
            "}";
        return res;
    }

    if (req.method == "POST" && routePath == "/api/auth/login") {
        const std::string gateToken = trimWhitespace(getEnvStringWithFallback("GIGACHAD_AUTH_LOGIN_TOKEN", "MEDIA_AUTH_LOGIN_TOKEN", ""));
        if (!gateToken.empty()) {
            std::string provided = getHeaderValue(req.headers, "X-Login-Token");
            if (provided.empty()) provided = extractJSONValue(req.body, "loginToken");
            if (provided != gateToken) {
                res.status = 401;
                res.statusText = "Unauthorized";
                res.body = "{\"error\":\"Invalid login token\"}";
                return res;
            }
        }

        std::string username = extractJSONValue(req.body, "username");
        if (username.empty()) username = extractJSONValue(req.body, "user");
        if (username.empty()) {
            res.status = 400;
            res.statusText = "Bad Request";
            res.body = "{\"error\":\"Missing username\"}";
            return res;
        }

        std::string role = toLower(trimWhitespace(extractJSONValue(req.body, "role")));
        if (role != "admin" && role != "moderator" && role != "user") {
            role = "user";
        }
        std::string tenantId = normalizeTenantId(extractJSONValue(req.body, "tenantId"));
        const std::string accessToken = buildJwtToken("access", username, role, tenantId, jwtAccessTtlSec());
        const std::string refreshToken = buildJwtToken("refresh", username, role, tenantId, jwtRefreshTtlSec());
        res.body =
            "{"
            "\"tokenType\":\"Bearer\","
            "\"accessToken\":\"" + escapeJSONString(accessToken) + "\","
            "\"refreshToken\":\"" + escapeJSONString(refreshToken) + "\","
            "\"expiresIn\":" + std::to_string(jwtAccessTtlSec()) + ","
            "\"role\":\"" + escapeJSONString(role) + "\","
            "\"tenantId\":\"" + escapeJSONString(tenantId) + "\""
            "}";
        return res;
    }

    if (req.method == "POST" && routePath == "/api/auth/refresh") {
        std::string refreshToken = extractJSONValue(req.body, "refreshToken");
        if (refreshToken.empty()) {
            refreshToken = extractBearerToken(getHeaderValue(req.headers, "Authorization"));
        }
        AuthContext refreshCtx;
        if (!tryParseAndValidateJwt(refreshToken, refreshCtx) || refreshCtx.tokenType != "refresh") {
            res.status = 401;
            res.statusText = "Unauthorized";
            res.body = "{\"error\":\"Invalid refresh token\"}";
            return res;
        }

        {
            std::lock_guard<std::mutex> lock(g_auth_mutex);
            g_revokedRefreshTokenJti.insert(refreshCtx.jti);
        }
        persistRevokedTokenJtiToSqlite("refresh", refreshCtx.jti, refreshCtx.exp);

        const std::string newAccess = buildJwtToken("access", refreshCtx.subject, refreshCtx.role, refreshCtx.tenantId, jwtAccessTtlSec());
        const std::string newRefresh = buildJwtToken("refresh", refreshCtx.subject, refreshCtx.role, refreshCtx.tenantId, jwtRefreshTtlSec());
        res.body =
            "{"
            "\"tokenType\":\"Bearer\","
            "\"accessToken\":\"" + escapeJSONString(newAccess) + "\","
            "\"refreshToken\":\"" + escapeJSONString(newRefresh) + "\","
            "\"expiresIn\":" + std::to_string(jwtAccessTtlSec()) + ","
            "\"role\":\"" + escapeJSONString(refreshCtx.role) + "\","
            "\"tenantId\":\"" + escapeJSONString(refreshCtx.tenantId) + "\""
            "}";
        return res;
    }

    if (req.method == "POST" && routePath == "/api/auth/logout") {
        std::string accessToken = extractApiTokenFromRequest(req);
        if (accessToken.empty()) accessToken = extractJSONValue(req.body, "accessToken");
        std::string refreshToken = extractJSONValue(req.body, "refreshToken");

        AuthContext accessCtx;
        if (!accessToken.empty() && tryParseAndValidateJwt(accessToken, accessCtx) && accessCtx.tokenType == "access") {
            {
                std::lock_guard<std::mutex> lock(g_auth_mutex);
                g_revokedAccessTokenJti.insert(accessCtx.jti);
            }
            persistRevokedTokenJtiToSqlite("access", accessCtx.jti, accessCtx.exp);
        }

        AuthContext refreshCtx;
        if (!refreshToken.empty() && tryParseAndValidateJwt(refreshToken, refreshCtx) && refreshCtx.tokenType == "refresh") {
            {
                std::lock_guard<std::mutex> lock(g_auth_mutex);
                g_revokedRefreshTokenJti.insert(refreshCtx.jti);
            }
            persistRevokedTokenJtiToSqlite("refresh", refreshCtx.jti, refreshCtx.exp);
        }

        res.body = "{\"status\":\"ok\"}";
        return res;
    }

    res.status = 404;
    res.statusText = "Not Found";
    res.body = "{\"error\":\"Auth endpoint not found\"}";
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
    if (jwtEnabled()) {
        std::string provided = getQueryParam(req.path, "token");
        if (provided.empty()) {
            provided = extractBearerToken(getHeaderValue(req.headers, "Authorization"));
        }
        if (provided.empty()) {
            provided = getHeaderValue(req.headers, "X-WS-Token");
        }
        AuthContext ctx;
        return tryParseAndValidateJwt(provided, ctx) && ctx.tokenType == "access";
    }

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
    return routePath == "/api" || routePath == "/api/health" ||
           routePath == "/api/auth/login" || routePath == "/api/auth/refresh" || routePath == "/api/auth/logout" ||
           routePath == "/api/auth/passkey/challenge" || routePath == "/api/auth/passkey/register" || routePath == "/api/auth/passkey/login";
}

bool isApiTokenProtectionEnabled() {
    if (jwtEnabled()) {
        return true;
    }
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

AuthContext extractAuthContextFromRequest(const HTTPRequest& req) {
    AuthContext ctx;
    const std::string token = extractApiTokenFromRequest(req);
    if (!token.empty()) {
        tryParseAndValidateJwt(token, ctx);
    }
    return ctx;
}

bool isHttpApiAuthorized(const std::string& routePath, const std::string& token) {
    if (routePath.find("/api/") != 0) {
        return true;
    }
    if (isPublicApiPath(routePath)) {
        return true;
    }

    if (jwtEnabled()) {
        AuthContext ctx;
        return tryParseAndValidateJwt(token, ctx) && ctx.tokenType == "access";
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
    if (jwtEnabled()) {
        AuthContext ctx;
        if (tryParseAndValidateJwt(token, ctx) && ctx.tokenType == "access") {
            return ctx.role;
        }
    }

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

std::string resolveTenantFromRequest(const HTTPRequest& req) {
    if (jwtEnabled()) {
        AuthContext ctx = extractAuthContextFromRequest(req);
        if (ctx.valid && ctx.tokenType == "access") {
            return normalizeTenantId(ctx.tenantId);
        }
    }

    std::string tenant = getQueryParam(req.path, "tenantId");
    if (tenant.empty()) tenant = getHeaderValue(req.headers, "X-Tenant-Id");
    return normalizeTenantId(tenant);
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
    if (jwtEnabled()) {
        std::string jwtToken = getQueryParam(req.path, "token");
        if (jwtToken.empty()) jwtToken = extractBearerToken(getHeaderValue(req.headers, "Authorization"));
        if (jwtToken.empty()) jwtToken = getHeaderValue(req.headers, "X-WS-Token");
        AuthContext ctx;
        if (tryParseAndValidateJwt(jwtToken, ctx) && ctx.tokenType == "access") {
            client.userId = ctx.subject;
            client.tenantId = normalizeTenantId(ctx.tenantId);
        }
    } else {
        client.userId = getQueryParam(req.path, "user");
        client.tenantId = normalizeTenantId(getQueryParam(req.path, "tenant"));
        if (client.tenantId == "default") {
            client.tenantId = normalizeTenantId(getHeaderValue(req.headers, "X-Tenant-Id"));
        }
    }
    if (client.userId.empty()) {
        client.userId = getQueryParam(req.path, "user");
    }
    if (client.tenantId.empty()) client.tenantId = "default";

    {
        std::lock_guard<std::mutex> lock(ws_mutex);
        wsClients.push_back(client);
    }

    broadcastEvent(
        "presence.joined",
        "{\"user\":\"" + escapeJSONString(client.userId) + "\",\"roomId\":\"" + escapeJSONString(client.roomId) + "\",\"tenantId\":\"" + escapeJSONString(client.tenantId) + "\"}",
        client.roomId,
        client.tenantId
    );

    sendWebSocketFrame(
        clientSocket,
        0x1,
        "{\"type\":\"connected\",\"roomId\":\"" + escapeJSONString(client.roomId) + "\",\"tenantId\":\"" + escapeJSONString(client.tenantId) + "\",\"timestamp\":\"" + escapeJSONString(getCurrentTimestamp()) + "\"}"
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
        "{\"user\":\"" + escapeJSONString(client.userId) + "\",\"roomId\":\"" + escapeJSONString(client.roomId) + "\",\"tenantId\":\"" + escapeJSONString(client.tenantId) + "\"}",
        client.roomId,
        client.tenantId
    );
    closeClientSocket(clientSocket);
}

HTTPResponse handleRequest(const HTTPRequest& req) {
    HTTPResponse res;
    const std::string routePath = getPathOnly(req.path);
    res.headers["Content-Type"] = "application/json";
    res.headers["Access-Control-Allow-Origin"] = "*";
    res.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-API-Token, X-WS-Token, X-Tenant-Id";
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
      "JWT env": "GIGACHAD_JWT_SECRET or MEDIA_JWT_SECRET",
      "POST /api/auth/login": "Issue access+refresh JWT",
      "POST /api/auth/refresh": "Rotate refresh and issue new tokens",
      "POST /api/auth/logout": "Revoke provided token jti",
      "POST /api/auth/passkey/challenge": "Create short-lived challenge for register/login",
      "POST /api/auth/passkey/register": "Passkey register with strict clientData/authenticatorData checks",
      "POST /api/auth/passkey/login": "Passkey login with strict metadata + credential-bound signature verification and JWT issue",
      "Role tokens (optional)": [
        "GIGACHAD_MOD_TOKEN / MEDIA_MOD_TOKEN",
        "GIGACHAD_ADMIN_TOKEN / MEDIA_ADMIN_TOKEN"
      ],
      "Passkey challenge TTL env": "GIGACHAD_PASSKEY_CHALLENGE_TTL_SEC or MEDIA_PASSKEY_CHALLENGE_TTL_SEC",
      "Passkey RP/Origin env": "GIGACHAD_PASSKEY_RP_ID + GIGACHAD_PASSKEY_ALLOWED_ORIGINS (MEDIA_* aliases)",
      "Passkey strict metadata env": "GIGACHAD_PASSKEY_STRICT_METADATA or MEDIA_PASSKEY_STRICT_METADATA",
      "Passkey counter policy env": "GIGACHAD_PASSKEY_COUNTER_STRICT or MEDIA_PASSKEY_COUNTER_STRICT",
      "Passkey signature mode env": "GIGACHAD_PASSKEY_SIGNATURE_MODE (hmac|es256) or MEDIA_PASSKEY_SIGNATURE_MODE",
      "Passkey rate-limit env": "GIGACHAD_PASSKEY_RATE_MAX_ATTEMPTS + GIGACHAD_PASSKEY_RATE_WINDOW_SEC (MEDIA_* aliases)",
      "Passkey lockout env": "GIGACHAD_PASSKEY_LOCKOUT_THRESHOLD + GIGACHAD_PASSKEY_LOCKOUT_SEC (MEDIA_* aliases)",
      "Auth audit log": "data/auth_audit.jsonl"
    },
    "Realtime": {
      "GET /ws?room={roomId}&user={userId}&tenant={tenantId}&token={token}": "WebSocket event stream"
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
    if ((isApiTokenProtectionEnabled() || jwtEnabled()) &&
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

    if (routePath.find("/api/auth/") == 0) {
        return handleAuthAPI(req);
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
        if (getHeaderValue(req.headers, "X-Real-IP").empty()) {
            req.headers["X-Real-IP"] = clientIp;
        }

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
