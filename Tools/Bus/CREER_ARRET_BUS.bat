@echo off
rem ------------------------------------------------------------------
rem  BUS PENDANT L'INVASION ZOMBIE - CHELLES 3D (double-clic, Unreal FERME)
rem  (plugin deja compile) cree seulement le Blueprint d'arret de bus /Game/Chelles/Bus/BP_Arret_Bus
rem  La carte Rachid n'est pas modifiee : posez vous-meme les arrets.
rem  Journal : Tools\Bus\creer_arret_bus.log
rem ------------------------------------------------------------------
setlocal
title Installation du bus de l'invasion zombie

set "UE=C:\Program Files\Epic Games\UE_4.27"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "PLUGIN=%PROJET%\Plugins\ChellesBus"

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
echo ===== Creation de BP_Arret_Bus (1 a 3 minutes)...
rem Le chemin du projet contient un espace ("CHELLES 3D") : Unreal lirait mal -script=, on copie le script dans un dossier sans espace
set "SCRIPT=%TEMP%\ChellesBus_creer_arret_bus.py"
copy /y "%PROJET%\Tools\Bus\creer_arret_bus.py" "%SCRIPT%" >nul
"%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%PROJET%\FPS_Chelles.uproject" -run=pythonscript -script="%SCRIPT%" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nosplash -stdout > "%PROJET%\Tools\Bus\creer_arret_bus_unreal.log" 2>&1

echo.
echo ===== Resultat =====
type "%PROJET%\Tools\Bus\creer_arret_bus.log"
echo.
echo Ouvrez Unreal, puis glissez Content\Chelles\Bus\BP_Arret_Bus dans la carte Rachid, au bord des routes.
pause
