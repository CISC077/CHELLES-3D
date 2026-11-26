@echo off
rem ------------------------------------------------------------------
rem  COMMERCES OUVERTS - CHELLES 3D (double-clic, Unreal FERME)
rem   1. compile le plugin ChellesCommerces (pieces + boutiques) ;
rem   2. ouvre Unreal sur Rachid, construit la BOULANGERIE et le TABAC-PRESSE
rem      pres du CHELLES BAR, enregistre et referme.
rem  Journal : Tools\Commerces\commerces.log
rem ------------------------------------------------------------------
setlocal
set "SCRIPT=installer_commerces.py"
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"

tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)
if not exist "%PROJET%\Plugins\ChellesCommerces\ChellesCommerces.uplugin" (
    echo Le plugin Plugins\ChellesCommerces est introuvable. Abandon.
    pause
    exit /b 1
)

echo.
echo ===== 1/2 Compilation du plugin ChellesCommerces (1 a 5 minutes) =====
call "%PROJET%\Plugins\ChellesCommerces\Compiler_le_plugin.bat" SANSPAUSE
if errorlevel 1 (
    echo.
    echo ECHEC de la compilation : rien n'a ete change dans la carte.
    echo Envoyez a Claude le fichier Plugins\ChellesCommerces\Journal_compilation.txt
    pause
    exit /b 1
)

echo.
echo ===== 2/2 Construction de la boulangerie et du tabac-presse dans Rachid =====
set "RELAIS=%TEMP%\chelles_commerces_relais.py"
> "%RELAIS%" echo exec(open(r"%~dp0%SCRIPT%", encoding="utf-8").read())
set "CHELLES_QUITTER=1"
echo Unreal s'ouvre, travaille puis se referme tout seul (quelques minutes)...
"%UE%\Engine\Binaries\Win64\UE4Editor.exe" "%PROJET%\FPS_Chelles.uproject" /Game/Maps/Rachid -ExecutePythonScript="%RELAIS%"
echo.
type "%~dp0commerces.log"
echo.
echo Ouvrez le projet, lancez Play et entrez dans la BOULANGERIE, le TABAC-PRESSE ou le CHELLES BAR.
pause
