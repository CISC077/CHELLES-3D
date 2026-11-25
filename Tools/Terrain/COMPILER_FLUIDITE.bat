@echo off
chcp 65001 >nul
title CHELLES 3D - Agent Terrain : fluidite (changement de perso, mesure FPS)
rem =====================================================================
rem  FLUIDITE (25/11/2026) - Unreal ferme, double-clic, attendre TERMINE.
rem  Recompile : ChellesMenu (compteur FPS detaille + journal [Perf]),
rem  ChellesRoue (costumes des heros charges en avance),
rem  ChellesVolVoiture (autos garees : plus d'essais rates sur les trottoirs).
rem  Retour arriere : fichiers *.avant_perf a cote des fichiers modifies.
rem =====================================================================
setlocal
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "UE=C:\Program Files\Epic Games\UE_4.27"
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)
if not exist "%PROJET%\Tools\out" mkdir "%PROJET%\Tools\out"
set "PLUGINS=%PROJET%\Plugins"
set "ECHECS=0"
for %%P in (ChellesMenu ChellesRoue ChellesVolVoiture) do call :compiler %%P
echo.
if "%ECHECS%"=="0" (echo TERMINE.) else (echo %ECHECS% plugin^(s^) en echec : envoyez une photo de cette fenetre a Claude.)
pause
exit /b %ECHECS%

:compiler ChellesMenu

echo.
echo ===== ETAPE 2/2 : objets en double dans Rachid (3 a 10 minutes) =====
copy /y "%PROJET%\Tools\Terrain\doublons_rachid.py" "%TEMP%\chelles_doublons.py" >nul
"%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%PROJET%\FPS_Chelles.uproject" -run=pythonscript -script="%TEMP%\chelles_doublons.py" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nosplash -stdout > "%PROJET%\Tools\out\terrain_unreal.log" 2>&1
findstr /L /C:"[Terrain]" "%PROJET%\Tools\out\terrain_unreal.log"

echo.
if "%ECHECS%"=="0" (
    echo TERMINE. Ouvrez CHELLES 3D : MENU ^> OPTIONS ^> AVANCES.
) else (
    echo La compilation du menu a echoue : envoyez une photo de cette fenetre a Claude.
    echo Journal : Tools\out\terrain_ChellesMenu.log
)
pause
exit /b %ECHECS%

:compiler
set "NOM=%~1"
set "SORTIE=%TEMP%\Chelles_Terrain_%NOM%"
if exist "%SORTIE%" rmdir /s /q "%SORTIE%"
pushd "%UE%\Engine"
"%UE%\Engine\Binaries\DotNET\AutomationTool.exe" BuildPlugin -Plugin="%PLUGINS%\%NOM%\%NOM%.uplugin" -Package="%SORTIE%" -TargetPlatforms=Win64 -Rocket -VS2019 > "%PROJET%\Tools\out\terrain_%NOM%.log" 2>&1
set "RESULTAT=%ERRORLEVEL%"
popd
if not "%RESULTAT%"=="0" (
    echo ECHEC de %NOM%.
    findstr /i /c:": error" "%PROJET%\Tools\out\terrain_%NOM%.log"
    set /a ECHECS+=1
    goto :eof
)
xcopy /e /i /y "%SORTIE%\Binaries" "%PLUGINS%\%NOM%\Binaries" >nul
xcopy /e /i /y "%SORTIE%\Intermediate" "%PLUGINS%\%NOM%\Intermediate" >nul
rmdir /s /q "%SORTIE%"
echo %NOM% : OK
goto :eof
