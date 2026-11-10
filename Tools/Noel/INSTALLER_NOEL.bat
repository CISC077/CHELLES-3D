@echo off
rem ------------------------------------------------------------------
rem  EVENEMENT DE NOEL - CHELLES 3D (double-clic, Unreal FERME)
rem   1. compile le plugin Plugins\ChellesNoel (1 a 5 minutes)
rem   2. installe la neige + le sol de Noel dans la carte Rachid
rem      (une sauvegarde de Rachid est faite dans Saved\Sauvegardes\)
rem  Journal : Tools\Noel\installer_noel.log
rem ------------------------------------------------------------------
setlocal
title Installation de l'evenement de Noel

set "UE=C:\Program Files\Epic Games\UE_4.27"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "PLUGIN=%PROJET%\Plugins\ChellesNoel"

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

echo.
echo ===== 1/2 Compilation du plugin ChellesNoel (1 a 5 minutes)...
set "SORTIE=%TEMP%\ChellesNoel_Compile"
if exist "%SORTIE%" rmdir /s /q "%SORTIE%"
call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN%\ChellesNoel.uplugin" -Package="%SORTIE%" -TargetPlatforms=Win64 -Rocket -VS2019
set "RESULTAT=%errorlevel%"
for /f "delims=" %%F in ('dir /b /s /o-d "%APPDATA%\Unreal Engine\AutomationTool\Logs\UBT-UE4*-Win64-*.txt" 2^>nul') do (
    copy /y "%%F" "%PLUGIN%\Journal_compilation.txt" >nul
    goto journal_copie
)
:journal_copie
if not "%RESULTAT%"=="0" (
    echo.
    echo ===== ERREURS - journal complet : Plugins\ChellesNoel\Journal_compilation.txt =====
    if exist "%PLUGIN%\Journal_compilation.txt" findstr /i /c:": error" /c:": fatal error" "%PLUGIN%\Journal_compilation.txt"
    echo.
    echo ECHEC de la compilation. Envoyez le message d'erreur ci-dessus a Claude.
    pause
    exit /b 1
)
xcopy /e /i /y "%SORTIE%\Binaries" "%PLUGIN%\Binaries" >nul
xcopy /e /i /y "%SORTIE%\Intermediate" "%PLUGIN%\Intermediate" >nul
rmdir /s /q "%SORTIE%"
echo Plugin ChellesNoel compile.

echo.
echo ===== 2/2 Installation dans la carte Rachid (quelques minutes, compilation des materiaux)...
"%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%PROJET%\FPS_Chelles.uproject" -run=pythonscript -script="%PROJET%\Tools\Noel\installer_noel.py" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nosplash -stdout > "%PROJET%\Tools\Noel\installer_noel_unreal.log" 2>&1

echo.
echo ===== Resultat =====
type "%PROJET%\Tools\Noel\installer_noel.log"
echo.
echo Journal : Tools\Noel\installer_noel.log (detail : installer_noel_unreal.log)
pause
