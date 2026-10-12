@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title Deadlock Server Picker - Setup

set "PAUSE_CMD=pause"
if /i "%~1"=="--no-pause" set "PAUSE_CMD=rem"
set "FTXUI_TAG=v5.0.0"
set "EXTRA_PATH="
set "NO_PYTHON="

echo ============================================================
echo   Deadlock Server Picker - one-click setup
echo ============================================================

echo.
echo [1/6] Checking CMake...
where cmake >nul 2>nul
if not errorlevel 1 goto cmake_ok
call :winget_install Kitware.CMake "CMake"
if errorlevel 1 goto fail
call :refresh_path
if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "PATH=%PATH%;%ProgramFiles%\CMake\bin"
where cmake >nul 2>nul
if errorlevel 1 goto need_restart
:cmake_ok
echo       OK

echo.
echo [2/6] Checking Git...
where git >nul 2>nul
if not errorlevel 1 goto git_ok
call :winget_install Git.Git "Git"
if errorlevel 1 goto fail
call :refresh_path
if exist "%ProgramFiles%\Git\cmd\git.exe" set "PATH=%PATH%;%ProgramFiles%\Git\cmd"
where git >nul 2>nul
if errorlevel 1 goto need_restart
:git_ok
echo       OK

echo.
echo [3/6] Checking C++ compiler...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSPATH="
if exist "%VSWHERE%" for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if defined VSPATH goto compiler_vs

where g++ >nul 2>nul
if errorlevel 1 goto compiler_install
g++ -v 2>&1 | findstr /c:"Thread model: posix" >nul
if errorlevel 1 goto compiler_old
where mingw32-make >nul 2>nul
if errorlevel 1 goto compiler_install
goto compiler_ok

:compiler_old
echo       Found g++, but it does not use the posix thread model that std::thread needs.
echo       Using a newer MinGW-w64 build instead.

:compiler_install
call :find_winlibs
if defined MINGW_BIN goto compiler_path
call :winget_install BrechtSanders.WinLibs.POSIX.UCRT "MinGW-w64 (WinLibs)"
if errorlevel 1 goto fail
call :find_winlibs
if not defined MINGW_BIN goto need_restart

:compiler_path
set "EXTRA_PATH=%MINGW_BIN%;%EXTRA_PATH%"
set "PATH=%MINGW_BIN%;%PATH%"
echo       Using MinGW from: %MINGW_BIN%
goto compiler_ok

:compiler_vs
echo       Using Visual Studio C++ tools.

:compiler_ok
echo       OK

echo.
echo [4/6] Checking Python (used to fetch server IPs)...
python --version >nul 2>nul
if not errorlevel 1 goto python_ok
call :find_python
python --version >nul 2>nul
if not errorlevel 1 goto python_ok
call :winget_install Python.Python.3.12 "Python 3"
call :refresh_path
call :find_python
python --version >nul 2>nul
if not errorlevel 1 goto python_ok
set "NO_PYTHON=1"
echo       [warn] Python is not available. Server IP update will be skipped.
goto python_done
:python_ok
echo       OK
:python_done

echo.
echo [5/6] Downloading FTXUI %FTXUI_TAG%...
if exist "deps\FTXUI\CMakeLists.txt" goto ftxui_ok
if exist deps\FTXUI rmdir /s /q deps\FTXUI
if not exist deps mkdir deps
set "GIT_HTTP_LOW_SPEED_LIMIT=1000"
set "GIT_HTTP_LOW_SPEED_TIME=30"
set /a TRIES=0
:clone_retry
set /a TRIES+=1
echo       git clone attempt %TRIES% of 3...
git clone --depth 1 --branch %FTXUI_TAG% https://github.com/ArthurSonzogni/FTXUI.git deps\FTXUI
if not errorlevel 1 goto ftxui_check
if exist deps\FTXUI rmdir /s /q deps\FTXUI
if %TRIES% LSS 3 goto clone_retry

