@echo off
chcp 65001 >nul
title CHELLES 3D - Supprimer toutes les cartes sauf Rachid
rem =====================================================================
rem  SUPPRIMER LES MAPS (25/11/2026, agent Terrain) - Unreal FERME.
rem  1. Unreal fait la liste des cartes .umap a supprimer : tout sauf Rachid
rem     et ce dont Rachid a besoin (sous-niveaux, Config, plugins).
rem  2. La liste s'ouvre dans le Bloc-notes : verifiez-la.
rem  3. O = les cartes partent dans _Sauvegardes\avant_maps_<date> (copie de
rem     secours) et quittent le projet.   N = rien n'est touche.
rem  Remettre les cartes : RESTAURER_MAPS.bat
rem =====================================================================
setlocal
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "PY=%UE%\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
if not exist "%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" (
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
    echo Un autre outil CHELLES travaille deja sur le projet ^(UE4Editor-Cmd^). Attendez qu'il ait fini.
    pause
    exit /b 1
)
set "UPROJECT="
for %%F in ("%PROJET%\*.uproject") do set "UPROJECT=%%~fF"
if not defined UPROJECT (
    echo Fichier .uproject introuvable dans "%PROJET%".
    pause
    exit /b 1
)
if not exist "%PROJET%\Tools\out" mkdir "%PROJET%\Tools\out"
set "CHELLES_PROJET=%PROJET%"

echo.
echo ===== ETAPE 1/2 : liste des cartes (5 a 15 minutes, ne fermez pas) =====
set "CHELLES_MAPS=LISTE"
copy /y "%PROJET%\Tools\Maps\supprimer_maps.py" "%TEMP%\chelles_maps.py" >nul
"%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%UPROJECT%" -run=pythonscript -script="%TEMP%\chelles_maps.py" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nosplash -stdout > "%PROJET%\Tools\out\maps_unreal.log" 2>&1
findstr /L /C:"[Maps]" "%PROJET%\Tools\out\maps_unreal.log"
if not exist "%PROJET%\Tools\Maps\Rapport\maps_a_supprimer.json" (
    echo.
    echo La liste n'a pas pu etre faite : rien n'est supprime.
    echo Envoyez Tools\out\maps_unreal.log a Claude.
    pause
    exit /b 1
)
start "" notepad "%PROJET%\Tools\Maps\Rapport\maps_a_supprimer.txt"

echo.
echo ===== ETAPE 2/2 =====
echo Supprimer les cartes de la liste ^(une copie de secours est gardee^) ?
set "REPONSE=N"
set /p "REPONSE=O = supprimer, N = ne rien toucher, puis Entree : "
if /i not "%REPONSE%"=="O" (
    echo Rien n'a ete supprime.
    pause
    exit /b 0
)
set "CHELLES_MAPS=SUPPRIMER"
if exist "%PY%" (
    "%PY%" "%PROJET%\Tools\Maps\supprimer_maps.py"
) else (
    python "%PROJET%\Tools\Maps\supprimer_maps.py"
)
if errorlevel 1 (
    echo Il y a eu des erreurs : envoyez une photo de cette fenetre a Claude.
) else (
    echo TERMINE. Ouvrez Unreal : la carte Rachid doit s'ouvrir comme avant.
    echo Si tout va bien, vous pourrez vider _Sauvegardes\avant_maps_... pour gagner la place.
)
pause
exit /b 0
