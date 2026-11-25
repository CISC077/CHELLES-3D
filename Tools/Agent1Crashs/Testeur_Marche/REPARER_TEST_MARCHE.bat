@echo off
setlocal enabledelayedexpansion
title Reparer le test "Marcher tout droit" - CHELLES 3D
rem ------------------------------------------------------------------
rem  Agent 1 (25/11/2026). A lancer UNE fois (double-clic), Unreal FERME.
rem   1. recompile seulement le plugin ChellesTesteur (1 a 3 minutes) ;
rem   2. relance le test automatique du jeu et ouvre le rapport.
rem  Ne touche pas au jeu : seul le testeur change.
rem  Copie d'avant : Plugins\ChellesTesteur\Source\ChellesTesteur\Private\TesteurChelles.cpp.avant_marche
rem ------------------------------------------------------------------
cd /d "%~dp0"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"

set "UE=C:\Program Files\Epic Games\UE_4.27"
for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\4.27" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE=%%b"
if not exist "%UE%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo  Unreal 4.27 introuvable dans "%UE%". Dis-le a Claude.
    pause
    exit /b 1
)

:attendre_unreal
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo.
    echo  Unreal est ouvert : ENREGISTRE ton travail, ferme Unreal, puis appuie sur une touche.
    pause >nul
    goto attendre_unreal
)

set "NOM=ChellesTesteur"
set "PLUGIN=%PROJET%\Plugins\%NOM%"
set "SORTIE=%TEMP%\Chelles_Compile_%NOM%"
if exist "%SORTIE%" rmdir /s /q "%SORTIE%"
echo.
echo  ===== 1/2 : compilation du testeur (1 a 3 minutes, ne ferme pas cette fenetre) =====
call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN%\%NOM%.uplugin" -Package="%SORTIE%" -TargetPlatforms=Win64 -Rocket -VS2019 > "%PLUGIN%\Journal_compilation_complet.txt" 2>&1
set "RESULTAT=%errorlevel%"
if not "%RESULTAT%"=="0" (
    echo.
    echo  ECHEC de la compilation. Rien n'a change dans le jeu.
    findstr /i /c:": error" /c:": fatal error" "%PLUGIN%\Journal_compilation_complet.txt"
    echo  Dis-le a Claude, il lit Plugins\ChellesTesteur\Journal_compilation_complet.txt
    pause
    exit /b 1
)
xcopy /e /i /y "%SORTIE%\Binaries" "%PLUGIN%\Binaries" >nul
xcopy /e /i /y "%SORTIE%\Intermediate" "%PLUGIN%\Intermediate" >nul
rmdir /s /q "%SORTIE%"
echo  Testeur compile : OK

echo.
echo  ===== 2/2 : nouveau test du jeu =====
call "%~dp0TESTER_LE_JEU.bat"
exit /b 0
