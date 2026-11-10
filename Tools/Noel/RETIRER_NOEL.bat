@echo off
rem ------------------------------------------------------------------
rem  Retire la neige et le sol de Noel de la carte Rachid (Unreal FERME).
rem  Le Landscape retrouve son materiau d'origine.
rem ------------------------------------------------------------------
setlocal
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)
echo Retrait en cours, patientez quelques minutes...
"%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%PROJET%\FPS_Chelles.uproject" -run=pythonscript -script="%PROJET%\Tools\Noel\retirer_noel.py" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nosplash -stdout > "%PROJET%\Tools\Noel\retirer_noel_unreal.log" 2>&1
echo.
type "%PROJET%\Tools\Noel\retirer_noel.log"
pause
