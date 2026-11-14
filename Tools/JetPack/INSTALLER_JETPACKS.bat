@echo off
rem ------------------------------------------------------------------
rem  JETPACKS A RAMASSER - CHELLES 3D (double-clic, Unreal FERME)
rem  1. compile le plugin Plugins\ChellesJetPack (seulement la 1re fois)
rem  2. ouvre Unreal sur Rachid, lance installer_jetpacks.py, enregistre et referme
rem  Journal : Tools\JetPack\jetpack.log
rem ------------------------------------------------------------------
setlocal
set "SCRIPT=installer_jetpacks.py"
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "PLUGIN=%PROJET%\Plugins\ChellesJetPack"

if not exist "%UE%\Engine\Binaries\Win64\UE4Editor.exe" (
    echo Unreal 4.27 introuvable dans "%UE%".
    set /p "UE=Collez le dossier d'installation d'Unreal 4.27 (ex: D:\Epic Games\UE_4.27) puis Entree : "
)
if not exist "%PLUGIN%\ChellesJetPack.uplugin" (
    echo Le plugin est introuvable : "%PLUGIN%"
    pause
    exit /b 1
)
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)

rem ---------- 1. compilation du plugin (si pas encore fait)
if exist "%PLUGIN%\Binaries\Win64\UE4Editor-ChellesJetPack.dll" (
    echo Plugin ChellesJetPack deja compile.
    goto installer
)
set "SORTIE=%TEMP%\ChellesJetPack_Compile"
if exist "%SORTIE%" rmdir /s /q "%SORTIE%"
echo.
echo 1/2 Compilation du plugin ChellesJetPack (1 a 5 minutes)...
call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN%\ChellesJetPack.uplugin" -Package="%SORTIE%" -TargetPlatforms=Win64 -Rocket -VS2019
set "RESULTAT=%errorlevel%"
for /f "delims=" %%F in ('dir /b /s /o-d "%APPDATA%\Unreal Engine\AutomationTool\Logs\UBT-UE4*-Win64-*.txt" 2^>nul') do (
    copy /y "%%F" "%PLUGIN%\Journal_compilation.txt" >nul
    goto journal_copie
)
:journal_copie
if not "%RESULTAT%"=="0" (
    echo.
    echo ===== ERREURS - le journal complet est dans Plugins\ChellesJetPack\Journal_compilation.txt =====
    if exist "%PLUGIN%\Journal_compilation.txt" findstr /i /c:": error" /c:": fatal error" "%PLUGIN%\Journal_compilation.txt"
    echo.
    echo ECHEC de la compilation : envoyez le message d'erreur ci-dessus a Claude.
    pause
    exit /b 1
)
xcopy /e /i /y "%SORTIE%\Binaries" "%PLUGIN%\Binaries" >nul
xcopy /e /i /y "%SORTIE%\Intermediate" "%PLUGIN%\Intermediate" >nul
rmdir /s /q "%SORTIE%"
echo Plugin compile.

:installer
rem ---------- 2. Blueprints + pose dans Rachid
set "RELAIS=%TEMP%\chelles_jetpack_relais.py"
> "%RELAIS%" echo exec(open(r"%~dp0%SCRIPT%", encoding="utf-8").read())
set "CHELLES_QUITTER=1"
echo.
echo 2/2 Unreal s'ouvre, cree les JetPacks, les pose dans Rachid puis se referme (quelques minutes)...
"%UE%\Engine\Binaries\Win64\UE4Editor.exe" "%PROJET%\FPS_Chelles.uproject" /Game/Maps/Rachid -ExecutePythonScript="%RELAIS%"
echo.
type "%~dp0jetpack.log"
pause
