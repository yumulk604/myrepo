# API Examples - C++ Chat API

Complete examples for using the Chat API endpoints.

## 📤 Sending Messages

### PowerShell
```powershell
# Send a single message
$message = @{
    username = "alice"
    content = "Hello, World!"
} | ConvertTo-Json

Invoke-RestMethod -Uri "http://localhost:8080/api/messages" `
    -Method Post `
    -ContentType "application/json" `
    -Body $message
```

### cURL
```bash
curl -X POST http://localhost:8080/api/messages \
  -H "Content-Type: application/json" \
  -d '{"username":"alice","content":"Hello, World!"}'
```

### JavaScript (Fetch API)
```javascript
fetch('http://localhost:8080/api/messages', {
  method: 'POST',
  headers: {
    'Content-Type': 'application/json',
  },
  body: JSON.stringify({
    username: 'alice',
    content: 'Hello, World!'
  })
})
.then(response => response.json())
.then(data => console.log(data));
```

### Python
```python
import requests

message = {
    "username": "alice",
    "content": "Hello, World!"
}

response = requests.post(
    'http://localhost:8080/api/messages',
    json=message
)
print(response.json())
```

## 📥 Getting Messages

### Get All Messages

#### PowerShell
```powershell
Invoke-RestMethod -Uri "http://localhost:8080/api/messages"
```

#### cURL
```bash
curl http://localhost:8080/api/messages
```

#### JavaScript
```javascript
fetch('http://localhost:8080/api/messages')
  .then(response => response.json())
  .then(messages => console.log(messages));
```

#### Python
```python
import requests

response = requests.get('http://localhost:8080/api/messages')
messages = response.json()
print(messages)
```

### Get Specific Message

#### PowerShell
```powershell
Invoke-RestMethod -Uri "http://localhost:8080/api/messages/1"
```

#### cURL
```bash
curl http://localhost:8080/api/messages/1
```

## 🗑️ Deleting Messages

### PowerShell
```powershell
Invoke-RestMethod -Uri "http://localhost:8080/api/messages/1" -Method Delete
```

### cURL
```bash
curl -X DELETE http://localhost:8080/api/messages/1
```

### JavaScript
```javascript
fetch('http://localhost:8080/api/messages/1', {
  method: 'DELETE'
})
.then(response => response.json())
.then(data => console.log(data));
```

### Python
```python
import requests

response = requests.delete('http://localhost:8080/api/messages/1')
print(response.json())
```

## 🏥 Health Check

### PowerShell
```powershell
Invoke-RestMethod -Uri "http://localhost:8080/api/health"
```

### cURL
```bash
curl http://localhost:8080/api/health
```

## 🔄 Complete Chat Flow Example

### PowerShell Script
```powershell
# 1. Check if server is healthy
$health = Invoke-RestMethod -Uri "http://localhost:8080/api/health"
Write-Host "Server Status: $($health.status)" -ForegroundColor Green

# 2. Send a message from Alice
$msg1 = @{username="alice"; content="Hi everyone!"} | ConvertTo-Json
$response1 = Invoke-RestMethod -Uri "http://localhost:8080/api/messages" `
    -Method Post -ContentType "application/json" -Body $msg1
Write-Host "Alice sent message #$($response1.id)" -ForegroundColor Cyan

# 3. Send a message from Bob
$msg2 = @{username="bob"; content="Hello Alice!"} | ConvertTo-Json
$response2 = Invoke-RestMethod -Uri "http://localhost:8080/api/messages" `
    -Method Post -ContentType "application/json" -Body $msg2
Write-Host "Bob sent message #$($response2.id)" -ForegroundColor Cyan

# 4. Get all messages
$allMessages = Invoke-RestMethod -Uri "http://localhost:8080/api/messages"
Write-Host "`nAll Messages:" -ForegroundColor Yellow
$allMessages | ForEach-Object {
    Write-Host "[$($_.timestamp)] $($_.username): $($_.content)"
}

# 5. Get specific message
$message = Invoke-RestMethod -Uri "http://localhost:8080/api/messages/1"
Write-Host "`nMessage #1 Details:" -ForegroundColor Yellow
$message | Format-List
```

### Python Script
```python
import requests
import json

