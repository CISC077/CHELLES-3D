@echo off
chcp 65001 >nul
title CHELLES 3D - Vegetation, riviere et lac (OpenStreetMap)
rem =====================================================================
rem  VEGETATION + RIVIERE + LAC - UN SEUL FICHIER A LANCER
rem  1. Fermez Unreal.   2. Double-cliquez ici.   3. Attendez "TERMINE".
rem  - compile le petit plugin ChellesVegetation (1 a 5 minutes, une fois)
rem  - pose arbres, buissons, haies, riviere et lac dans la carte Rachid
rem    (copie de la carte avant : Tools\VegetationEau\Sauvegardes\)
rem  Pour tout enlever : ANNULER_VEGETATION_EAU.bat
rem =====================================================================
setlocal
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "PLUGIN=%PROJET%\Plugins\ChellesVegetation"
set "ICI=%PROJET%\Tools\VegetationEau"

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
if not exist "%PLUGIN%\ChellesVegetation.uplugin" (
    echo Le dossier Plugins\ChellesVegetation est introuvable.
    pause
    exit /b 1
)

echo.
echo ===== ETAPE 1/2 : compilation du plugin ChellesVegetation (1 a 5 minutes) =====
set "SORTIE=%TEMP%\Chelles_Vegetation_Compile"
if exist "%SORTIE%" rmdir /s /q "%SORTIE%"
pushd "%UE%\Engine"
"%UE%\Engine\Binaries\DotNET\AutomationTool.exe" BuildPlugin -Plugin="%PLUGIN%\ChellesVegetation.uplugin" -Package="%SORTIE%" -TargetPlatforms=Win64 -Rocket -VS2019 > "%ICI%\compilation.log" 2>&1
set "RESULTAT=%ERRORLEVEL%"
popd
if not "%RESULTAT%"=="0" (
    echo.
    findstr /i /c:": error" /c:": fatal error" "%ICI%\compilation.log"
    echo.
    echo ECHEC de la compilation. Envoyez Tools\VegetationEau\compilation.log a Claude.
    echo La carte n'a pas ete modifiee.
    pause
    exit /b 1
)
xcopy /e /i /y "%SORTIE%\Binaries" "%PLUGIN%\Binaries" >nul
xcopy /e /i /y "%SORTIE%\Intermediate" "%PLUGIN%\Intermediate" >nul
rmdir /s /q "%SORTIE%"
echo Plugin compile.

echo.
echo ===== ETAPE 2/2 : vegetation, riviere et lac dans Rachid (5 a 15 minutes) =====
echo (telechargement OpenStreetMap, la carte est regardee vue du ciel, puis pose des plantes)
rem Unreal lit mal un chemin avec des espaces : copie du script dans un dossier sans espace
copy /y "%ICI%\vegetation_eau.py" "%TEMP%\chelles_vegetation_eau.py" >nul
"%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%PROJET%\FPS_Chelles.uproject" -run=pythonscript -script="%TEMP%\chelles_vegetation_eau.py" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nosplash -stdout > "%ICI%\vegetation_eau_unreal.log" 2>&1

echo.
echo ===== Resultat =====
type "%ICI%\vegetation_eau.log"
echo.
echo Journal : Tools\VegetationEau\vegetation_eau.log  -  vue du ciel : Tools\VegetationEau\carte_controle.bmp
pause
