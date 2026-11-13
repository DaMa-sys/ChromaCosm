@echo off
setlocal enabledelayedexpansion
title DaMA ChromaCosm - Windows Build Script

echo ============================================================
echo         DaMA ChromaCosm - Dev Kit Build Utility
echo ============================================================
echo.

cd /d "%~dp0\.."

set BUILD_TYPE=Release
if "%1"=="Debug" set BUILD_TYPE=Debug
if "%1"=="debug" set BUILD_TYPE=Debug

echo [*] Modalita selezionata: %BUILD_TYPE%
echo [*] Verifica configurazione CMake...

if not exist build (
    mkdir build
)

cmake -B build -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% NEQ 0 (
    echo [!] Errore durante la generazione dei file di progetto CMake.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [*] Avvio della compilazione parallela (%BUILD_TYPE%)...
cmake --build build --config %BUILD_TYPE% --parallel 8
if %ERRORLEVEL% NEQ 0 (
    echo [!] Errore durante la compilazione.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ============================================================
echo  [+] COMPILAZIONE COMPLETATA CON SUCCESSO!
echo  Binari generati in:
echo   - VST3:       build\ChromaCosm_artefacts\%BUILD_TYPE%\VST3\ChromaCosm.vst3
echo   - Standalone: build\ChromaCosm_artefacts\%BUILD_TYPE%\Standalone\ChromaCosm.exe
echo ============================================================
echo.
pause
