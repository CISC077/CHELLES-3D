@echo off
rem ------------------------------------------------------------------
rem  CHELLES 3D - Installe le plugin ChellesEnnemis (double-clic).
rem   1. ajoute les dossiers des ennemis a la liste d'export (Config\DefaultGame.ini, copie faite avant)
rem   2. compile le plugin Plugins\ChellesEnnemis (1 a 5 minutes)
rem  FERMEZ UNREAL avant de lancer ce fichier.
rem ------------------------------------------------------------------
setlocal
title Installation des ennemis Chelles

set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"

if not exist "%PROJET%\Plugins\ChellesEnnemis\ChellesEnnemis.uplugin" (
    echo Le plugin Plugins\ChellesEnnemis est introuvable dans "%PROJET%".
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
echo ===== 1/2 Liste d'export (DefaultGame.ini)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0ajouter_cuisson.ps1" -Projet "%PROJET%"

echo.
echo ===== 2/2 Compilation du plugin ChellesEnnemis
call "%PROJET%\Plugins\ChellesEnnemis\Compiler_le_plugin.bat"
