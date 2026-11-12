@echo off
rem ------------------------------------------------------------------
rem  YEUX ROUGES LUMINEUX DES ZOMBIES - CHELLES 3D (double-clic, Unreal FERME)
rem  Ouvre Unreal, lance creer_yeux_zombies.py, enregistre et referme.
rem  Journal : Tools\Zombies\yeux.log
rem ------------------------------------------------------------------
setlocal
set "SCRIPT=creer_yeux_zombies.py"
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : lancez plutot File ^> Execute Python Script ^> Tools\Zombies\%SCRIPT%
    pause
    exit /b 1
)
set "RELAIS=%TEMP%\chelles_yeux_relais.py"
> "%RELAIS%" echo exec(open(r"%~dp0%SCRIPT%", encoding="utf-8").read())
set "CHELLES_QUITTER=1"
echo Unreal s'ouvre, cree le materiau puis se referme tout seul (1 a 3 minutes)...
if exist "%~dp0yeux.log" del "%~dp0yeux.log"
"%UE%\Engine\Binaries\Win64\UE4Editor.exe" "%PROJET%\FPS_Chelles.uproject" -ExecutePythonScript="%RELAIS%"
echo.
if exist "%~dp0yeux.log" (
    type "%~dp0yeux.log"
) else (
    echo Pas de journal : Unreal s'est ferme avant la fin.
    echo Ouvrez Unreal puis File ^> Execute Python Script ^> Tools\Zombies\%SCRIPT%
)
pause
