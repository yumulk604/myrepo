@echo off
title Media Server - Starting...
color 0A

echo.
echo ========================================
echo   Media Server - Hizli Baslatma
echo ========================================
echo.
echo [1/3] Klasore geciliyor...
cd /d "%~dp0"
echo       Konum: %CD%
echo.

echo [2/3] Sunucu baslatiliyor...
echo       Adres: http://localhost:8080
echo.
echo [3/3] Tarayici aciliyor...
timeout /t 2 /nobreak >nul
start "" "http://localhost:8080"
echo.
echo ========================================
echo   Sunucu Calistirildi!
echo   Durdurmak icin Ctrl+C basin
echo ========================================
echo.

media_server.exe

echo.
echo Sunucu durduruldu.
pause



