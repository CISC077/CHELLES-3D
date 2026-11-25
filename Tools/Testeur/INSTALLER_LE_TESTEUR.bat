@echo off
setlocal enabledelayedexpansion
title Installer le Testeur automatique (Agent 5) - CHELLES 3D
rem ------------------------------------------------------------------
rem  A LANCER UNE SEULE FOIS (double-clic), Unreal FERME.
rem   1. compile le plugin ChellesTesteur et le menu ChellesMenu (5 a 15 minutes) ;
rem   2. verifie que la tache Windows du soir existe (sinon la cree a 23:00) :
rem      chaque soir elle verifie les fichiers PUIS teste le jeu ;
rem   3. fait un premier test tout de suite et ouvre le rapport.
rem  Rien n'est supprime. Copies d'avant : Plugins\ChellesMenu\...\*.avant_testeur
rem ------------------------------------------------------------------
cd /d "%~dp0"
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"

set "UE=C:\Program Files\Epic Games\UE_4.27"
for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\4.27" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE=%%b"
if not exist "%UE%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo  Unreal 4.27 introuvable dans "%UE%".
    set /p "UE=  Colle le dossier d'installation d'Unreal 4.27 (ex: D:\Epic Games\UE_4.27) puis Entree : "
)
if not exist "%UE%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo  Toujours introuvable. Abandon.
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

echo.
echo  ===== 1/3 : compilation (5 a 15 minutes, ne ferme pas cette fenetre) =====
set "ECHECS=0"
call :compiler ChellesTesteur
call :compiler ChellesMenu
if not "%ECHECS%"=="0" (
    echo.
    echo  ECHEC de la compilation. Envoie a Claude le fichier Journal_compilation.txt
    echo  du plugin en echec (dans Plugins\ChellesTesteur ou Plugins\ChellesMenu).
    pause
    exit /b 1
)

echo.
echo  ===== 2/3 : tache du soir =====
schtasks /Query /TN "CHELLES 3D - Verification erreurs" >nul 2>nul
if errorlevel 1 (
    echo  La tache du soir n'existe pas : je la cree pour 23:00.
    powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJET%\Tools\VerifErreurs\installer_tache.ps1" -Heure "23:00"
) else (
    echo  OK : la tache "CHELLES 3D - Verification erreurs" existe deja, elle lancera aussi le test du jeu.
)

echo.
echo  ===== 3/3 : premier test du jeu =====
call "%~dp0TESTER_LE_JEU.bat"
exit /b 0

:compiler
set "NOM=%~1"
set "PLUGIN=%PROJET%\Plugins\%NOM%"
set "SORTIE=%TEMP%\Chelles_Compile_%NOM%"
if exist "%SORTIE%" rmdir /s /q "%SORTIE%"
echo.
echo  --- %NOM% ...
call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN%\%NOM%.uplugin" -Package="%SORTIE%" -TargetPlatforms=Win64 -Rocket -VS2019 > "%PLUGIN%\Journal_compilation_complet.txt" 2>&1
set "RESULTAT=%errorlevel%"
for /f "delims=" %%F in ('dir /b /s /o-d "%APPDATA%\Unreal Engine\AutomationTool\Logs\UBT-UE4*-Win64-*.txt" 2^>nul') do (
    copy /y "%%F" "%PLUGIN%\Journal_compilation.txt" >nul
    goto :journal_copie
)
:journal_copie
if not "%RESULTAT%"=="0" (
    echo  %NOM% : ECHEC
    findstr /i /c:": error" /c:": fatal error" "%PLUGIN%\Journal_compilation_complet.txt"
    set /a ECHECS+=1
    goto :eof
)
xcopy /e /i /y "%SORTIE%\Binaries" "%PLUGIN%\Binaries" >nul
xcopy /e /i /y "%SORTIE%\Intermediate" "%PLUGIN%\Intermediate" >nul
rmdir /s /q "%SORTIE%"
echo  %NOM% : OK
goto :eof
