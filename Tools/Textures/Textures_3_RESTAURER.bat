@echo off
chcp 65001 >nul
title Restauration des textures
rem Restauration des textures - UNREAL DOIT ETRE FERME.
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
tasklist /FI "IMAGENAME eq UE4Editor.exe" | find /I "UE4Editor.exe" >nul
if not errorlevel 1 (
  echo Unreal est encore ouvert. Ferme-le puis relance ce fichier.
  pause
  exit /b 1
)
if not exist "%PROJET%\Tools\out" mkdir "%PROJET%\Tools\out"
echo Restauration des textures en cours... Patiente, cela peut prendre plusieurs minutes.
set "CHELLES_TEXTURES=restaurer"
"C:\Program Files\Epic Games\UE_4.27\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%PROJET%\FPS_Chelles.uproject" -run=pythonscript -script="%PROJET%\Tools\Textures\restaurer_textures.py" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nosplash -stdout > "%PROJET%\Tools\out\textures_unreal.log" 2>&1
echo.
echo ===== Resultat =====
findstr /L /C:"[Textures]" "%PROJET%\Tools\out\textures_unreal.log"
echo.
echo Journal complet : Tools\out\textures_unreal.log
pause
