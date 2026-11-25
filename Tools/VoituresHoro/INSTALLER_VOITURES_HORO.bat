@echo off
chcp 65001 >nul
title CHELLES 3D - Voitures garees DV_Car_Horo + Bugatti
rem =====================================================================
rem  VOITURES GAREES DV_CAR_HORO + BUGATTI (25/11/2026) - UN SEUL FICHIER A LANCER
rem  1. Fermez Unreal.   2. Double-cliquez ici.   3. Attendez TERMINE (5 a 15 minutes).
rem  a) recompile le plugin ChellesVolVoiture : autos garees = DV_Car_Horo + Bugatti,
rem     plaques CHELLES-3D, phares / saut / reinitialiser / clignotants en multijoueur
rem  b) importe les 2 Bugatti de Rachid (carrosserie + 4 roues) et cree BP_Bugatti_Chelles + BP_BugattiBrouillard_Chelles,
rem     ajoute les touches C (camera) et Inser (ordinateur de bord), et les dossiers du jeu exporte
rem  Retour arriere : Tools\Sauvegarde\RETOUR_EN_ARRIERE.bat (photo prise avant l'import)
rem  et les fichiers *.avant_horo (plugin et Config).
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
echo ===== ETAPE 1/2 : compilation du vol de voiture (1 a 5 minutes) =====
call :compiler ChellesVolVoiture

echo.
echo ===== ETAPE 2/2 : import des 2 Bugatti dans Unreal (3 a 15 minutes) =====
for %%V in (Bugatti BugattiBrouillard) do (
    if not exist "%PROJET%\Tools\VoituresHoro\%%V\SM_%%V_Carrosserie.fbx" if exist "%PROJET%\Tools\VoituresHoro\%%V\SM_%%V_Carrosserie.zip" (
        echo Decompression de la carrosserie %%V...
        powershell -NoProfile -Command "Expand-Archive -Force -LiteralPath '%PROJET%\Tools\VoituresHoro\%%V\SM_%%V_Carrosserie.zip' -DestinationPath '%PROJET%\Tools\VoituresHoro\%%V'"
    )
)
if exist "%PROJET%\Tools\VoituresHoro\etat.txt" del "%PROJET%\Tools\VoituresHoro\etat.txt"
copy /y "%PROJET%\Tools\VoituresHoro\installer_voitures_horo.py" "%TEMP%\chelles_voitures_horo.py" >nul
"%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%PROJET%\FPS_Chelles.uproject" -run=pythonscript -script="%TEMP%\chelles_voitures_horo.py" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nosplash -stdout > "%PROJET%\Tools\out\voitures_horo_unreal.log" 2>&1
findstr /L /C:"[Horo]" "%PROJET%\Tools\out\voitures_horo_unreal.log"
set "ETAT=pas de reponse"
if exist "%PROJET%\Tools\VoituresHoro\etat.txt" set /p ETAT=<"%PROJET%\Tools\VoituresHoro\etat.txt"

echo.
if "%ECHECS%"=="0" if "%ETAT%"=="OK" (
    echo TERMINE.
    echo Ouvrez CHELLES 3D, cliquez sur Play : des DV_Car_Horo et des Bugatti sont garees au bord des rues.
    echo   F  monter / descendre        H  klaxon            L  phares
    echo   R  saut americain            Retour arriere  remettre sur roues + reparer
    echo   G  clignotant gauche         U  clignotant droit  V  warnings
    echo   C  camera interieure / exterieure                 Inser  ordinateur de bord
    pause
    exit /b 0
)
echo Il y a eu un souci : envoyez une photo de cette fenetre a Claude.
echo Compilation : Tools\out\horo_ChellesVolVoiture.log
echo Import      : Tools\out\voitures_horo_unreal.log  (etat : %ETAT%)
pause
exit /b 1

:compiler
set "NOM=%~1"
set "SORTIE=%TEMP%\Chelles_Horo_%NOM%"
if exist "%SORTIE%" rmdir /s /q "%SORTIE%"
pushd "%UE%\Engine"
"%UE%\Engine\Binaries\DotNET\AutomationTool.exe" BuildPlugin -Plugin="%PLUGINS%\%NOM%\%NOM%.uplugin" -Package="%SORTIE%" -TargetPlatforms=Win64 -Rocket -VS2019 > "%PROJET%\Tools\out\horo_%NOM%.log" 2>&1
set "RESULTAT=%ERRORLEVEL%"
popd
if not "%RESULTAT%"=="0" (
    echo ECHEC de %NOM%.
    findstr /i /c:": error" "%PROJET%\Tools\out\horo_%NOM%.log"
    set /a ECHECS+=1
    goto :eof
)
xcopy /e /i /y "%SORTIE%\Binaries" "%PLUGINS%\%NOM%\Binaries" >nul
xcopy /e /i /y "%SORTIE%\Intermediate" "%PLUGINS%\%NOM%\Intermediate" >nul
rmdir /s /q "%SORTIE%"
echo %NOM% : OK
goto :eof
