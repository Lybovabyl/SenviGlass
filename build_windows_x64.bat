@echo off
setlocal
cd /d "%~dp0"

echo.
echo ========================================
echo   SENVI GLASS - Windows x64 VST3 build
echo ========================================
echo.

where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: CMake was not found.
    echo Install CMake and enable "Add CMake to PATH".
    pause
    exit /b 1
)

where git >nul 2>nul
if errorlevel 1 (
    echo ERROR: Git was not found.
    echo Install Git for Windows and reopen this folder.
    pause
    exit /b 1
)

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :fail

cmake --build build --config Release --target SenviGlass_VST3
if errorlevel 1 goto :fail

echo.
echo BUILD COMPLETE.
echo Your plugin is here:
echo %CD%\build\SenviGlass_artefacts\Release\VST3\SENVI GLASS.vst3
echo.
echo Copy that whole .vst3 folder to:
echo C:\Program Files\Common Files\VST3\
echo.
pause
exit /b 0

:fail
echo.
echo BUILD FAILED. Do not guess: copy the FIRST error block from this window.
echo.
pause
exit /b 1
