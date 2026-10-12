@echo off
setlocal EnableExtensions
title Deadlock Server Picker - Uninstall

set "INSTALL_DIR=C:\deadlock_server_picker"
set "APP_NAME=Deadlock Server Picker"

net session >nul 2>&1
if not errorlevel 1 goto elevated
echo Requesting administrator rights...
powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
exit /b 0

:elevated
echo ============================================================
echo   %APP_NAME% - uninstall
echo ============================================================
echo.
echo This will remove:
echo   - the program in %INSTALL_DIR%
echo   - every firewall rule created by this app
echo   - the Desktop and Start Menu shortcuts
echo.
choice /c YN /m "Continue"
if errorlevel 2 goto cancelled

echo.
echo [1/4] Stopping the program...
taskkill /f /im deadlock-server-picker.exe >nul 2>&1

echo [2/4] Removing firewall rules...
powershell -NoProfile -Command "Get-NetFirewallRule -DisplayName 'DeadlockServerPicker_*' -ErrorAction SilentlyContinue | Remove-NetFirewallRule"

echo [3/4] Removing shortcuts...
powershell -NoProfile -Command "foreach ($f in 'Desktop','Programs') { Remove-Item -LiteralPath (Join-Path ([Environment]::GetFolderPath($f)) '%APP_NAME%.lnk') -Force -ErrorAction SilentlyContinue }"

echo [4/4] Removing %INSTALL_DIR%...
if exist "%INSTALL_DIR%" start "" /min cmd /c "ping -n 8 127.0.0.1 >nul & rmdir /s /q %INSTALL_DIR%"

echo.
echo Uninstalled. This window closes in a few seconds.
timeout /t 4 /nobreak >nul
exit /b 0

:cancelled
echo Cancelled.
timeout /t 2 /nobreak >nul
exit /b 0
