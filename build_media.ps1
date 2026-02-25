# PowerShell build script for Media Server

Write-Host "=== Building Media Server API ===" -ForegroundColor Green

Write-Host "`nChecking for C++ compiler..." -ForegroundColor Yellow

# Stop any running instances first
Get-Process | Where-Object {$_.ProcessName -eq 'media_server'} | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500

# Try to find g++ (MinGW)
$gpp = Get-Command g++ -ErrorAction SilentlyContinue
if ($gpp) {
    Write-Host "✓ Found g++ at: $($gpp.Source)" -ForegroundColor Green
    Write-Host "`nCompiling Media Server..." -ForegroundColor Yellow
    
    g++ -std=c++17 media_server.cpp -o media_server.exe -lws2_32 -lbcrypt -O2 -static
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Build successful!" -ForegroundColor Green
        Write-Host "`nExecutable created: media_server.exe" -ForegroundColor Cyan
        Write-Host "`nTo run the server:" -ForegroundColor Cyan
        Write-Host "  .\media_server.exe" -ForegroundColor White
        Write-Host "`nTo test all APIs:" -ForegroundColor Cyan
        Write-Host "  .\test_media.ps1" -ForegroundColor White
        exit 0
    } else {
        Write-Host "✗ Build failed" -ForegroundColor Red
        exit 1
    }
}

# Try to find cl (MSVC)
$cl = Get-Command cl -ErrorAction SilentlyContinue
if ($cl) {
    Write-Host "✓ Found MSVC cl at: $($cl.Source)" -ForegroundColor Green
    Write-Host "`nCompiling Media Server..." -ForegroundColor Yellow
    
    cl /EHsc /std:c++17 /O2 media_server.cpp /Fe:media_server.exe ws2_32.lib bcrypt.lib
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Build successful!" -ForegroundColor Green
        Write-Host "`nExecutable created: media_server.exe" -ForegroundColor Cyan
        
        # Clean up MSVC temporary files
        Remove-Item *.obj -ErrorAction SilentlyContinue
        
        exit 0
    } else {
        Write-Host "✗ Build failed" -ForegroundColor Red
        exit 1
    }
}

# No compiler found
Write-Host "✗ No C++ compiler found!" -ForegroundColor Red
Write-Host "`nPlease install one of the following:" -ForegroundColor Yellow
Write-Host "  1. MinGW-w64: https://www.mingw-w64.org/" -ForegroundColor White
Write-Host "  2. TDM-GCC: https://jmeubank.github.io/tdm-gcc/" -ForegroundColor White
exit 1


