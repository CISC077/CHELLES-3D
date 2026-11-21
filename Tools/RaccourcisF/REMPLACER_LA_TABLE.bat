@echo off
rem ------------------------------------------------------------------
rem  CHELLES 3D - Refaire les tableaux DT_Raccourcis et DT_Armes depuis Raccourcis.csv et Armes.csv (ECRASE vos changements) (double-clic, Unreal FERME)
rem  Ouvre Unreal, cree DT_Raccourcis (touches F1-F12) et DT_Armes (roue TAB) dans /Game/Chelles/Donnees, enregistre et referme.
rem  Il faut avoir compile Plugins\ChellesRaccourcis avant.
rem  Journal : Tools\RaccourcisF\table.log
rem ------------------------------------------------------------------
setlocal
set "SCRIPT=creer_table_raccourcis.py"
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le, ou lancez File ^> Execute Python Script ^> Tools\RaccourcisF\%SCRIPT%
    pause
    exit /b 1
)
set "RELAIS=%TEMP%\chelles_raccourcis_relais.py"
> "%RELAIS%" echo exec(open(r"%~dp0%SCRIPT%", encoding="utf-8").read(), {"__file__": r"%~dp0%SCRIPT%"})
set "CHELLES_QUITTER=1"
set "CHELLES_REMPLACER=1"
echo Unreal s'ouvre, cree le tableau puis se referme tout seul (1 a 3 minutes)...
"%UE%\Engine\Binaries\Win64\UE4Editor.exe" "%PROJET%\FPS_Chelles.uproject" -ExecutePythonScript="%RELAIS%"
echo.
type "%~dp0table.log"
pause
