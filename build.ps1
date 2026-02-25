# PowerShell build script for Chat API

Write-Host "=== Building Chat API (Native Version) ===" -ForegroundColor Green

Write-Host "`nChecking for C++ compiler..." -ForegroundColor Yellow

# Try to find g++ (MinGW)
$gpp = Get-Command g++ -ErrorAction SilentlyContinue
if ($gpp) {
    Write-Host "✓ Found g++ at: $($gpp.Source)" -ForegroundColor Green
    Write-Host "`nCompiling with g++..." -ForegroundColor Yellow
    
    g++ -std=c++17 chat_api_native.cpp -o chat_api.exe -lws2_32 -O2 -static
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Build successful!" -ForegroundColor Green
        Write-Host "`nExecutable created: chat_api.exe" -ForegroundColor Cyan
        Write-Host "`nTo run the server:" -ForegroundColor Cyan
        Write-Host "  .\chat_api.exe" -ForegroundColor White
        Write-Host "`nTo test the API:" -ForegroundColor Cyan
        Write-Host "  .\test_api.ps1" -ForegroundColor White
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
    Write-Host "`nCompiling with MSVC..." -ForegroundColor Yellow
    
    cl /EHsc /std:c++17 /O2 chat_api_native.cpp /Fe:chat_api.exe ws2_32.lib
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Build successful!" -ForegroundColor Green
        Write-Host "`nExecutable created: chat_api.exe" -ForegroundColor Cyan
        Write-Host "`nTo run the server:" -ForegroundColor Cyan
        Write-Host "  .\chat_api.exe" -ForegroundColor White
        Write-Host "`nTo test the API:" -ForegroundColor Cyan
        Write-Host "  .\test_api.ps1" -ForegroundColor White
        
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
Write-Host "  2. Visual Studio Build Tools: https://visualstudio.microsoft.com/downloads/" -ForegroundColor White
Write-Host "  3. MSYS2: https://www.msys2.org/" -ForegroundColor White
Write-Host "  4. TDM-GCC: https://jmeubank.github.io/tdm-gcc/" -ForegroundColor White
exit 1
