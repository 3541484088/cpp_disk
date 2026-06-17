@echo off
title cpp_disk Service Stopper

echo Stopping all cpp_disk services...
taskkill /F /IM register_server.exe 2>nul
taskkill /F /IM xauth_server.exe 2>nul
taskkill /F /IM xlog_server.exe 2>nul
taskkill /F /IM config_server.exe 2>nul
taskkill /F /IM xms_gateway.exe 2>nul
taskkill /F /IM xms_dir_service.exe 2>nul
taskkill /F /IM xms_upload_service.exe 2>nul
taskkill /F /IM xms_download_service.exe 2>nul
taskkill /F /IM xms_share_service.exe 2>nul
echo All services stopped.
timeout /t 2 /nobreak >nul
