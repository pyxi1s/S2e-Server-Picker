@echo off
setlocal

set "ACTION=%~1"
if "%ACTION%"=="" set "ACTION=build"

where cmake >nul 2>nul
if errorlevel 1 goto no_cmake

if /i "%ACTION%"=="build"   goto build
if /i "%ACTION%"=="debug"   goto debug
if /i "%ACTION%"=="run"     goto run
if /i "%ACTION%"=="rebuild" goto rebuild
if /i "%ACTION%"=="clean"   goto clean
goto help

:pick_generator
set "GEN_ARGS="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not defined GENERATOR goto detect_vs
set GEN_ARGS=-G %GENERATOR%
echo [build.bat] Using generator override.
exit /b 0

:detect_vs
set "VSPATH="
if exist "%VSWHERE%" for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH goto try_mingw
echo [build.bat] Using Visual Studio C++ tools.
exit /b 0

:try_mingw
where g++ >nul 2>nul
if errorlevel 1 goto no_compiler
where mingw32-make >nul 2>nul
if errorlevel 1 goto no_compiler
set GEN_ARGS=-G "MinGW Makefiles"
echo [build.bat] Using MinGW Makefiles (g++ found in PATH)
exit /b 0

:configure_and_build
call :pick_generator
if errorlevel 1 exit /b 1
if exist build\CMakeCache.txt (
    findstr /c:"CMAKE_BUILD_TYPE:STRING=%~1" build\CMakeCache.txt >nul 2>nul
    if errorlevel 1 rmdir /s /q build
)
if exist build\CMakeCache.txt findstr /c:"NMake Makefiles" build\CMakeCache.txt >nul 2>nul && rmdir /s /q build
cmake -S . -B build %GEN_ARGS% -DCMAKE_BUILD_TYPE=%~1
if errorlevel 1 exit /b 1
cmake --build build --config %~1
if errorlevel 1 exit /b 1
echo.
if exist build\%~1\s2e-server-picker.exe (
    echo Built: build\%~1\s2e-server-picker.exe
) else (
    echo Built: build\s2e-server-picker.exe
)
exit /b 0

:build
call :configure_and_build Release
if errorlevel 1 exit /b 1
goto end

:debug
call :configure_and_build Debug
if errorlevel 1 exit /b 1
goto end

:run
call :configure_and_build Release
if errorlevel 1 exit /b 1
if exist build\Release\s2e-server-picker.exe (
    build\Release\s2e-server-picker.exe
) else (
    build\s2e-server-picker.exe
)
goto end

:rebuild
if exist build rmdir /s /q build
goto build

:clean
if exist build rmdir /s /q build
echo Cleaned.
goto end

:no_cmake
echo [ERROR] cmake was not found in PATH.
echo Install it with: winget install Kitware.CMake
echo Then open a NEW terminal and try again.
exit /b 1

:no_compiler
echo [ERROR] No C++ compiler found. Install ONE of these, then open a NEW terminal:
echo   Option A (MinGW, small):  winget install BrechtSanders.WinLibs.POSIX.UCRT
echo   Option B (Visual Studio): install "Build Tools for Visual Studio" with the
echo                             "Desktop development with C++" workload.
exit /b 1

:help
echo Usage: build.bat [build^|debug^|run^|rebuild^|clean]
echo   build    Configure and compile (Release)
echo   debug    Compile a Debug build
echo   run      Build, then launch (asks for administrator rights)
echo   rebuild  Clean, then build
echo   clean    Remove the build directory

:end
endlocal
