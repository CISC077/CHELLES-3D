@echo off
title CHELLES 3D - Installer le MODE PHOTO
rem =====================================================================
rem  MODE PHOTO (touche F12) - 26/11/2026
rem  1. Fermez Unreal.  2. Double-cliquez ici.  3. Attendez "TERMINE".
rem  Compile :
rem    - ChellesPhoto (nouveau plugin : le mode photo)
rem    - ChellesMenu  (la ligne MODE PHOTO dans OPTIONS > CONTROLES)
rem =====================================================================
setlocal
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"

set "UE=C:\Program Files\Epic Games\UE_4.27"
if not exist "%UE%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo Unreal 4.27 introuvable dans "%UE%".
    set /p "UE=Collez le dossier d'installation d'Unreal 4.27 (ex: D:\Epic Games\UE_4.27) puis Entree : "
)
if not exist "%UE%\Engine\Build\BatchFiles\RunUAT.bat" (
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

if not exist "%PROJET%\Plugins\ChellesPhoto\ChellesPhoto.uplugin" (
    echo Le plugin ChellesPhoto est introuvable dans "%PROJET%\Plugins".
    pause
    exit /b 1
)

call :compiler ChellesPhoto || goto echec
call :compiler ChellesMenu || goto echec

echo.
echo =====================================================================
echo  TERMINE. Ouvrez CHELLES 3D, lancez une partie et appuyez sur F12.
echo  Les photos vont dans : %USERPROFILE%\Documents\CHELLES 3D - Photos
echo =====================================================================
pause
exit /b 0

:echec
echo.
echo ECHEC de la compilation. Envoyez a Claude le fichier :
echo   "%PROJET%\Plugins\%NOM%\Journal_compilation.txt"
pause
exit /b 1

rem ------------------------------------------------------------------
:compiler
set "NOM=%~1"
set "PLUGIN=%PROJET%\Plugins\%NOM%"
set "SORTIE=%TEMP%\%NOM%_Compile"
if exist "%SORTIE%" rmdir /s /q "%SORTIE%"
echo.
echo ===== Compilation de %NOM% (1 a 10 minutes)... =====
call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN%\%NOM%.uplugin" -Package="%SORTIE%" -TargetPlatforms=Win64 -Rocket -VS2019
set "RESULTAT=%errorlevel%"
for /f "delims=" %%F in ('dir /b /s /o-d "%APPDATA%\Unreal Engine\AutomationTool\Logs\UBT-UE4*-Win64-*.txt" 2^>nul') do (
    copy /y "%%F" "%PLUGIN%\Journal_compilation.txt" >nul
    goto journal_%NOM%
)
:journal_ChellesPhoto
:journal_ChellesMenu
if not "%RESULTAT%"=="0" (
    echo.
    echo ===== ERREURS dans %NOM% =====
    if exist "%PLUGIN%\Journal_compilation.txt" findstr /i /c:": error" /c:": fatal error" "%PLUGIN%\Journal_compilation.txt"
    exit /b 1
)
xcopy /e /i /y "%SORTIE%\Binaries" "%PLUGIN%\Binaries" >nul
xcopy /e /i /y "%SORTIE%\Intermediate" "%PLUGIN%\Intermediate" >nul
rmdir /s /q "%SORTIE%"
echo %NOM% : OK
exit /b 0
