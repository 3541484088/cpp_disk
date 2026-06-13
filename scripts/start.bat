@echo off
chcp 65001 >nul
title cpp_disk Service Starter

set BIN_DIR=%~dp0..\build\debug\bin
set SERVER_ROOT=%~dp0..\server_root

if not exist "%BIN_DIR%" (
    echo [FAIL] Build dir not found: %BIN_DIR%
    echo Please build the project first!
    pause
    exit /b 1
)

if not exist "%SERVER_ROOT%" mkdir "%SERVER_ROOT%"
if not exist "%SERVER_ROOT%\shared" mkdir "%SERVER_ROOT%\shared"

echo ========== Starting cpp_disk Services ==========
echo Bin: %BIN_DIR%
echo.

echo [1/8] Starting register_server (Port 20018)...
start /b "" "%BIN_DIR%\register_server.exe" 20018
timeout /t 2 /nobreak >nul

echo [2/8] Starting xauth_server (Port 20012)...
start /b "" "%BIN_DIR%\xauth_server.exe" 20012 127.0.0.1 20018
timeout /t 1 /nobreak >nul

echo [3/8] Starting xlog_server (Port 20030)...
start /b "" "%BIN_DIR%\xlog_server.exe" 127.0.0.1 20018 20030
timeout /t 1 /nobreak >nul

echo [4/8] Starting config_server (Port 20019)...
start /b "" "%BIN_DIR%\config_server.exe" 127.0.0.1 20018 20019
timeout /t 1 /nobreak >nul

echo [5/8] Starting xms_gateway (Port 20010)...
start /b "" "%BIN_DIR%\xms_gateway.exe" 20010 127.0.0.1 20018
timeout /t 2 /nobreak >nul

echo [6/8] Starting xms_dir_service (Port 20300)...
start /b "" "%BIN_DIR%\xms_dir_service.exe" 127.0.0.1 20018
timeout /t 1 /nobreak >nul

echo [7/8] Starting xms_upload_service (Port 20100)...
start /b "" "%BIN_DIR%\xms_upload_service.exe" 127.0.0.1 20018 20100
timeout /t 1 /nobreak >nul

echo [8/8] Starting xms_download_service (Port 20200)...
start /b "" "%BIN_DIR%\xms_download_service.exe" 127.0.0.1 20018 20200
timeout /t 1 /nobreak >nul

echo.
echo ========== All Services Started ==========
echo.
echo Keep this window open to keep services running.
echo Press any key to STOP all services and exit.
echo.
pause >nul

echo.
echo Stopping all services...
taskkill /F /IM register_server.exe 2>nul
taskkill /F /IM xauth_server.exe 2>nul
taskkill /F /IM xlog_server.exe 2>nul
taskkill /F /IM config_server.exe 2>nul
taskkill /F /IM xms_gateway.exe 2>nul
taskkill /F /IM xms_dir_service.exe 2>nul
taskkill /F /IM xms_upload_service.exe 2>nul
taskkill /F /IM xms_download_service.exe 2>nul
echo All services stopped.
timeout /t 1 /nobreak >nul
