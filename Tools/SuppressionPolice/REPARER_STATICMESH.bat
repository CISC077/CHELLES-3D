@echo off
rem ------------------------------------------------------------------
rem  REPARER /Game/Meshes/StaticMesh apres la suppression du pack police (Unreal FERME)
rem  Journal : Tools\SuppressionPolice\suppression.log
rem ------------------------------------------------------------------
setlocal
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "ICI=%~dp0"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)
set "RELAIS=%TEMP%\chelles_reparer_staticmesh.py"
> "%RELAIS%" echo exec(open(r"%ICI%reparer_staticmesh.py", encoding="utf-8").read())
set "CHELLES_QUITTER=1"
echo Unreal s'ouvre, repare le maillage puis se referme tout seul...
"%UE%\Engine\Binaries\Win64\UE4Editor.exe" "%PROJET%\FPS_Chelles.uproject" /Game/Maps/Rachid -ExecutePythonScript="%RELAIS%"
:attendre
timeout /t 3 /nobreak >nul
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 goto attendre
echo.
findstr /c:"[StaticMesh]" "%ICI%suppression.log"
echo.
echo TERMINE.
if not defined CHELLES_SANS_PAUSE pause
