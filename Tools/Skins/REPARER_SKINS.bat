@echo off
title CHELLES 3D - Skins et habillage
rem =====================================================================
rem  SKINS ET HABILLAGE (25/11/2026) - UN SEUL FICHIER A LANCER
rem  1. Fermez Unreal.   2. Double-cliquez ici.   3. Attendez "TERMINE".
rem  Ce fichier :
rem    a) recompile ChellesWraith (touche K, LENA/SOFIA/LUCIA, heros Paragon)
rem       et ChellesMenu (menu HABILLAGE)
rem    b) ajoute les heros PARAGON de Content (Greystone, Zinx, Terra, Iggy, Yin,
rem       Wukong, Kwang, Shinbi, Phase, Countess...) avec leurs skins et leurs
rem       danses (touche J + apercu du menu) dans Plugins\ChellesWraith\Wraith.txt
rem    c) verifie tous les skins (corps, squelettes, materiaux, animations)
rem       et repare ce qui peut l'etre (materiaux, LOD, doublons de LUCIA)
rem    d) ouvre le rapport RAPPORT_SKINS.txt
rem  Pour revenir en arriere : ANNULER_SKINS.bat
rem =====================================================================
setlocal
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "UE=C:\Program Files\Epic Games\UE_4.27"
if not exist "%UE%\Engine\Binaries\DotNET\AutomationTool.exe" (
    echo Unreal 4.27 introuvable dans "%UE%".
    pause
    exit /b 1
)
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)
if not exist "%PROJET%\Tools\out" mkdir "%PROJET%\Tools\out"
set "PLUGINS=%PROJET%\Plugins"
set "ECHECS=0"

echo.
echo ===== ETAPE 1/2 : compilation des plugins (2 a 10 minutes) =====
for %%P in (ChellesWraith ChellesMenu) do (
    if exist "%PLUGINS%\%%P\%%P.uplugin" call :compiler %%P
)

echo.
echo ===== ETAPE 2/2 : heros Paragon + verification des skins (5 a 30 minutes) =====
rem Unreal lit mal un chemin avec des espaces : copie du script dans un dossier sans espace
copy /y "%PROJET%\Tools\Skins\verifier_skins.py" "%TEMP%\chelles_skins.py" >nul
"%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%PROJET%\FPS_Chelles.uproject" -run=pythonscript -script="%TEMP%\chelles_skins.py" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nosplash -stdout > "%PROJET%\Tools\out\skins_unreal.log" 2>&1
findstr /L /C:"[Skins]" "%PROJET%\Tools\out\skins_unreal.log"

echo.
if "%ECHECS%"=="0" (
    echo TERMINE. Vous pouvez ouvrir CHELLES 3D.
) else (
    echo %ECHECS% plugin^(s^) en echec : envoyez une photo de cette fenetre a Claude.
    echo Le jeu marche quand meme avec l'ancienne version du plugin.
)
if exist "%PROJET%\Tools\Skins\RAPPORT_SKINS.txt" start "" notepad "%PROJET%\Tools\Skins\RAPPORT_SKINS.txt"
pause
exit /b %ECHECS%

:compiler
set "NOM=%~1"
set "SORTIE=%TEMP%\Chelles_Skins_%NOM%"
if exist "%SORTIE%" rmdir /s /q "%SORTIE%"
echo.
echo ----- %NOM% : compilation (1 a 5 minutes)...
pushd "%UE%\Engine"
"%UE%\Engine\Binaries\DotNET\AutomationTool.exe" BuildPlugin -Plugin="%PLUGINS%\%NOM%\%NOM%.uplugin" -Package="%SORTIE%" -TargetPlatforms=Win64 -Rocket -VS2019 > "%PROJET%\Tools\out\skins_%NOM%.log" 2>&1
set "RESULTAT=%ERRORLEVEL%"
popd
if not "%RESULTAT%"=="0" (
    echo ECHEC de %NOM%. Journal : Tools\out\skins_%NOM%.log
    findstr /i /c:": error" "%PROJET%\Tools\out\skins_%NOM%.log"
    set /a ECHECS+=1
    goto :eof
)
xcopy /e /i /y "%SORTIE%\Binaries" "%PLUGINS%\%NOM%\Binaries" >nul
xcopy /e /i /y "%SORTIE%\Intermediate" "%PLUGINS%\%NOM%\Intermediate" >nul
rmdir /s /q "%SORTIE%"
echo %NOM% : OK
goto :eof
