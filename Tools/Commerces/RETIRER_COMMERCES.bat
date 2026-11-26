@echo off
rem ------------------------------------------------------------------
rem  ENLEVER LA BOULANGERIE ET LE TABAC-PRESSE (double-clic, Unreal FERME)
rem  Le CHELLES BAR et le plugin ChellesCommerces ne sont pas touches.
rem ------------------------------------------------------------------
setlocal
set "SCRIPT=retirer_commerces.py"
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : lancez plutot File ^> Execute Python Script ^> Tools\Commerces\%SCRIPT%
    pause
    exit /b 1
)
set "RELAIS=%TEMP%\chelles_commerces_retirer.py"
> "%RELAIS%" echo exec(open(r"%~dp0%SCRIPT%", encoding="utf-8").read())
set "CHELLES_QUITTER=1"
echo Unreal s'ouvre, enleve les commerces, enregistre et se referme...
"%UE%\Engine\Binaries\Win64\UE4Editor.exe" "%PROJET%\FPS_Chelles.uproject" /Game/Maps/Rachid -ExecutePythonScript="%RELAIS%"
echo.
type "%~dp0commerces.log"
pause
