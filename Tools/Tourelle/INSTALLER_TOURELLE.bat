@echo off
rem ------------------------------------------------------------------
rem  TOURELLE BP_Turet2026 - CHELLES 3D (double-clic, Unreal FERME)
rem   1. compile le plugin Plugins\ChellesTourelle (1 a 5 minutes)
rem   2. ajoute le dossier ScifiGun a la liste d'export (DefaultGame.ini, copie faite avant)
rem   3. ouvre Unreal, cree BP_Turet2026 dans Content\ScifiGun, enregistre et referme
rem  Journal : Tools\Tourelle\tourelle.log
rem ------------------------------------------------------------------
setlocal
title Installation de la tourelle Chelles
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "PLUGIN=%PROJET%\Plugins\ChellesTourelle"

if not exist "%UE%\Engine\Binaries\Win64\UE4Editor.exe" (
    echo Unreal 4.27 introuvable dans "%UE%".
    set /p "UE=Collez le dossier d'installation d'Unreal 4.27 (ex: D:\Epic Games\UE_4.27) puis Entree : "
)
if not exist "%PLUGIN%\ChellesTourelle.uplugin" (
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

rem ---------- 1. compilation du plugin
set "SORTIE=%TEMP%\ChellesTourelle_Compile"
if exist "%SORTIE%" rmdir /s /q "%SORTIE%"
echo.
echo 1/3 Compilation du plugin ChellesTourelle (1 a 5 minutes)...
call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN%\ChellesTourelle.uplugin" -Package="%SORTIE%" -TargetPlatforms=Win64 -Rocket -VS2019
set "RESULTAT=%errorlevel%"
for /f "delims=" %%F in ('dir /b /s /o-d "%APPDATA%\Unreal Engine\AutomationTool\Logs\UBT-UE4*-Win64-*.txt" 2^>nul') do (
    copy /y "%%F" "%PLUGIN%\Journal_compilation.txt" >nul
    goto journal_copie
)
:journal_copie
if not "%RESULTAT%"=="0" (
    echo.
    echo ===== ERREURS - le journal complet est dans Plugins\ChellesTourelle\Journal_compilation.txt =====
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

rem ---------- 2. liste d'export
echo.
echo 2/3 Liste d'export (DefaultGame.ini)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0ajouter_cuisson.ps1" -Projet "%PROJET%"

rem ---------- 3. Blueprint BP_Turet2026
set "RELAIS=%TEMP%\chelles_tourelle_relais.py"
> "%RELAIS%" echo exec(open(r"%~dp0creer_bp_turet2026.py", encoding="utf-8").read())
set "CHELLES_QUITTER=1"
echo.
echo 3/3 Unreal s'ouvre, cree BP_Turet2026 dans ScifiGun puis se referme (quelques minutes)...
"%UE%\Engine\Binaries\Win64\UE4Editor.exe" "%PROJET%\FPS_Chelles.uproject" -ExecutePythonScript="%RELAIS%"
echo.
type "%~dp0tourelle.log"
echo.
pause
