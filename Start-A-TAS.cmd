@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0update.ps1"
if "%errorlevel%"=="2" exit /b 0
start "" /D "%~dp0" "%~dp0A-TAS-Manager.exe"
