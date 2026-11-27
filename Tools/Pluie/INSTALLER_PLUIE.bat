@echo off
chcp 65001 >nul
title CHELLES 3D - Installation de la pluie et des flaques
rem =====================================================================
rem  PLUIE + FLAQUES (26/11/2026) - Unreal FERME, double-clic.
rem   1. compile le plugin Plugins\ChellesPluie (1 a 5 minutes)
rem   2. recompile Plugins\ChellesMenu (nouvelle ligne OPTIONS > AVANCES > PLUIE / FLAQUES)
rem   3. cree les materiaux et le bruit de pluie dans /Game/Chelles/Pluie
rem      (Unreal travaille en arriere-plan, sans fenetre, 2 a 10 minutes)
rem  La carte Rachid et MI_AsphaltRoad ne sont PAS modifies.
rem  Journal : Tools\Pluie\installer_pluie.log (detail : installer_pluie_unreal.log)
rem =====================================================================
setlocal
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "ICI=%PROJET%\Tools\Pluie"

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
echo ===== 1/3 Compilation du plugin ChellesPluie (1 a 5 minutes)...
call :compiler ChellesPluie
if errorlevel 1 goto echec

echo.
echo ===== 2/3 Compilation du plugin ChellesMenu (ligne PLUIE dans OPTIONS)...
call :compiler ChellesMenu
if errorlevel 1 goto echec

echo.
echo ===== 3/3 Creation de la pluie en arriere-plan (2 a 10 minutes, fenetre noire seulement)...
copy /y "%ICI%\creer_pluie.py" "%TEMP%\chelles_creer_pluie.py" >nul
"%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%PROJET%\FPS_Chelles.uproject" -run=pythonscript -script="%TEMP%\chelles_creer_pluie.py" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nosplash -stdout > "%ICI%\installer_pluie_unreal.log" 2>&1

echo.
echo ===== Resultat =====
type "%ICI%\installer_pluie.log"
echo.
echo Pour voir la pluie tout de suite : Play, touche ² puis tapez   Chelles.Pluie Oui
echo Journal : Tools\Pluie\installer_pluie.log (detail : installer_pluie_unreal.log)
pause
exit /b 0

:echec
echo.
echo ECHEC de la compilation. Envoyez le message d'erreur ci-dessus a Claude.
pause
exit /b 1

rem ---------------------------------------------------------------------
:compiler
set "PLUGIN=%PROJET%\Plugins\%~1"
set "SORTIE=%TEMP%\%~1_Compile"
if exist "%SORTIE%" rmdir /s /q "%SORTIE%"
call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN%\%~1.uplugin" -Package="%SORTIE%" -TargetPlatforms=Win64 -Rocket -VS2019
set "RESULTAT=%errorlevel%"
for /f "delims=" %%F in ('dir /b /s /o-d "%APPDATA%\Unreal Engine\AutomationTool\Logs\UBT-UE4*-Win64-*.txt" 2^>nul') do (
    copy /y "%%F" "%PLUGIN%\Journal_compilation.txt" >nul
    goto :journal_copie
)
:journal_copie
if not "%RESULTAT%"=="0" (
    echo.
    echo ===== ERREURS de %~1 - journal complet : Plugins\%~1\Journal_compilation.txt =====
    if exist "%PLUGIN%\Journal_compilation.txt" findstr /i /c:": error" /c:": fatal error" "%PLUGIN%\Journal_compilation.txt"
    exit /b 1
)
xcopy /e /i /y "%SORTIE%\Binaries" "%PLUGIN%\Binaries" >nul
xcopy /e /i /y "%SORTIE%\Intermediate" "%PLUGIN%\Intermediate" >nul
rmdir /s /q "%SORTIE%"
echo Plugin %~1 compile.
exit /b 0
