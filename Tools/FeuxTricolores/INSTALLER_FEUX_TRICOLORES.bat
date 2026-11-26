@echo off
setlocal enabledelayedexpansion
title Feux tricolores - CHELLES 3D
rem ------------------------------------------------------------------
rem  A lancer UNE fois, double-clic, Unreal FERME.
rem  26/11/2026 : FEUX TRICOLORES aux carrefours, les voitures s'arretent au rouge.
rem  Reglages : Plugins\ChellesCirculation\Circulation.txt (lignes Feux...). Recompile ChellesCirculation.
rem  Copies d'avant : CirculationSpawner.cpp.avant_feux et CirculationSpawner.h.avant_feux
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
echo  ===== compilation de ChellesCirculation, 1 a 3 minutes, ne ferme pas cette fenetre =====
set "ECHECS=0"
call :compiler ChellesCirculation
if not "%ECHECS%"=="0" (
    echo.
    echo  ECHEC de la compilation : le plugin en echec garde son ancienne version.
    echo  Dis-le a Claude, il lit Journal_compilation_complet.txt dans le dossier du plugin.
    pause
    exit /b 1
)

echo.
echo  TERMINE. Rouvre CHELLES 3D, lance la map Rachid : les feux sont aux grands carrefours.
echo  Pour les couper : FeuxTricolores=0 dans Plugins\ChellesCirculation\Circulation.txt
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