echo       git clone failed, trying zip download...
curl -fL --connect-timeout 20 --max-time 300 -o deps\ftxui.zip https://codeload.github.com/ArthurSonzogni/FTXUI/zip/refs/tags/%FTXUI_TAG%
if errorlevel 1 goto fail_ftxui
tar -xf deps\ftxui.zip -C deps
if errorlevel 1 goto fail_ftxui
del deps\ftxui.zip >nul 2>nul
for /d %%D in (deps\FTXUI-*) do ren "%%D" FTXUI

:ftxui_check
if not exist "deps\FTXUI\CMakeLists.txt" goto fail_ftxui
:ftxui_ok
echo       OK

echo.
echo [6/6] Fetching server IPs and building...
if defined NO_PYTHON goto build
python tools\update_servers.py --write
if errorlevel 1 goto servers_failed
echo       Server IPs updated.
goto build
:servers_failed
echo       [warn] Could not update server IPs. Placeholders stay in place, so ping and blocking
echo       will not work until you run:  python tools\update_servers.py --write

:build
call build.bat build
if errorlevel 1 goto fail_build

set "EXE=build\deadlock-server-picker.exe"
if exist "build\Release\deadlock-server-picker.exe" set "EXE=build\Release\deadlock-server-picker.exe"
for %%I in ("%EXE%") do set "EXE_FULL=%%~fI"

echo.
echo Creating Desktop and Start Menu shortcuts...
powershell -NoProfile -ExecutionPolicy Bypass -File "tools\create_shortcuts.ps1" -Exe "%EXE_FULL%"
if errorlevel 1 echo       [warn] Could not create shortcuts.

echo.
echo ============================================================
echo   Done! Program: %EXE%
echo   It asks for administrator rights itself (UAC prompt).
echo ============================================================
choice /c YN /m "Launch now"
if errorlevel 2 goto done
"%EXE%"

:done
%PAUSE_CMD%
exit /b 0

:winget_install
where winget >nul 2>nul
if errorlevel 1 goto no_winget
echo       Installing %~2 with winget. This can take a few minutes...
winget install -e --id %~1 --silent --accept-package-agreements --accept-source-agreements
exit /b 0
:no_winget
echo [ERROR] winget was not found. Install "App Installer" from the Microsoft Store,
echo         or install %~2 manually, then run setup.bat again.
exit /b 1

:refresh_path
set "SYSPATH="
set "USRPATH="
for /f "tokens=2*" %%A in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v Path 2^>nul') do set "SYSPATH=%%B"
for /f "tokens=2*" %%A in ('reg query "HKCU\Environment" /v Path 2^>nul') do set "USRPATH=%%B"
call set "PATH=%EXTRA_PATH%%SYSPATH%;%USRPATH%"
exit /b 0

:find_winlibs
set "MINGW_BIN="
for /d %%D in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs*") do if exist "%%~D\mingw64\bin\g++.exe" set "MINGW_BIN=%%~D\mingw64\bin"
exit /b 0

:find_python
for /d %%D in ("%LOCALAPPDATA%\Programs\Python\Python3*") do if exist "%%~D\python.exe" set "EXTRA_PATH=%%~D;%%~D\Scripts;%EXTRA_PATH%" & set "PATH=%%~D;%%~D\Scripts;%PATH%"
exit /b 0

:need_restart
echo.
echo [!] The tools were installed, but this window cannot see them yet.
echo     Close this window and double-click setup.bat again. It will continue where it stopped.
goto fail_end

:fail_ftxui
echo.
echo [ERROR] Could not download FTXUI. Check your internet connection and run setup.bat again.
goto fail_end

:fail_build
echo.
echo [ERROR] The build failed. Scroll up for the compiler message.
goto fail_end

:fail
echo.
echo [ERROR] Setup could not finish.

:fail_end
%PAUSE_CMD%
exit /b 1
