@echo off
rem ------------------------------------------------------------------
rem  CHELLES 3D - Toutes les armes des packs dans la roue TAB (double-clic, Unreal FERME)
rem  Ouvre Unreal, cherche les modeles d'armes des packs, ecrit Armes_Packs.csv et refait DT_Armes
rem  (armes du jeu + armes des packs ; copie DT_Armes_avant_packs). Enregistre et referme.
rem  Il faut avoir compile Plugins\ChellesRaccourcis et Plugins\ChellesArmes avant.
rem  Journal : Tools\RaccourcisF\packs.log
rem ------------------------------------------------------------------
setlocal
set "SCRIPT=ajouter_armes_packs.py"
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le, ou lancez File ^> Execute Python Script ^> Tools\RaccourcisF\%SCRIPT%
    pause
    exit /b 1
)
set "RELAIS=%TEMP%\chelles_armes_packs_relais.py"
> "%RELAIS%" echo exec(open(r"%~dp0%SCRIPT%", encoding="utf-8").read(), {"__file__": r"%~dp0%SCRIPT%"})
set "CHELLES_QUITTER=1"
echo Unreal s'ouvre, cherche les armes des packs puis se referme tout seul (2 a 5 minutes)...
"%UE%\Engine\Binaries\Win64\UE4Editor.exe" "%PROJET%\FPS_Chelles.uproject" -ExecutePythonScript="%RELAIS%"
echo.
type "%~dp0packs.log"
pause
