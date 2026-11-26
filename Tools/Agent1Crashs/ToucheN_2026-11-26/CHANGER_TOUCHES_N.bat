@echo off
setlocal enabledelayedexpansion
title Changer les touches N - CHELLES 3D
rem ------------------------------------------------------------------
rem  Agent 1. A lancer UNE fois, double-clic, Unreal FERME.
rem  26/11/2026 : N coupe la musique. Lacher le JetPack passe sur Suppr, MISSIONS sur F10.
rem  Recompile ChellesJetPack et ChellesMenu. Copies d'avant : *.avant_touche_n
rem ------------------------------------------------------------------
cd /d "%~dp0"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"

set "UE=C:\Program Files\Epic Games\UE_4.27"
for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\4.27" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE=%%b"
if not exist "%UE%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo  Unreal 4.27 introuvable dans "%UE%". Dis-le a Claude.
    pause
    exit /b 1
)

:attendre_unreal
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo.
    echo  Unreal est ouvert : ENREGISTRE ton travail, ferme Unreal, puis appuie sur une touche.
    pause >nul
    goto attendre_unreal
)

echo.
echo  ===== compilation de 2 plugins, 2 a 6 minutes, ne ferme pas cette fenetre =====
set "ECHECS=0"
call :compiler ChellesJetPack
call :compiler ChellesMenu
if not "%ECHECS%"=="0" (
    echo.
    echo  ECHEC de la compilation : le plugin en echec garde son ancienne version.
    echo  Dis-le a Claude, il lit Journal_compilation_complet.txt dans le dossier du plugin.
    pause
    exit /b 1
)

echo.
echo  TERMINE. Tu peux rouvrir CHELLES 3D et jouer.
pause
exit /b 0

:compiler
set "NOM=%~1"
set "PLUGIN=%PROJET%\Plugins\%NOM%"
set "SORTIE=%TEMP%\Chelles_Compile_%NOM%"
if exist "%SORTIE%" rmdir /s /q "%SORTIE%"
echo.
echo  --- %NOM% ...
call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN%\%NOM%.uplugin" -Package="%SORTIE%" -TargetPlatforms=Win64 -Rocket -VS2019 > "%PLUGIN%\Journal_compilation_complet.txt" 2>&1
set "RESULTAT=%errorlevel%"
if not "%RESULTAT%"=="0" (
    echo  %NOM% : ECHEC
    findstr /i /c:": error" /c:": fatal error" "%PLUGIN%\Journal_compilation_complet.txt"
    set /a ECHECS+=1
    goto :eof
)
xcopy /e /i /y "%SORTIE%\Binaries" "%PLUGIN%\Binaries" >nul
xcopy /e /i /y "%SORTIE%\Intermediate" "%PLUGIN%\Intermediate" >nul
rmdir /s /q "%SORTIE%"
echo  %NOM% : OK
goto :eof
