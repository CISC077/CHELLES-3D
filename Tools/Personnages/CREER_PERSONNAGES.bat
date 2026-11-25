@echo off
title CHELLES 3D - Personnages jouables (LENA, SOFIA, LUCIA, BRENDA...)
rem =====================================================================
rem  PERSONNAGES JOUABLES (25/11/2026) - UN SEUL FICHIER A LANCER
rem  1. Fermez Unreal.   2. Double-cliquez ici.   3. Attendez "TERMINE".
rem  Ce fichier :
rem    a) importe les personnages du Bureau (ex. Bureau\BRENDA 3D) : corps avec
rem       squelette, couleurs, animations Mixamo (marche, course, vol, danses...)
rem       et les ajoute dans Plugins\ChellesWraith\Wraith.txt
rem    b) 2e passage : versions allegees (LOD) + menage des fichiers en double
rem    c) lance Tools\Skins\REPARER_SKINS.bat (compilation + verification)
rem  Les imports se font un par un, en vidant la memoire entre chaque.
rem =====================================================================
setlocal
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "UE=C:\Program Files\Epic Games\UE_4.27"
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
if not exist "%PROJET%\Tools\out" mkdir "%PROJET%\Tools\out"
set "CHELLES_SANS_FENETRE=1"
copy /y "%PROJET%\Tools\Personnages\creer_personnages.py" "%TEMP%\chelles_persos.py" >nul

for %%N in (1 2) do (
    echo.
    echo ===== PASSAGE %%N/2 : personnages ^(5 a 20 minutes^) =====
    "%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%PROJET%\FPS_Chelles.uproject" -run=pythonscript -script="%TEMP%\chelles_persos.py" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nosplash -stdout > "%PROJET%\Tools\out\personnages_%%N.log" 2>&1
    findstr /L /C:"[Personnages]" "%PROJET%\Tools\out\personnages_%%N.log"
)

echo.
echo ===== Compilation et verification des skins =====
call "%PROJET%\Tools\Skins\REPARER_SKINS.bat"
