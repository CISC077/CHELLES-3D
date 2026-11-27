@echo off
chcp 65001 >nul
title CHELLES 3D - TOUT INSTALLER (27/11/2026)
rem =====================================================================
rem  TOUT INSTALLER DU 27/11/2026 - UN SEUL FICHIER A LANCER
rem  1. Fermez Unreal (et le selecteur de projets Epic).
rem  2. Double-cliquez ici.   3. Attendez "TERMINE" (1 h a 1 h 30).
rem  Ce fichier fait, dans l'ordre :
rem    1) SAUVEGARDE de la carte Rachid (Content\Maps) dans
rem       _Sauvegardes\Avant_TOUT_INSTALLER_27-11_<heure>\
rem    2) PERSONNAGES : RACHID VIP (8466), animations de Mr.JONES,
rem       puis compile ChellesMenu (+ correction de l'erreur du menu
rem       HABILLAGE au choix du perso), ChellesWraith et ChellesFun
rem    3) COMMERCES : CADEAU DU JOUR + pieces x2 le week-end
rem    4) FLUIDITE : LOD automatiques des objets lourds sans LOD (dont
rem       les 30 Bugatti garees) - retour arriere : Tools\LOD\retirer_lod.py
rem       + plus de gel de 30 s pendant le chargement du catalogue HABILLAGE
rem    5) ROUTES : asphalte MI_AsphaltRoad + croisements a la meme
rem       hauteur + liste des fleurs (Unreal s'ouvre et se ferme seul)
rem    6) TEST AUTOMATIQUE du jeu (10 a 15 min) pour mesurer la fluidite
rem  Journal : TOUT_INSTALLER_27-11.log (a cote de ce fichier)
rem =====================================================================
setlocal
set "PROJET=%~dp0"
if "%PROJET:~-1%"=="\" set "PROJET=%PROJET:~0,-1%"
set "JOURNAL=%PROJET%\TOUT_INSTALLER_27-11.log"
set "UE=C:\Program Files\Epic Games\UE_4.27"

tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)
if not exist "%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" (
    echo Unreal 4.27 introuvable dans "%UE%".
    pause
    exit /b 1
)

echo TOUT INSTALLER 27/11 - debut %date% %time% > "%JOURNAL%"

rem ---------- 1) Sauvegarde de la carte ----------
for /f "tokens=1-2 delims=:" %%a in ("%time: =0%") do set "HEURE=%%ah%%b"
set "SAUVE=%PROJET%\_Sauvegardes\Avant_TOUT_INSTALLER_27-11_%HEURE%"
echo.
echo ===== 1/6 : sauvegarde de la carte Rachid =====
mkdir "%SAUVE%" 2>nul
copy /y "%PROJET%\Content\Maps\Rachid.umap" "%SAUVE%\" >nul
copy /y "%PROJET%\Content\Maps\Rachid_BuiltData.uasset" "%SAUVE%\" >nul
if exist "%SAUVE%\Rachid.umap" (
    echo Sauvegarde OK : %SAUVE%
    echo [1/6] Sauvegarde OK : %SAUVE% >> "%JOURNAL%"
) else (
    echo ECHEC de la sauvegarde de Rachid.umap : on s'arrete par prudence.
    echo [1/6] ECHEC sauvegarde - arret >> "%JOURNAL%"
    pause
    exit /b 1
)

rem ---------- 2) Personnages + ChellesMenu / Wraith / Fun ----------
echo.
echo ===== 2/6 : personnages (RACHID VIP, Mr.JONES) + menu HABILLAGE (15 a 40 min) =====
call "%PROJET%\Tools\Personnages\CREER_PERSONNAGES.bat" < nul
echo [2/6] Personnages + skins : code %errorlevel% (detail : Tools\Skins\RAPPORT_SKINS.txt) >> "%JOURNAL%"
call :verifier ChellesMenu
call :verifier ChellesWraith
call :verifier ChellesFun

