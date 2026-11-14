@echo off
rem ------------------------------------------------------------------
rem  COULEURS DES JETPACKS - CHELLES 3D (double-clic, Unreal FERME)
rem  Fabrique le materiau colorable et le donne a BP_JetPack.
rem ------------------------------------------------------------------
setlocal
set "SCRIPT=colorer_jetpacks.py"
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : lancez plutot File ^> Execute Python Script ^> Tools\JetPack\%SCRIPT%
    pause
    exit /b 1
)
set "RELAIS=%TEMP%\chelles_jetpack_colorer.py"
> "%RELAIS%" echo exec(open(r"%~dp0%SCRIPT%", encoding="utf-8").read())
set "CHELLES_QUITTER=1"
echo Unreal s'ouvre, colore les JetPacks puis se referme tout seul...
"%UE%\Engine\Binaries\Win64\UE4Editor.exe" "%PROJET%\FPS_Chelles.uproject" /Game/Maps/Rachid -ExecutePythonScript="%RELAIS%"
echo.
type "%~dp0jetpack.log"
pause
