@echo off
rem ------------------------------------------------------------------
rem  Compile le plugin ChellesPouvoirs pour Unreal 4.27 (une seule fois).
rem  Ne modifie pas le projet : seul le dossier Binaries du plugin est cree.
rem  Fermez Unreal avant de lancer ce fichier (double-clic).
rem ------------------------------------------------------------------
setlocal

set "UE=C:\Program Files\Epic Games\UE_4.27"
if not exist "%UE%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo Unreal 4.27 introuvable dans "%UE%".
    set /p "UE=Collez le dossier d'installation d'Unreal 4.27 (ex: D:\Epic Games\UE_4.27) puis Entree : "
)
if not exist "%UE%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo Toujours introuvable. Abandon.
    pause
    exit /b 1
)

tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)

set "PLUGIN=%~dp0"
set "SORTIE=%TEMP%\ChellesPouvoirs_Compile"
if exist "%SORTIE%" rmdir /s /q "%SORTIE%"

echo.
echo Compilation en cours (1 a 5 minutes)...
call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN%ChellesPouvoirs.uplugin" -Package="%SORTIE%" -TargetPlatforms=Win64 -Rocket -VS2019
set "RESULTAT=%errorlevel%"

rem Copie le journal de compilation dans le plugin, Claude peut le lire sans capture
for /f "delims=" %%F in ('dir /b /s /o-d "%APPDATA%\Unreal Engine\AutomationTool\Logs\UBT-UE4*-Win64-*.txt" 2^>nul') do (
    copy /y "%%F" "%PLUGIN%Journal_compilation.txt" >nul
    goto journal_copie
)
:journal_copie

if not "%RESULTAT%"=="0" (
    echo.
    echo ===== ERREURS - le journal complet est dans Journal_compilation.txt =====
    if exist "%PLUGIN%Journal_compilation.txt" findstr /i /c:": error" /c:": fatal error" "%PLUGIN%Journal_compilation.txt"
    echo.
    echo ECHEC de la compilation. Verifiez que Visual Studio 2019 est installe
    echo avec "Developpement Desktop en C++", puis envoyez le message d'erreur ci-dessus.
    pause
    exit /b 1
)

echo Copie des fichiers compiles dans le plugin...
xcopy /e /i /y "%SORTIE%\Binaries" "%PLUGIN%Binaries" >nul
xcopy /e /i /y "%SORTIE%\Intermediate" "%PLUGIN%Intermediate" >nul
rmdir /s /q "%SORTIE%"

echo.
echo TERMINE. Vous pouvez ouvrir le projet CHELLES 3D.
pause
