@echo off
rem ------------------------------------------------------------------
rem  HABILLAGE DES BATIMENTS - 1. ANALYSE - CHELLES 3D (double-clic, Unreal FERME)
rem  Ouvre Unreal sur Rachid, lance analyser_habillage.py, puis se referme tout seul.
rem  Journal : Tools\HabillageBatiments\habillage.log
rem ------------------------------------------------------------------
setlocal
title HABILLAGE DES BATIMENTS - 1. ANALYSE
set "SCRIPT=analyser_habillage.py"
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo.
    echo  Unreal est deja ouvert. Ferme TOUS les Unreal puis relance ce fichier,
    echo  ou bien dans Unreal : File ^> Execute Python Script ^> Tools\HabillageBatiments\%SCRIPT%
    echo.
    pause
    exit /b 1
)
echo.
echo  Recherche des maisons et immeubles des packs qui ont le meme volume. RIEN n'est modifie.
echo  Unreal s'ouvre, travaille puis se referme tout seul. Ne touche a rien pendant ce temps.
echo.
set "RELAIS=%TEMP%\chelles_habillage_relais.py"
> "%RELAIS%" echo exec(open(r"%~dp0%SCRIPT%", encoding="utf-8").read())
set "CHELLES_QUITTER=1"
"%UE%\Engine\Binaries\Win64\UE4Editor.exe" "%PROJET%\FPS_Chelles.uproject" /Game/Maps/Rachid -ExecutePythonScript="%RELAIS%"
echo.
type "%~dp0habillage.log"
echo.
echo  Ouvre maintenant Tools\HabillageBatiments\Rapport\rapport_habillage.txt
if exist "%~dp0Rapport\rapport_habillage.txt" start "" notepad "%~dp0Rapport\rapport_habillage.txt"
pause
