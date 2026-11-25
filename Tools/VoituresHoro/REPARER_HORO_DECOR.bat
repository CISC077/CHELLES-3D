@echo off
chcp 65001 >nul
title CHELLES 3D - Horo de decor dans Rachid
rem =====================================================================
rem  HORO DE DECOR DANS RACHID (25/11/2026) - fermez Unreal puis double-cliquez ici
rem  Corrige l'avertissement "Accessed None ... Car_Hatchback_horo" (5 Horo rue Gambetta).
rem  Photo du projet prise avant (Tools\Sauvegarde\RETOUR_EN_ARRIERE.bat pour revenir).
rem =====================================================================
setlocal
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "UE=C:\Program Files\Epic Games\UE_4.27"
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)
if not exist "%PROJET%\Tools\out" mkdir "%PROJET%\Tools\out"
echo.
echo   1 = retirer seulement l'animation des 5 Horo de la rue Gambetta (conseille, rien ne change a l'ecran)
echo   2 = supprimer de Rachid TOUTES les Horo de decor (115), les voitures garees conduisibles restent
echo.
choice /c 12 /n /m "Votre choix (1 ou 2) : "
if errorlevel 2 (set "CHELLES_HORO_MODE=SUPPRIMER") else (set "CHELLES_HORO_MODE=ANIM")
if exist "%PROJET%\Tools\VoituresHoro\etat_decor.txt" del "%PROJET%\Tools\VoituresHoro\etat_decor.txt"
copy /y "%PROJET%\Tools\VoituresHoro\reparer_horo_decor.py" "%TEMP%\chelles_horo_decor.py" >nul
echo Travail en cours (2 a 5 minutes)...
"%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%PROJET%\FPS_Chelles.uproject" -run=pythonscript -script="%TEMP%\chelles_horo_decor.py" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nosplash -stdout > "%PROJET%\Tools\out\horo_decor.log" 2>&1
findstr /L /C:"[Horo]" "%PROJET%\Tools\out\horo_decor.log" | findstr /v /c:"supprime "
set "ETAT=pas de reponse"
if exist "%PROJET%\Tools\VoituresHoro\etat_decor.txt" set /p ETAT=<"%PROJET%\Tools\VoituresHoro\etat_decor.txt"
echo.
if "%ETAT%"=="OK" (echo TERMINE.) else (echo Il y a eu un souci : %ETAT%  - envoyez une photo de cette fenetre a Claude.)
pause
