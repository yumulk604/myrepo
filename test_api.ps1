# PowerShell script to test the Chat API

$baseUrl = "http://localhost:8080"

Write-Host "=== Testing Chat API ===" -ForegroundColor Green

# Test 1: Health Check
Write-Host "`n1. Health Check..." -ForegroundColor Yellow
try {
    $health = Invoke-RestMethod -Uri "$baseUrl/api/health" -Method Get
    Write-Host "✓ Health check passed" -ForegroundColor Green
    $health | ConvertTo-Json
} catch {
    Write-Host "✗ Health check failed: $_" -ForegroundColor Red
}

# Test 2: Send first message
Write-Host "`n2. Sending first message..." -ForegroundColor Yellow
try {
    $message1 = @{
        username = "alice"
        content = "Hello, this is my first message!"
    } | ConvertTo-Json

    $response1 = Invoke-RestMethod -Uri "$baseUrl/api/messages" `
        -Method Post `
        -ContentType "application/json" `
        -Body $message1
    
    Write-Host "✓ Message sent successfully" -ForegroundColor Green
    $response1 | ConvertTo-Json
} catch {
    Write-Host "✗ Failed to send message: $_" -ForegroundColor Red
}

# Test 3: Send second message
Write-Host "`n3. Sending second message..." -ForegroundColor Yellow
try {
    $message2 = @{
        username = "bob"
        content = "Hi Alice! Nice to meet you."
    } | ConvertTo-Json

    $response2 = Invoke-RestMethod -Uri "$baseUrl/api/messages" `
        -Method Post `
        -ContentType "application/json" `
        -Body $message2
    
    Write-Host "✓ Message sent successfully" -ForegroundColor Green
    $response2 | ConvertTo-Json
} catch {
    Write-Host "✗ Failed to send message: $_" -ForegroundColor Red
}

# Test 4: Get all messages
Write-Host "`n4. Retrieving all messages..." -ForegroundColor Yellow
try {
    $allMessages = Invoke-RestMethod -Uri "$baseUrl/api/messages" -Method Get
    Write-Host "✓ Retrieved $($allMessages.Count) messages" -ForegroundColor Green
    $allMessages | ConvertTo-Json
} catch {
    Write-Host "✗ Failed to retrieve messages: $_" -ForegroundColor Red
}

# Test 5: Get specific message
Write-Host "`n5. Getting message by ID (1)..." -ForegroundColor Yellow
try {
    $message = Invoke-RestMethod -Uri "$baseUrl/api/messages/1" -Method Get
    Write-Host "✓ Message retrieved" -ForegroundColor Green
    $message | ConvertTo-Json
} catch {
    Write-Host "✗ Failed to retrieve message: $_" -ForegroundColor Red
}

# Test 6: Test invalid request (missing fields)
Write-Host "`n6. Testing invalid request (should fail)..." -ForegroundColor Yellow
try {
    $invalidMessage = @{
        username = "charlie"
    } | ConvertTo-Json

    $response = Invoke-RestMethod -Uri "$baseUrl/api/messages" `
        -Method Post `
        -ContentType "application/json" `
        -Body $invalidMessage
    
    Write-Host "✗ Should have failed but didn't" -ForegroundColor Red
} catch {
    Write-Host "✓ Correctly rejected invalid request" -ForegroundColor Green
    Write-Host $_.Exception.Message
}

Write-Host "`n=== Testing Complete ===" -ForegroundColor Green



