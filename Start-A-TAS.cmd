@echo off
setlocal
set "APP=%~dp0app"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%APP%\update.ps1"
if "%errorlevel%"=="2" exit /b 0
start "" /D "%APP%" "%APP%\A-TAS-Manager.exe"
