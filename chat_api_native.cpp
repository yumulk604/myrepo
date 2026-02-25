// Native C++ Chat API with minimal dependencies
// Compile with: g++ -std=c++17 chat_api_native.cpp -o chat_api.exe -lws2_32 -O2

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

// Message structure
struct Message {
    std::string id;
    std::string username;
    std::string content;
    std::string timestamp;
    
    std::string toJSON() const {
        std::stringstream ss;
        ss << "{"
           << "\"id\":\"" << escapeJSON(id) << "\","
           << "\"username\":\"" << escapeJSON(username) << "\","
           << "\"content\":\"" << escapeJSON(content) << "\","
           << "\"timestamp\":\"" << escapeJSON(timestamp) << "\""
           << "}";
        return ss.str();
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

// Global state
std::vector<Message> messages;
std::mutex messages_mutex;
int message_counter = 0;

// Helper functions
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

std::string urlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::sscanf(str.substr(i + 1, 2).c_str(), "%x", &value);
            result += static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string extractJSONValue(const std::string& json, const std::string& key) {
    // Look for "key": with optional whitespace
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";
    
    pos += searchKey.length();
    
    // Skip whitespace and colon
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == ':')) {
        pos++;
    }
    
    // Expect a quote
    if (pos >= json.length() || json[pos] != '"') return "";
    pos++; // Skip opening quote
    
    // Find closing quote
    size_t endPos = json.find("\"", pos);
    if (endPos == std::string::npos) return "";
    
    return json.substr(pos, endPos - pos);
}

bool hasJSONKey(const std::string& json, const std::string& key) {
    return json.find("\"" + key + "\":") != std::string::npos;
}

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
    
    // Parse request line
    if (std::getline(stream, line)) {
        line.erase(line.find_last_not_of("\r\n") + 1);
        std::istringstream lineStream(line);
        lineStream >> req.method >> req.path >> req.version;
    }
    
    // Parse headers
    while (std::getline(stream, line) && line != "\r") {
        line.erase(line.find_last_not_of("\r\n") + 1);
        if (line.empty()) break;
        
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            // Trim whitespace
            value.erase(0, value.find_first_not_of(" \t"));
            req.headers[key] = value;
        }
    }
    
    // Parse body
    std::string bodyContent;
    while (std::getline(stream, line)) {
        bodyContent += line;
    }
    req.body = bodyContent;
    
    return req;
}