BASE_URL = "http://localhost:8080"

# 1. Check health
response = requests.get(f"{BASE_URL}/api/health")
print(f"Server Status: {response.json()['status']}")

# 2. Send messages
alice_msg = {"username": "alice", "content": "Hi everyone!"}
bob_msg = {"username": "bob", "content": "Hello Alice!"}

resp1 = requests.post(f"{BASE_URL}/api/messages", json=alice_msg)
print(f"Alice sent message #{resp1.json()['id']}")

resp2 = requests.post(f"{BASE_URL}/api/messages", json=bob_msg)
print(f"Bob sent message #{resp2.json()['id']}")

# 3. Get all messages
messages = requests.get(f"{BASE_URL}/api/messages").json()
print("\nAll Messages:")
for msg in messages:
    print(f"[{msg['timestamp']}] {msg['username']}: {msg['content']}")

# 4. Get specific message
message = requests.get(f"{BASE_URL}/api/messages/1").json()
print(f"\nMessage #1: {json.dumps(message, indent=2)}")
```

## 🌐 HTML/JavaScript Chat Client

```html
<!DOCTYPE html>
<html>
<head>
    <title>Chat API Client</title>
    <style>
        body { font-family: Arial, sans-serif; max-width: 800px; margin: 50px auto; padding: 20px; }
        #messages { border: 1px solid #ddd; padding: 20px; height: 400px; overflow-y: auto; margin-bottom: 20px; }
        .message { margin: 10px 0; padding: 10px; background: #f0f0f0; border-radius: 5px; }
        input, button { padding: 10px; margin: 5px; }
        input { width: 200px; }
        button { cursor: pointer; background: #007bff; color: white; border: none; border-radius: 3px; }
    </style>
</head>
<body>
    <h1>Chat API Client</h1>
    
    <div id="messages"></div>
    
    <div>
        <input type="text" id="username" placeholder="Username" value="alice">
        <input type="text" id="content" placeholder="Message" style="width: 400px;">
        <button onclick="sendMessage()">Send</button>
        <button onclick="loadMessages()">Refresh</button>
    </div>

    <script>
        const API_URL = 'http://localhost:8080';

        async function sendMessage() {
            const username = document.getElementById('username').value;
            const content = document.getElementById('content').value;
            
            if (!username || !content) {
                alert('Please enter both username and message');
                return;
            }

            const response = await fetch(`${API_URL}/api/messages`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ username, content })
            });

            if (response.ok) {
                document.getElementById('content').value = '';
                loadMessages();
            }
        }

        async function loadMessages() {
            const response = await fetch(`${API_URL}/api/messages`);
            const messages = await response.json();
            
            const messagesDiv = document.getElementById('messages');
            messagesDiv.innerHTML = messages.map(msg => `
                <div class="message">
                    <strong>${msg.username}</strong> <small>(${msg.timestamp})</small><br>
                    ${msg.content}
                </div>
            `).join('');
        }

        // Load messages on page load
        loadMessages();
        
        // Auto-refresh every 2 seconds
        setInterval(loadMessages, 2000);
    </script>
</body>
</html>
```

Save this as `chat.html` and open it in your browser while the server is running!

## 📊 Response Examples

### Successful Message Post
```json
{
  "id": "1",
  "username": "alice",
  "content": "Hello, World!",
  "timestamp": "2025-10-22 04:19:01",
  "status": "Message sent successfully"
}
```

### All Messages
```json
[
  {
    "id": "1",
    "username": "alice",
    "content": "Hello, World!",
    "timestamp": "2025-10-22 04:19:01"
  },
  {
    "id": "2",
    "username": "bob",
    "content": "Hi Alice!",
    "timestamp": "2025-10-22 04:19:05"
  }
]
```

### Error Response (400 Bad Request)
```json
{
  "error": "Missing required fields: username and content"
}
```

### Error Response (404 Not Found)
```json
{
  "error": "Message not found"
}
```

---

For more information, see the main [README.md](README.md) or [QUICKSTART.md](QUICKSTART.md).



