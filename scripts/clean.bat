@echo off
setlocal
cd /d "%~dp0\.."

echo [*] Eliminazione della cartella di compilazione build...
if exist build (
    rmdir /s /q build
    echo [+] Cartella build rimossa con successo.
) else (
    echo [*] Nessuna cartella build presente.
)

pause
