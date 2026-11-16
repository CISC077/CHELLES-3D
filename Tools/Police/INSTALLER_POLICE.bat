@echo off
rem ------------------------------------------------------------------
rem  CHELLES 3D - VOITURES DE POLICE a la place du SUV (double-clic, Unreal FERME)
rem   1. range le SUV (pack DriveableCar_basic + plugin ChellesConduite) dans _Quarantaine
rem      apres verification (rien n'est supprime ; REMETTRE_LE_SUV.bat remet tout)
rem   2. compile les plugins ChellesPolice, ChellesRoue et ChellesCirculation
rem   3. ouvre Unreal, cree BP_Police_Drivable, enregistre et referme
rem  Journaux : Tools\Police\rapport_suv.txt et Tools\Police\police.log
rem ------------------------------------------------------------------
setlocal enabledelayedexpansion
title CHELLES 3D - Installation des voitures de police
cd /d "%~dp0"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "PLUGINS=%PROJET%\Plugins"

set "UE=C:\Program Files\Epic Games\UE_4.27"
for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\4.27" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE=%%b"
if not exist "!UE!\Engine\Binaries\Win64\UE4Editor.exe" (
    echo Unreal 4.27 introuvable dans "!UE!".
    set /p "UE=Collez le dossier d'installation d'Unreal 4.27 (ex: D:\Epic Games\UE_4.27) puis Entree : "
)
if not exist "!UE!\Engine\Binaries\Win64\UE4Editor.exe" (
    echo Toujours introuvable. Abandon.
    pause
    exit /b 1
)

tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)

echo.
echo ===== 1/3 : le SUV part en quarantaine =====
set "PY=!UE!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
if exist "!PY!" (
    "!PY!" -I "%~dp0police_installer.py" installer
) else (
    echo Python d'Unreal introuvable : etape passee, le SUV reste en place.
)

echo.
echo ===== 2/3 : compilation des plugins (1 a 5 minutes chacun) =====
set "ECHECS="
for %%P in (ChellesPolice ChellesRoue ChellesCirculation) do (
    if exist "%PLUGINS%\%%P\%%P.uplugin" call :compiler %%P
)
if defined ECHECS (
    echo.
    echo ECHEC de la compilation de :!ECHECS!
    echo Le journal est dans Plugins\NOM_DU_PLUGIN\Journal_compilation.txt : envoyez-le a Claude.
    pause
    exit /b 1
)

echo.
echo ===== 3/3 : creation de BP_Police_Drivable =====
echo Unreal s'ouvre, cree la voiture puis se referme tout seul (quelques minutes)...
set "RELAIS=%TEMP%\chelles_police_bp.py"
> "%RELAIS%" echo exec(open(r"%~dp0creer_bp_police.py", encoding="utf-8").read())
set "CHELLES_QUITTER=1"
"!UE!\Engine\Binaries\Win64\UE4Editor.exe" "%PROJET%\FPS_Chelles.uproject" /Game/Maps/Rachid -ExecutePythonScript="%RELAIS%"
echo.
if exist "%~dp0police.log" type "%~dp0police.log"

echo.
echo TERMINE. Ouvrez CHELLES 3D, lancez Play :
echo   TAB puis POLICE = conduire une voiture de police
echo   F8 = poser une voiture la ou vous visez, F9 = retirer celle visee
echo   E pres d'une voiture = monter / descendre, L = sirene, H = klaxon
pause
exit /b 0

:compiler
set "NOM=%~1"
set "SORTIE=%TEMP%\Chelles_Compile_%NOM%"
if exist "%SORTIE%" rmdir /s /q "%SORTIE%"
echo.
echo --- %NOM% ...
call "!UE!\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGINS%\%NOM%\%NOM%.uplugin" -Package="%SORTIE%" -TargetPlatforms=Win64 -Rocket -VS2019
set "RESULTAT=!errorlevel!"
for /f "delims=" %%F in ('dir /b /s /o-d "%APPDATA%\Unreal Engine\AutomationTool\Logs\UBT-UE4*-Win64-*.txt" 2^>nul') do (
    copy /y "%%F" "%PLUGINS%\%NOM%\Journal_compilation.txt" >nul
    goto :journal_%NOM%
)
:journal_ChellesPolice
:journal_ChellesRoue
:journal_ChellesCirculation
if not "!RESULTAT!"=="0" (
    echo %NOM% : ECHEC
    if exist "%PLUGINS%\%NOM%\Journal_compilation.txt" findstr /i /c:": error" /c:": fatal error" "%PLUGINS%\%NOM%\Journal_compilation.txt"
    set "ECHECS=!ECHECS! %NOM%"
    goto :eof
)
xcopy /e /i /y "%SORTIE%\Binaries" "%PLUGINS%\%NOM%\Binaries" >nul
xcopy /e /i /y "%SORTIE%\Intermediate" "%PLUGINS%\%NOM%\Intermediate" >nul
rmdir /s /q "%SORTIE%"
echo %NOM% : OK
goto :eof
