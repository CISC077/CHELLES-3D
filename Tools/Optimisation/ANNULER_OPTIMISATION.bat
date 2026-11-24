@echo off
chcp 65001 >nul
title CHELLES 3D - Annuler l'optimisation finale
rem Remet tout comme avant le 24/11/2026 (UNREAL FERME).
setlocal
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord.
    pause
    exit /b 1
)
copy /y "%PROJET%\Config\DefaultEngine.ini.avant_optim_2026-11-24" "%PROJET%\Config\DefaultEngine.ini"
copy /y "%PROJET%\Saved\Config\Windows\EditorPerProjectUserSettings.ini.avant_optim_2026-11-24" "%PROJET%\Saved\Config\Windows\EditorPerProjectUserSettings.ini"
copy /y "%PROJET%\Plugins\ChellesInventaire\Source\ChellesInventaire\Private\InventaireCyber.cpp.avant_optim" "%PROJET%\Plugins\ChellesInventaire\Source\ChellesInventaire\Private\InventaireCyber.cpp"
copy /y "%PROJET%\Plugins\ChellesTourelle\Source\ChellesTourelle\Private\TourelleCadeau.cpp.avant_optim" "%PROJET%\Plugins\ChellesTourelle\Source\ChellesTourelle\Private\TourelleCadeau.cpp"
"C:\Program Files\Epic Games\UE_4.27\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" "%PROJET%\Tools\Optimisation\remettre_assets.py"
echo.
echo Fait. Pour les 2 plugins, relancez leur Compiler_le_plugin.bat.
pause
