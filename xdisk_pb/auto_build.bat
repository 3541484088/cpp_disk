@echo off
:start
protoc -I=./ --cpp_out=./ xms_disk_client_gui.proto



choice /t 3 /d y /n >nul
goto start