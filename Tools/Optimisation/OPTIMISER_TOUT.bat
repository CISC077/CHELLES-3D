@echo off
chcp 65001 >nul
title CHELLES 3D - Optimisation finale
rem =====================================================================
rem  OPTIMISATION FINALE (24/11/2026) - UN SEUL FICHIER A LANCER
rem  1. Fermez Unreal.   2. Double-cliquez ici.   3. Attendez "TERMINE".
rem  Les reglages .ini sont DEJA en place (copies sur le PC le 24/11).
rem  Ce fichier fait le reste :
rem    a) recompile ChellesInventaire et ChellesTourelle (moins d'a-coups)
rem    b) re-enregistre les squelettes qu'Unreal repare a chaque ouverture
rem  Pour tout annuler : ANNULER_OPTIMISATION.bat
rem =====================================================================
setlocal
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "UE=C:\Program Files\Epic Games\UE_4.27"
if not exist "%UE%\Engine\Binaries\DotNET\AutomationTool.exe" (
    echo Unreal 4.27 introuvable dans "%UE%".
    pause
    exit /b 1
)
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)
if not exist "%PROJET%\Tools\out" mkdir "%PROJET%\Tools\out"
set "PLUGINS=%PROJET%\Plugins"
set "ECHECS=0"

echo.
echo ===== ETAPE 1/2 : compilation des plugins modifies =====
for %%P in (ChellesInventaire ChellesTourelle) do (
    if exist "%PLUGINS%\%%P\%%P.uplugin" call :compiler %%P
)

echo.
echo ===== ETAPE 2/2 : squelettes repares a chaque ouverture (2 a 10 minutes) =====
"%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%PROJET%\FPS_Chelles.uproject" -run=pythonscript -script="%PROJET%\Tools\Optimisation\resauver_assets_lents.py" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nosplash -stdout > "%PROJET%\Tools\out\optimisation_unreal.log" 2>&1
findstr /L /C:"[Optim]" "%PROJET%\Tools\out\optimisation_unreal.log"

echo.
if "%ECHECS%"=="0" (
    echo TERMINE. Vous pouvez ouvrir CHELLES 3D.
) else (
    echo %ECHECS% plugin^(s^) en echec : envoyez une photo de cette fenetre a Claude.
    echo Le jeu marche quand meme avec l'ancienne version du plugin.
)
pause
exit /b %ECHECS%

:compiler
set "NOM=%~1"
set "SORTIE=%TEMP%\Chelles_Optim_%NOM%"
if exist "%SORTIE%" rmdir /s /q "%SORTIE%"
echo.
echo ----- %NOM% : compilation (1 a 5 minutes)...
pushd "%UE%\Engine"
"%UE%\Engine\Binaries\DotNET\AutomationTool.exe" BuildPlugin -Plugin="%PLUGINS%\%NOM%\%NOM%.uplugin" -Package="%SORTIE%" -TargetPlatforms=Win64 -Rocket -VS2019 > "%PROJET%\Tools\out\optim_%NOM%.log" 2>&1
set "RESULTAT=%ERRORLEVEL%"
popd
if not "%RESULTAT%"=="0" (
    echo ECHEC de %NOM%. Journal : Tools\out\optim_%NOM%.log
    set /a ECHECS+=1
    goto :eof
)
xcopy /e /i /y "%SORTIE%\Binaries" "%PLUGINS%\%NOM%\Binaries" >nul
xcopy /e /i /y "%SORTIE%\Intermediate" "%PLUGINS%\%NOM%\Intermediate" >nul
rmdir /s /q "%SORTIE%"
echo %NOM% : OK
goto :eof