HTTPResponse handleRequest(const HTTPRequest& req) {
    HTTPResponse res;
    res.headers["Content-Type"] = "application/json";
    res.headers["Access-Control-Allow-Origin"] = "*";
    
    // Route: GET /
    if (req.method == "GET" && req.path == "/") {
        res.body = R"({
  "message": "Welcome to Chat API",
  "version": "1.0.0",
  "endpoints": {
    "GET /api/messages": "Get all messages",
    "POST /api/messages": "Send a new message",
    "GET /api/messages/:id": "Get message by ID",
    "DELETE /api/messages/:id": "Delete message by ID",
    "GET /api/health": "Health check"
  }
})";
        return res;
    }
    
    // Route: GET /api/health
    if (req.method == "GET" && req.path == "/api/health") {
        std::stringstream ss;
        ss << "{"
           << "\"status\":\"healthy\","
           << "\"service\":\"Chat API\","
           << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
           << "}";
        res.body = ss.str();
        return res;
    }
    
    // Route: GET /api/messages
    if (req.method == "GET" && req.path == "/api/messages") {
        std::lock_guard<std::mutex> lock(messages_mutex);
        
        std::stringstream ss;
        ss << "[";
        for (size_t i = 0; i < messages.size(); ++i) {
            if (i > 0) ss << ",";
            ss << messages[i].toJSON();
        }
        ss << "]";
        
        res.body = ss.str();
        return res;
    }
    
    // Route: POST /api/messages
    if (req.method == "POST" && req.path == "/api/messages") {
        if (!hasJSONKey(req.body, "username") || !hasJSONKey(req.body, "content")) {
            res.status = 400;
            res.statusText = "Bad Request";
            res.body = "{\"error\":\"Missing required fields: username and content\"}";
            return res;
        }
        
        std::lock_guard<std::mutex> lock(messages_mutex);
        
        Message msg;
        msg.id = std::to_string(++message_counter);
        msg.username = extractJSONValue(req.body, "username");
        msg.content = extractJSONValue(req.body, "content");
        msg.timestamp = getCurrentTimestamp();
        
        messages.push_back(msg);
        
        std::stringstream ss;
        ss << "{"
           << "\"id\":\"" << msg.id << "\","
           << "\"username\":\"" << msg.username << "\","
           << "\"content\":\"" << msg.content << "\","
           << "\"timestamp\":\"" << msg.timestamp << "\","
           << "\"status\":\"Message sent successfully\""
           << "}";
        
        res.status = 201;
        res.statusText = "Created";
        res.body = ss.str();
        return res;
    }
    
    // Route: GET /api/messages/:id
    if (req.method == "GET" && req.path.find("/api/messages/") == 0) {
        std::string id = req.path.substr(14); // After "/api/messages/"
        
        std::lock_guard<std::mutex> lock(messages_mutex);
        
        for (const auto& msg : messages) {
            if (msg.id == id) {
                res.body = msg.toJSON();
                return res;
            }
        }
        
        res.status = 404;
        res.statusText = "Not Found";
        res.body = "{\"error\":\"Message not found\"}";
        return res;
    }
    
    // Route: DELETE /api/messages/:id
    if (req.method == "DELETE" && req.path.find("/api/messages/") == 0) {
        std::string id = req.path.substr(14);
        
        std::lock_guard<std::mutex> lock(messages_mutex);
        
        for (auto it = messages.begin(); it != messages.end(); ++it) {
            if (it->id == id) {
                messages.erase(it);
                res.body = "{\"status\":\"Message deleted successfully\"}";
                return res;
            }
        }
        
        res.status = 404;
        res.statusText = "Not Found";
        res.body = "{\"error\":\"Message not found\"}";
        return res;
    }
    
    // 404 Not Found
    res.status = 404;
    res.statusText = "Not Found";
    res.body = "{\"error\":\"Endpoint not found\"}";
    return res;
}

void handleClient(int clientSocket) {
    char buffer[4096] = {0};
    
    #ifdef _WIN32
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    #else
    int bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
    #endif
    
    if (bytesRead > 0) {
        std::string requestStr(buffer, bytesRead);
        HTTPRequest req = parseRequest(requestStr);
        HTTPResponse res = handleRequest(req);
        
        std::string response = res.toString();
        
        #ifdef _WIN32
        send(clientSocket, response.c_str(), response.length(), 0);
        closesocket(clientSocket);
        #else
        write(clientSocket, response.c_str(), response.length());
        close(clientSocket);
        #endif
        
        std::cout << req.method << " " << req.path << " - " << res.status << std::endl;
    } else {
        #ifdef _WIN32
        closesocket(clientSocket);
        #else
        close(clientSocket);
        #endif
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
    
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8080);
    
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
    
    if (listen(serverSocket, 10) < 0) {
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
    std::cout << "  Chat API Server" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Server running on http://localhost:8080" << std::endl;
    std::cout << "\nAvailable endpoints:" << std::endl;
    std::cout << "  GET    /" << std::endl;
    std::cout << "  GET    /api/health" << std::endl;
    std::cout << "  GET    /api/messages" << std::endl;
    std::cout << "  POST   /api/messages" << std::endl;
    std::cout << "  GET    /api/messages/:id" << std::endl;
    std::cout << "  DELETE /api/messages/:id" << std::endl;
    std::cout << "\nPress Ctrl+C to stop the server" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\nListening for requests...\n" << std::endl;
    
    while (true) {
        sockaddr_in clientAddr;
        int addrLen = sizeof(clientAddr);
        
        #ifdef _WIN32
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &addrLen);
        #else
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, (socklen_t*)&addrLen);
        #endif
        
        if (clientSocket < 0) {
            std::cerr << "Accept failed" << std::endl;
            continue;
        }
        
        // Handle client in a new thread
        std::thread clientThread(handleClient, clientSocket);
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

