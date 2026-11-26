@echo off
chcp 65001 >nul
title CHELLES 3D - Routes en MI_AsphaltRoad, meme hauteur aux croisements + liste des fleurs
rem =====================================================================
rem  ROUTES + FLEURS (26/11/2026, agent Terrain) - Unreal FERME, double-clic.
rem  Unreal s'ouvre puis se ferme tout seul (5 a 15 minutes).
rem  - copie de Rachid avant : Tools\Routes\Sauvegardes\asphalte_<date>\
rem  - croisements et ronds-points : routes et plaques a la meme hauteur
rem  - routes, ronds-points et croisements en MI_AsphaltRoad
rem  - LISTE des fleurs (rien n'est supprime) : Tools\Routes\fleurs_a_supprimer.txt
rem =====================================================================
setlocal
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "ICI=%PROJET%\Tools\Routes"
if not exist "%UE%\Engine\Binaries\Win64\UE4Editor.exe" (
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
echo Unreal s'ouvre et travaille tout seul, puis se ferme. Ne touchez a rien...
copy /y "%ICI%\ameliorer_routes.py" "%TEMP%\chelles_ameliorer_routes.py" >nul
set "CHELLES_QUITTER=1"
start "" /wait "%UE%\Engine\Binaries\Win64\UE4Editor.exe" "%PROJET%\FPS_Chelles.uproject" -ExecutePythonScript="%TEMP%\chelles_ameliorer_routes.py" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -nosplash -log="ameliorer_routes_unreal.log"
echo.
echo ===== Resultat =====
type "%ICI%\ameliorer_routes.log"
if exist "%ICI%\fleurs_a_supprimer.txt" start "" notepad "%ICI%\fleurs_a_supprimer.txt"
echo.
echo Journal : Tools\Routes\ameliorer_routes.log   -   fleurs : Tools\Routes\fleurs_a_supprimer.txt
pause
