@echo off
chcp 65001 >nul
title CHELLES 3D - Agent Terrain : reglages graphiques + doublons
rem =====================================================================
rem  AGENT TERRAIN (25/11/2026) - UN SEUL FICHIER A LANCER
rem  1. Fermez Unreal.   2. Double-cliquez ici.   3. Repondez O ou N.
rem  a) recompile le plugin ChellesMenu : onglet OPTIONS > AVANCES,
rem     compteur FPS, touches des modes CHELLES modifiables
rem  b) repere les objets en double dans Rachid et les retire
rem     (copie de Rachid faite avant dans _Sauvegardes\avant_doublons_...)
rem  Pour remettre Rachid comme avant : RESTAURER_DOUBLONS.bat
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
tasklist /fi "imagename eq UE4Editor-Cmd.exe" | find /i "UE4Editor-Cmd.exe" >nul
if not errorlevel 1 (
    echo Un autre outil CHELLES travaille deja sur le projet ^(UE4Editor-Cmd^).
    echo Attendez qu'il ait fini, puis relancez ce fichier.
    pause
    exit /b 1
)
if not exist "%PROJET%\Tools\out" mkdir "%PROJET%\Tools\out"
set "PLUGINS=%PROJET%\Plugins"
set "ECHECS=0"

echo.
echo Retirer les objets en double de la carte Rachid ?
echo   O = oui ^(une copie de Rachid est faite avant^)   N = juste les compter
set "REPONSE=O"
set /p "REPONSE=Votre choix [O/N] puis Entree : "
if /i "%REPONSE%"=="N" (set "CHELLES_DOUBLONS=SIMULATION") else (set "CHELLES_DOUBLONS=SUPPRIMER")

echo.
echo ===== ETAPE 1/2 : compilation du menu (1 a 5 minutes) =====
call :compiler ChellesMenu

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
