@echo off
rem ------------------------------------------------------------------
rem  SUPPRIMER LE PACK "Sports and Police Cars - Drivable" - CHELLES 3D
rem  (double-clic, Unreal FERME). Demande de MERIDJA le 25/11/2026 :
rem  ces voitures font bugger le serveur.
rem   1. sauvegarde (pack + carte + reglages) dans _Sauvegardes\SuppressionPolice_date
rem   2. Unreal s'ouvre seul : retire les voitures de Rachid, corrige les reglages, se referme
rem   3. efface Content\SportVehiclesPack
rem   4. recompile ChellesRoue (case VOITURE = voiture de sport VehicleVarietyPack)
rem  Journal : Tools\SuppressionPolice\suppression.log
rem ------------------------------------------------------------------
setlocal
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "ICI=%~dp0"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "PACK=%PROJET%\Content\SportVehiclesPack"

tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)
if not exist "%PACK%" (
    echo Le pack SportVehiclesPack est deja supprime. Rien a faire.
    pause
    exit /b 0
)

for /f %%D in ('powershell -NoProfile -Command "Get-Date -Format yyyy-MM-dd_HH\hmm"') do set "DATE_SAV=%%D"
set "CHELLES_SAUVEGARDE=%PROJET%\_Sauvegardes\SuppressionPolice_%DATE_SAV%"
echo.
echo [1/4] Sauvegarde dans %CHELLES_SAUVEGARDE% ...
robocopy "%PACK%" "%CHELLES_SAUVEGARDE%\Content\SportVehiclesPack" /E /NFL /NDL /NJH /NJS /NP >nul
if errorlevel 8 (
    echo ECHEC de la sauvegarde du pack. Rien n'a ete supprime.
    pause
    exit /b 1
)
if not exist "%CHELLES_SAUVEGARDE%\Content\Maps" mkdir "%CHELLES_SAUVEGARDE%\Content\Maps"
copy /y "%PROJET%\Content\Maps\Rachid.umap" "%CHELLES_SAUVEGARDE%\Content\Maps\" >nul
if errorlevel 1 (
    echo ECHEC de la copie de Rachid.umap. Rien n'a ete supprime.
    pause
    exit /b 1
)
if not exist "%CHELLES_SAUVEGARDE%\Plugins\ChellesRoue\Binaries" mkdir "%CHELLES_SAUVEGARDE%\Plugins\ChellesRoue\Binaries"
xcopy /e /i /y /q "%PROJET%\Plugins\ChellesRoue\Binaries" "%CHELLES_SAUVEGARDE%\Plugins\ChellesRoue\Binaries" >nul

echo.
echo [2/4] Unreal s'ouvre, retire les voitures de Rachid puis se referme (quelques minutes)...
if exist "%ICI%etat.txt" del /q "%ICI%etat.txt"
set "RELAIS=%TEMP%\chelles_suppression_police.py"
> "%RELAIS%" echo exec(open(r"%ICI%supprimer_pack_police.py", encoding="utf-8").read())
set "CHELLES_QUITTER=1"
"%UE%\Engine\Binaries\Win64\UE4Editor.exe" "%PROJET%\FPS_Chelles.uproject" /Game/Maps/Rachid -ExecutePythonScript="%RELAIS%"
:attendre
timeout /t 3 /nobreak >nul
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 goto attendre

if not exist "%ICI%etat.txt" (
    echo.
    echo Le travail dans Unreal ne s'est pas termine correctement : le pack N'EST PAS efface.
    echo Envoyez le fichier Tools\SuppressionPolice\suppression.log a Claude.
    type "%ICI%suppression.log"
    pause
    exit /b 1
)

echo.
echo [3/4] Suppression du dossier Content\SportVehiclesPack ...
rmdir /s /q "%PACK%"
if exist "%PACK%" (
    echo ATTENTION : le dossier n'a pas pu etre efface entierement.
) else (
    echo Pack supprime.
)

echo.
echo [4/4] Compilation de ChellesRoue (1 a 5 minutes)...
set "SORTIE=%TEMP%\ChellesRoue_Compile"
if exist "%SORTIE%" rmdir /s /q "%SORTIE%"
call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PROJET%\Plugins\ChellesRoue\ChellesRoue.uplugin" -Package="%SORTIE%" -TargetPlatforms=Win64 -Rocket -VS2019 > "%ICI%compilation_roue.log" 2>&1
if errorlevel 1 (
    echo ECHEC de la compilation de ChellesRoue : voir Tools\SuppressionPolice\compilation_roue.log
    echo Le reste est fait. Envoyez ce fichier a Claude.
) else (
    xcopy /e /i /y /q "%SORTIE%\Binaries" "%PROJET%\Plugins\ChellesRoue\Binaries" >nul
    xcopy /e /i /y /q "%SORTIE%\Intermediate" "%PROJET%\Plugins\ChellesRoue\Intermediate" >nul
    rmdir /s /q "%SORTIE%"
    echo ChellesRoue compile.
)

echo.
echo ================= JOURNAL =================
type "%ICI%suppression.log"
echo.
echo TERMINE. Vous pouvez rouvrir CHELLES 3D.
echo (La sauvegarde %CHELLES_SAUVEGARDE% peut etre effacee quand tout marche.)
pause
