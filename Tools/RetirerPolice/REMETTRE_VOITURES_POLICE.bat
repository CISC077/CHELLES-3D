@echo off
rem ------------------------------------------------------------------
rem  REMETTRE LES VOITURES DE POLICE DANS RACHID - CHELLES 3D (double-clic, Unreal FERME)
rem  Ouvre Unreal sur Rachid, lance remettre_voitures_police.py, enregistre et referme.
rem  Journal : Tools\RetirerPolice\police.log
rem ------------------------------------------------------------------
setlocal
set "SCRIPT=remettre_voitures_police.py"
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)
set "RELAIS=%TEMP%\chelles_police_relais.py"
> "%RELAIS%" echo exec(open(r"%~dp0%SCRIPT%", encoding="utf-8").read())
set "CHELLES_QUITTER=1"
echo Unreal s'ouvre, travaille puis se referme tout seul (quelques minutes)...
"%UE%\Engine\Binaries\Win64\UE4Editor.exe" "%PROJET%\FPS_Chelles.uproject" /Game/Maps/Rachid -ExecutePythonScript="%RELAIS%"
echo.
type "%~dp0police.log"
pause