rem ---------- 3) Commerces : cadeau du jour ----------
echo.
echo ===== 3/6 : commerces - cadeau du jour et week-end x2 (1 a 5 min) =====
call "%PROJET%\Plugins\ChellesCommerces\Compiler_le_plugin.bat" SANSPAUSE
echo [3/6] ChellesCommerces : code %errorlevel% >> "%JOURNAL%"
call :verifier ChellesCommerces

rem ---------- 4) Fluidite : LOD automatiques ----------
echo.
echo ===== 4/6 : fluidite - LOD des objets lourds (5 a 30 min) =====
copy /y "%PROJET%\Tools\LOD\ajouter_lod_auto.py" "%TEMP%\chelles_lod_auto.py" >nul
"%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%PROJET%\FPS_Chelles.uproject" -run=pythonscript -script="%TEMP%\chelles_lod_auto.py" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nosplash -stdout > "%PROJET%\Tools\out\lod_auto.log" 2>&1
findstr /L /C:"[LOD] TERMINE" /C:"[LOD] Sans LOD" /C:"[LOD] ERREUR" "%PROJET%\Tools\out\lod_auto.log"
findstr /L /C:"[LOD] TERMINE" "%PROJET%\Tools\out\lod_auto.log" >nul
if errorlevel 1 (
    echo [4/6] ECHEC LOD : voir Tools\out\lod_auto.log >> "%JOURNAL%"
) else (
    for /f "delims=" %%L in ('findstr /L /C:"[LOD] TERMINE" "%PROJET%\Tools\out\lod_auto.log"') do echo [4/6] %%L >> "%JOURNAL%"
)

rem ---------- 5) Routes ----------
echo.
echo ===== 5/6 : routes en asphalte + croisements (5 a 15 min, Unreal s'ouvre et se ferme seul) =====
call "%PROJET%\Tools\Routes\AMELIORER_ROUTES.bat" < nul
if exist "%PROJET%\Tools\Routes\ameliorer_routes.log" (
    echo [5/6] Routes : voir Tools\Routes\ameliorer_routes.log >> "%JOURNAL%"
) else (
    echo [5/6] Routes : PAS de journal ameliorer_routes.log - a verifier >> "%JOURNAL%"
)

rem ---------- 6) Test automatique ----------
echo.
echo ===== 6/6 : test automatique du jeu (10 a 15 min, une fenetre de jeu s'ouvre seule) =====
call "%PROJET%\Tools\Testeur\TESTER_LE_JEU.bat" < nul
if exist "%PROJET%\Tools\Testeur\Rapport\DERNIER_RAPPORT_TEST.txt" (
    findstr /C:"RESULTAT" /C:"Fluidite" "%PROJET%\Tools\Testeur\Rapport\DERNIER_RAPPORT_TEST.txt" >> "%JOURNAL%"
)

echo fin %date% %time% >> "%JOURNAL%"
echo.
echo =====================================================================
echo  TERMINE. Resume ci-dessous (aussi dans TOUT_INSTALLER_27-11.log).
echo  Si une ligne dit ECHEC, envoyez-la a Claude.
echo  Retour en arriere de la carte : copiez les 2 fichiers de
echo  %SAUVE%
echo  dans Content\Maps (Unreal ferme).
echo =====================================================================
type "%JOURNAL%"
pause
exit /b 0

:verifier
rem Le plugin est-il bien compile (DLL plus recente que les sources) ?
set "DLL=%PROJET%\Plugins\%1\Binaries\Win64\UE4Editor-%1.dll"
if not exist "%DLL%" (
    echo      ECHEC : %1 pas compile >> "%JOURNAL%"
    goto :eof
)
for /f %%R in ('powershell -NoProfile -Command "$d=(Get-Item '%DLL%').LastWriteTime; $s=(Get-ChildItem '%PROJET%\Plugins\%1\Source' -Recurse -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime; if ($d -ge $s) {'OK'} else {'ANCIEN'}"') do set "ETAT=%%R"
if "%ETAT%"=="OK" (
    echo      %1 : compile OK >> "%JOURNAL%"
) else (
    echo      ECHEC : %1 pas recompile ^(DLL plus ancienne que les sources^) >> "%JOURNAL%"
)
goto :eof
