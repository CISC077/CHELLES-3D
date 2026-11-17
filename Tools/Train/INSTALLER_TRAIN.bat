@echo off
rem ------------------------------------------------------------------
rem  LIGNE DE TRAIN - CHELLES 3D (double-clic, Unreal FERME)
rem   Compile le plugin Plugins\ChellesTrain (1 a 5 minutes).
rem  Rien a poser dans la carte : le train et ses arrets arrivent tout seuls
rem  au lancement du jeu. Les arrets se changent dans Plugins\ChellesTrain\Train.txt
rem ------------------------------------------------------------------
setlocal
title Installation de la ligne de train

set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "PLUGIN=%PROJET%\Plugins\ChellesTrain"

if not exist "%PLUGIN%\Compiler_le_plugin.bat" (
    echo Le plugin est introuvable : %PLUGIN%
    pause
    exit /b 1
)
if not exist "%PROJET%\Content\Intercity_Train\Meshes\SM_PowerCar.uasset" (
    echo ATTENTION : le pack Intercity_Train n'est pas dans Content : le train sera invisible.
    echo.
)
call "%PLUGIN%\Compiler_le_plugin.bat"
