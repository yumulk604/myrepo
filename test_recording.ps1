# Test Screen Recording API
$API_BASE = "http://localhost:8080"

Write-Host "=== Testing Screen Recording API ===" -ForegroundColor Cyan

# 1. Start recording
Write-Host "`n1. Starting screen recording..." -ForegroundColor Yellow
$startBody = @{
    userId = "user_test"
    userName = "Test User"
    filename = "test-kayit"
    quality = "1080p"
    captureType = "screen"
} | ConvertTo-Json

try {
    $recording = Invoke-RestMethod -Uri "$API_BASE/api/recording/start" -Method POST -Body $startBody -ContentType "application/json"
    Write-Host "✓ Recording started successfully" -ForegroundColor Green
    Write-Host "  Recording ID: $($recording.id)" -ForegroundColor Gray
    Write-Host "  Status: $($recording.status)" -ForegroundColor Gray
    $recordingId = $recording.id
} catch {
    Write-Host "✗ Failed to start recording: $_" -ForegroundColor Red
    exit 1
}

# 2. List recordings (should show recording in progress)
Write-Host "`n2. Listing recordings..." -ForegroundColor Yellow
try {
    $recordings = Invoke-RestMethod -Uri "$API_BASE/api/recording/list" -Method GET
    Write-Host "✓ Found $($recordings.Count) recording(s)" -ForegroundColor Green
    $recordings | ForEach-Object {
        Write-Host "  - $($_.filename) [$($_.status)] - $($_.quality)" -ForegroundColor Gray
    }
} catch {
    Write-Host "✗ Failed to list recordings: $_" -ForegroundColor Red
}

# Wait a bit
Write-Host "`n3. Simulating recording (waiting 3 seconds)..." -ForegroundColor Yellow
Start-Sleep -Seconds 3

# 4. Stop recording
Write-Host "`n4. Stopping recording..." -ForegroundColor Yellow
$stopBody = @{
    duration = "3"
} | ConvertTo-Json

try {
    $stopped = Invoke-RestMethod -Uri "$API_BASE/api/recording/stop/$recordingId" -Method POST -Body $stopBody -ContentType "application/json"
    Write-Host "✓ Recording stopped successfully" -ForegroundColor Green
    Write-Host "  Status: $($stopped.status)" -ForegroundColor Gray
    Write-Host "  Duration: $($stopped.duration)s" -ForegroundColor Gray
} catch {
    Write-Host "✗ Failed to stop recording: $_" -ForegroundColor Red
}

# 5. Get specific recording
Write-Host "`n5. Getting recording details..." -ForegroundColor Yellow
try {
    $rec = Invoke-RestMethod -Uri "$API_BASE/api/recording/$recordingId" -Method GET
    Write-Host "✓ Recording details retrieved" -ForegroundColor Green
    Write-Host "  ID: $($rec.id)" -ForegroundColor Gray
    Write-Host "  Filename: $($rec.filename)" -ForegroundColor Gray
    Write-Host "  Status: $($rec.status)" -ForegroundColor Gray
    Write-Host "  Quality: $($rec.quality)" -ForegroundColor Gray
    Write-Host "  Duration: $($rec.duration)s" -ForegroundColor Gray
} catch {
    Write-Host "✗ Failed to get recording: $_" -ForegroundColor Red
}

# 6. List recordings (should show stopped recording)
Write-Host "`n6. Listing all recordings..." -ForegroundColor Yellow
try {
    $recordings = Invoke-RestMethod -Uri "$API_BASE/api/recording/list" -Method GET
    Write-Host "✓ Found $($recordings.Count) recording(s)" -ForegroundColor Green
    $recordings | ForEach-Object {
        $statusColor = if ($_.status -eq "stopped") { "Green" } else { "Red" }
        Write-Host "  - $($_.filename) [" -NoNewline -ForegroundColor Gray
        Write-Host $_.status -NoNewline -ForegroundColor $statusColor
        Write-Host "] - $($_.quality) - $($_.duration)s" -ForegroundColor Gray
    }
} catch {
    Write-Host "✗ Failed to list recordings: $_" -ForegroundColor Red
}

Write-Host "`n=== All tests completed! ===" -ForegroundColor Cyan



