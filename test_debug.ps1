# Simple debug test
$API_BASE = "http://localhost:8080"

Write-Host "1. Start recording..."
$startResp = Invoke-WebRequest -Uri "$API_BASE/api/recording/start" -Method POST -Body '{"userId":"u1","userName":"User","filename":"test","quality":"1080p","captureType":"screen"}' -ContentType "application/json"
$recording = $startResp.Content | ConvertFrom-Json
Write-Host "Recording ID: $($recording.id)"
Write-Host "Status: $($recording.status)"

Write-Host "`n2. Try to stop with full URL..."
Write-Host "URL will be: $API_BASE/api/recording/stop/$($recording.id)"

try {
    $stopResp = Invoke-WebRequest -Uri "$API_BASE/api/recording/stop/$($recording.id)" -Method POST -Body '{"duration":"5"}' -ContentType "application/json" -Verbose
    Write-Host "SUCCESS!"
    Write-Host $stopResp.Content
} catch {
    Write-Host "FAILED!"
    Write-Host "StatusCode:" $_.Exception.Response.StatusCode.value__
    Write-Host "Error:" $_.Exception.Message
    if ($_.Exception.Response) {
        $reader = New-Object System.IO.StreamReader($_.Exception.Response.GetResponseStream())
        $reader.BaseStream.Position = 0
        $responseBody = $reader.ReadToEnd()
        Write-Host "Response Body:" $responseBody
    }
}



