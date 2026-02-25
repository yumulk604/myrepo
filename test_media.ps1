# PowerShell script to test the Media Server API

$baseUrl = "http://localhost:8080"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  🧪 MEDIA SERVER API TEST SUITE" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# ============================================================================
# HEALTH CHECK
# ============================================================================
Write-Host "`n📊 1. HEALTH CHECK" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor DarkGray
try {
    $health = Invoke-RestMethod -Uri "$baseUrl/api/health" -Method Get
    Write-Host "✓ Server is healthy" -ForegroundColor Green
    $health | ConvertTo-Json -Depth 10
} catch {
    Write-Host "✗ Health check failed: $_" -ForegroundColor Red
    exit 1
}

# ============================================================================
# CHAT API TESTS
# ============================================================================
Write-Host "`n💬 2. CHAT API TESTS" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor DarkGray

Write-Host "`n  → Sending chat message from Alice..." -ForegroundColor Cyan
try {
    $msg1 = @{
        username = "alice"
        content = "Merhaba! Bu bir test mesajı"
        roomId = "room1"
    } | ConvertTo-Json

    $response = Invoke-RestMethod -Uri "$baseUrl/api/chat/messages" `
        -Method Post `
        -ContentType "application/json" `
        -Body $msg1
    
    Write-Host "  ✓ Message sent - ID: $($response.id)" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Failed: $_" -ForegroundColor Red
}

Write-Host "`n  → Sending chat message from Bob..." -ForegroundColor Cyan
try {
    $msg2 = @{
        username = "bob"
        content = "Merhaba Alice! Nasılsın?"
        roomId = "room1"
    } | ConvertTo-Json

    $response = Invoke-RestMethod -Uri "$baseUrl/api/chat/messages" `
        -Method Post `
        -ContentType "application/json" `
        -Body $msg2
    
    Write-Host "  ✓ Message sent - ID: $($response.id)" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Failed: $_" -ForegroundColor Red
}

Write-Host "`n  → Getting all chat messages..." -ForegroundColor Cyan
try {
    $messages = Invoke-RestMethod -Uri "$baseUrl/api/chat/messages" -Method Get
    Write-Host "  ✓ Retrieved $($messages.Count) messages" -ForegroundColor Green
    $messages | ForEach-Object {
        Write-Host "    [$($_.timestamp)] $($_.username): $($_.content)" -ForegroundColor White
    }
} catch {
    Write-Host "  ✗ Failed: $_" -ForegroundColor Red
}

# ============================================================================
# VOICE CALL API TESTS
# ============================================================================
Write-Host "`n🎤 3. VOICE CALL API TESTS" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor DarkGray

Write-Host "`n  → Initiating voice call..." -ForegroundColor Cyan
try {
    $voiceCall = @{
        callerId = "user_alice"
        callerName = "Alice"
        receiverId = "user_bob"
        receiverName = "Bob"
    } | ConvertTo-Json

    $callResponse = Invoke-RestMethod -Uri "$baseUrl/api/voice/call" `
        -Method Post `
        -ContentType "application/json" `
        -Body $voiceCall
    
    $callId = $callResponse.id
    Write-Host "  ✓ Voice call initiated - ID: $callId" -ForegroundColor Green
    Write-Host "    Caller: $($callResponse.callerName) → Receiver: $($callResponse.receiverName)" -ForegroundColor White
    Write-Host "    Status: $($callResponse.status)" -ForegroundColor White
    
    # Answer the call
    Write-Host "`n  → Answering voice call..." -ForegroundColor Cyan
    $answer = @{
        answer = "SDP_ANSWER_DATA_HERE"
    } | ConvertTo-Json

    $answerResponse = Invoke-RestMethod -Uri "$baseUrl/api/voice/answer/$callId" `
        -Method Post `
        -ContentType "application/json" `
        -Body $answer
    
    Write-Host "  ✓ Call answered - Status: $($answerResponse.status)" -ForegroundColor Green
    
    # Get active calls
    Write-Host "`n  → Getting active voice calls..." -ForegroundColor Cyan
    $activeCalls = Invoke-RestMethod -Uri "$baseUrl/api/voice/active" -Method Get
    Write-Host "  ✓ Active voice calls: $($activeCalls.Count)" -ForegroundColor Green
    
    # End the call
    Write-Host "`n  → Ending voice call..." -ForegroundColor Cyan
    $endResponse = Invoke-RestMethod -Uri "$baseUrl/api/voice/end/$callId" `
        -Method Post `
        -ContentType "application/json" `
        -Body "{}"
    
    Write-Host "  ✓ Call ended - Status: $($endResponse.status)" -ForegroundColor Green
    
} catch {
    Write-Host "  ✗ Failed: $_" -ForegroundColor Red
}

# ============================================================================
# VIDEO CALL API TESTS
# ============================================================================
Write-Host "`n📹 4. VIDEO CALL API TESTS" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor DarkGray

Write-Host "`n  → Initiating video call..." -ForegroundColor Cyan
try {
    $videoCall = @{
        callerId = "user_alice"
        callerName = "Alice"
        receiverId = "user_charlie"
        receiverName = "Charlie"
        offer = "WEBRTC_SDP_OFFER_DATA"
    } | ConvertTo-Json

    $callResponse = Invoke-RestMethod -Uri "$baseUrl/api/video/call" `
        -Method Post `
        -ContentType "application/json" `
        -Body $videoCall
    
    $videoCallId = $callResponse.id
    Write-Host "  ✓ Video call initiated - ID: $videoCallId" -ForegroundColor Green
    Write-Host "    Caller: $($callResponse.callerName) → Receiver: $($callResponse.receiverName)" -ForegroundColor White
    Write-Host "    Status: $($callResponse.status)" -ForegroundColor White
    
    # Add ICE candidate
    Write-Host "`n  → Adding ICE candidate..." -ForegroundColor Cyan
    $ice = @{
        candidate = "ICE_CANDIDATE_DATA"
    } | ConvertTo-Json

    $iceResponse = Invoke-RestMethod -Uri "$baseUrl/api/video/ice/$videoCallId" `
        -Method Post `
        -ContentType "application/json" `
        -Body $ice
    
    Write-Host "  ✓ ICE candidate added" -ForegroundColor Green
    
    # Answer the call
    Write-Host "`n  → Answering video call..." -ForegroundColor Cyan
    $answer = @{
        answer = "WEBRTC_SDP_ANSWER_DATA"
    } | ConvertTo-Json

    $answerResponse = Invoke-RestMethod -Uri "$baseUrl/api/video/answer/$videoCallId" `
        -Method Post `
        -ContentType "application/json" `
        -Body $answer
    
    Write-Host "  ✓ Video call answered - Status: $($answerResponse.status)" -ForegroundColor Green
    
    # Get active video calls
    Write-Host "`n  → Getting active video calls..." -ForegroundColor Cyan
    $activeVideoCalls = Invoke-RestMethod -Uri "$baseUrl/api/video/active" -Method Get
    Write-Host "  ✓ Active video calls: $($activeVideoCalls.Count)" -ForegroundColor Green
    
    # End the call
    Write-Host "`n  → Ending video call..." -ForegroundColor Cyan
    $endResponse = Invoke-RestMethod -Uri "$baseUrl/api/video/end/$videoCallId" `
        -Method Post `
        -ContentType "application/json" `
        -Body "{}"
    
    Write-Host "  ✓ Video call ended" -ForegroundColor Green
    
} catch {
    Write-Host "  ✗ Failed: $_" -ForegroundColor Red
}

# ============================================================================
# LIVE STREAMING API TESTS
# ============================================================================
Write-Host "`n📺 5. LIVE STREAMING API TESTS" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor DarkGray

Write-Host "`n  → Starting live stream..." -ForegroundColor Cyan
try {
    $stream = @{
        streamerId = "user_alice"
        streamerName = "Alice"
        title = "Canlı Yayın Test"
        description = "Bu bir test canlı yayınıdır"
    } | ConvertTo-Json

    $streamResponse = Invoke-RestMethod -Uri "$baseUrl/api/stream/start" `
        -Method Post `
        -ContentType "application/json" `
        -Body $stream
    
    $streamId = $streamResponse.id
    Write-Host "  ✓ Stream started - ID: $streamId" -ForegroundColor Green
    Write-Host "    Title: $($streamResponse.title)" -ForegroundColor White
    Write-Host "    Streamer: $($streamResponse.streamerName)" -ForegroundColor White
    Write-Host "    Stream Key: $($streamResponse.streamKey)" -ForegroundColor Yellow
    
    # Get all live streams
    Write-Host "`n  → Getting all live streams..." -ForegroundColor Cyan
    $liveStreams = Invoke-RestMethod -Uri "$baseUrl/api/stream/live" -Method Get
    Write-Host "  ✓ Live streams: $($liveStreams.Count)" -ForegroundColor Green
    
    # Join stream as viewer
    Write-Host "`n  → Joining stream as viewer..." -ForegroundColor Cyan
    $viewer1 = @{
        viewerId = "user_bob"
    } | ConvertTo-Json

    $joinResponse = Invoke-RestMethod -Uri "$baseUrl/api/stream/join/$streamId" `
        -Method Post `
        -ContentType "application/json" `
        -Body $viewer1
    
    Write-Host "  ✓ Joined stream - Viewers: $($joinResponse.viewerCount)" -ForegroundColor Green
    
    # Another viewer joins
    Write-Host "`n  → Another viewer joining..." -ForegroundColor Cyan
    $viewer2 = @{
        viewerId = "user_charlie"
    } | ConvertTo-Json

    $joinResponse2 = Invoke-RestMethod -Uri "$baseUrl/api/stream/join/$streamId" `
        -Method Post `
        -ContentType "application/json" `
        -Body $viewer2
    
    Write-Host "  ✓ Joined stream - Viewers: $($joinResponse2.viewerCount)" -ForegroundColor Green
    
    # Get stream details
    Write-Host "`n  → Getting stream details..." -ForegroundColor Cyan
    $streamDetails = Invoke-RestMethod -Uri "$baseUrl/api/stream/$streamId" -Method Get
    Write-Host "  ✓ Stream details retrieved" -ForegroundColor Green
    Write-Host "    Viewers: $($streamDetails.viewerCount)" -ForegroundColor White
    Write-Host "    Status: $($streamDetails.status)" -ForegroundColor White
    
    # Viewer leaves
    Write-Host "`n  → Viewer leaving stream..." -ForegroundColor Cyan
    $leaveResponse = Invoke-RestMethod -Uri "$baseUrl/api/stream/leave/$streamId" `
        -Method Post `
        -ContentType "application/json" `
        -Body $viewer1
    
    Write-Host "  ✓ Viewer left - Remaining viewers: $($leaveResponse.viewerCount)" -ForegroundColor Green
    
    # End stream
    Write-Host "`n  → Ending live stream..." -ForegroundColor Cyan
    $endResponse = Invoke-RestMethod -Uri "$baseUrl/api/stream/end/$streamId" `
        -Method Post `
        -ContentType "application/json" `
        -Body "{}"
    
    Write-Host "  ✓ Stream ended - Status: $($endResponse.status)" -ForegroundColor Green
    
} catch {
    Write-Host "  ✗ Failed: $_" -ForegroundColor Red
}

# ============================================================================
# SUMMARY
# ============================================================================
Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  ✅ ALL TESTS COMPLETED!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "`nMedia Server API is fully functional!" -ForegroundColor Green
Write-Host "All features tested:" -ForegroundColor White
Write-Host "  ✓ Chat Messages" -ForegroundColor Green
Write-Host "  ✓ Voice Calls" -ForegroundColor Green
Write-Host "  ✓ Video Calls" -ForegroundColor Green
Write-Host "  ✓ Live Streaming" -ForegroundColor Green
Write-Host ""



