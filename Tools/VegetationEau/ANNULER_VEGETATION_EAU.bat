@echo off
chcp 65001 >nul
title CHELLES 3D - Annuler vegetation, riviere et lac
rem Enleve tout ce qu'a pose INSTALLER_VEGETATION_EAU.bat (Unreal ferme, double-clic).
setlocal
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "ICI=%PROJET%\Tools\VegetationEau"
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)
copy /y "%ICI%\annuler_vegetation_eau.py" "%TEMP%\chelles_annuler_vegetation_eau.py" >nul
"%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%PROJET%\FPS_Chelles.uproject" -run=pythonscript -script="%TEMP%\chelles_annuler_vegetation_eau.py" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nosplash -stdout > "%ICI%\annuler_unreal.log" 2>&1
type "%ICI%\annuler_vegetation_eau.log"
pause
