@echo off
rem ------------------------------------------------------------------
rem  CHELLES 3D - Installe l'APPARTEMENT DU JOUEUR (touche F1)
rem  Compile le plugin Plugins\ChellesAppartement (1 a 5 minutes).
rem  Fermez Unreal avant de lancer ce fichier (double-clic).
rem ------------------------------------------------------------------
setlocal
set "PROJET=%~dp0..\.."
if not exist "%PROJET%\Plugins\ChellesAppartement\ChellesAppartement.uplugin" (
    echo Le dossier Plugins\ChellesAppartement est introuvable.
    pause
    exit /b 1
)
call "%PROJET%\Plugins\ChellesAppartement\Compiler_le_plugin.bat"
