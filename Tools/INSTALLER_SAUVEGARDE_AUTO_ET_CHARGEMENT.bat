@echo off
title CHELLES 3D - Sauvegarde auto + Ecran de chargement
rem =====================================================================
rem  SAUVEGARDE AUTOMATIQUE + ECRAN DE CHARGEMENT (25/11/2026)
rem  UN SEUL FICHIER A LANCER :
rem    1. Fermez Unreal.   2. Double-cliquez ici.   3. Attendez "TERMINE".
rem
rem  Ce fichier :
rem    a) branche la sauvegarde automatique sur tous les scripts de Tools
rem       (une "photo" du projet est prise avant chaque script) ;
rem    b) compile le nouveau plugin ChellesChargement (ecran de chargement) ;
rem    c) prend une premiere photo pour verifier que tout marche.
rem
rem  Retour en arriere : Tools\Sauvegarde\RETOUR_EN_ARRIERE.bat
rem  Pour debrancher : Tools\Sauvegarde\sauvegarde_auto.ini  (Actif=0)
rem =====================================================================
setlocal
for %%I in ("%~dp0..") do set "PROJET=%%~fI"
set "UE=C:\Program Files\Epic Games\UE_4.27"
if not exist "%UE%\Engine\Binaries\DotNET\AutomationTool.exe" (
    echo Unreal 4.27 introuvable dans "%UE%".
    set /p "UE=Collez le dossier d'installation d'Unreal 4.27 (ex: D:\Epic Games\UE_4.27) puis Entree : "
)
if not exist "%UE%\Engine\Binaries\DotNET\AutomationTool.exe" (
    echo Toujours introuvable. Abandon.
    pause
    exit /b 1
)
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)
set "PY=%UE%\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
if not exist "%PY%" (
    for /f "delims=" %%P in ('where python 2^>nul') do if not defined PY2 set "PY2=%%P"
)
if not exist "%PY%" set "PY=%PY2%"
if not defined PY (
    echo Python introuvable ^(il est normalement livre avec Unreal 4.27^). Abandon.
    pause
    exit /b 1
)
if not exist "%PROJET%\Tools\out" mkdir "%PROJET%\Tools\out"
set "ECHECS=0"

echo.
echo ===== ETAPE 1/3 : branchement de la sauvegarde automatique sur les scripts =====
"%PY%" "%PROJET%\Tools\Sauvegarde\brancher_sauvegarde_auto.py" > "%PROJET%\Tools\out\branchement_sauvegarde.log" 2>&1
if errorlevel 1 (
    echo [ATTENTION] probleme pendant le branchement : voir Tools\out\branchement_sauvegarde.log
    set /a ECHECS+=1
)
findstr /b /c:"branche :" /c:"Rien a faire" /c:"ERREUR" "%PROJET%\Tools\out\branchement_sauvegarde.log"

echo.
echo ===== ETAPE 2/3 : compilation de l'ecran de chargement (1 a 5 minutes) =====
set "SORTIE=%TEMP%\Chelles_Compile_ChellesChargement"
if exist "%SORTIE%" rmdir /s /q "%SORTIE%"
pushd "%UE%\Engine"
"%UE%\Engine\Binaries\DotNET\AutomationTool.exe" BuildPlugin -Plugin="%PROJET%\Plugins\ChellesChargement\ChellesChargement.uplugin" -Package="%SORTIE%" -TargetPlatforms=Win64 -Rocket -VS2019 > "%PROJET%\Tools\out\compil_ChellesChargement.log" 2>&1
set "RESULTAT=%ERRORLEVEL%"
popd
if not "%RESULTAT%"=="0" (
    echo ECHEC de la compilation. Journal : Tools\out\compil_ChellesChargement.log
    findstr /i /c:": error" /c:": fatal error" "%PROJET%\Tools\out\compil_ChellesChargement.log"
    set /a ECHECS+=1
) else (
    xcopy /e /i /y "%SORTIE%\Binaries" "%PROJET%\Plugins\ChellesChargement\Binaries" >nul
    xcopy /e /i /y "%SORTIE%\Intermediate" "%PROJET%\Plugins\ChellesChargement\Intermediate" >nul
    rmdir /s /q "%SORTIE%"
    echo ChellesChargement : OK
)

echo.
echo ===== ETAPE 3/3 : premiere photo du projet (1 a 3 minutes) =====
"%PY%" "%PROJET%\Tools\Sauvegarde\sauvegarde_auto.py" --maintenant installation
if errorlevel 1 (
    echo [ATTENTION] la premiere photo n'a pas pu etre prise.
    set /a ECHECS+=1
)

echo.
if "%ECHECS%"=="0" (
    echo TERMINE. Tout est en place, vous pouvez ouvrir CHELLES 3D.
    echo  - Avant chaque script de Tools, une photo du projet est prise toute seule.
    echo  - En cas de souci : Tools\Sauvegarde\RETOUR_EN_ARRIERE.bat
    echo  - L'ecran de chargement s'affiche au bouton Jouer et dans le jeu final.
) else (
    echo %ECHECS% etape^(s^) en echec : envoyez une photo de cette fenetre a Claude.
)
pause
exit /b %ECHECS%
