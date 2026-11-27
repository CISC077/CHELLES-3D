@echo off
chcp 65001 >nul
title CHELLES 3D - TOUT INSTALLER (27/11/2026)
rem =====================================================================
rem  TOUT INSTALLER DU 27/11/2026 - UN SEUL FICHIER A LANCER
rem  1. Fermez Unreal (et le selecteur de projets Epic).
rem  2. Double-cliquez ici.   3. Attendez "TERMINE" (30 a 60 minutes).
rem  Ce fichier fait, dans l'ordre :
rem    1) SAUVEGARDE de la carte Rachid (Content\Maps) dans
rem       _Sauvegardes\Avant_TOUT_INSTALLER_27-11_<heure>\
rem    2) PERSONNAGES : RACHID VIP (8466), animations de Mr.JONES,
rem       puis compile ChellesMenu (+ correction de l'erreur du menu
rem       HABILLAGE au choix du perso), ChellesWraith et ChellesFun
rem    3) COMMERCES : CADEAU DU JOUR + pieces x2 le week-end
rem    4) ROUTES : asphalte MI_AsphaltRoad + croisements a la meme
rem       hauteur + liste des fleurs (Unreal s'ouvre et se ferme seul)
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
echo ===== 1/4 : sauvegarde de la carte Rachid =====
mkdir "%SAUVE%" 2>nul
copy /y "%PROJET%\Content\Maps\Rachid.umap" "%SAUVE%\" >nul
copy /y "%PROJET%\Content\Maps\Rachid_BuiltData.uasset" "%SAUVE%\" >nul
if exist "%SAUVE%\Rachid.umap" (
    echo Sauvegarde OK : %SAUVE%
    echo [1/4] Sauvegarde OK : %SAUVE% >> "%JOURNAL%"
) else (
    echo ECHEC de la sauvegarde de Rachid.umap : on s'arrete par prudence.
    echo [1/4] ECHEC sauvegarde - arret >> "%JOURNAL%"
    pause
    exit /b 1
)

rem ---------- 2) Personnages + ChellesMenu / Wraith / Fun ----------
echo.
echo ===== 2/4 : personnages (RACHID VIP, Mr.JONES) + menu HABILLAGE (15 a 40 min) =====
call "%PROJET%\Tools\Personnages\CREER_PERSONNAGES.bat" < nul
echo [2/4] Personnages + skins : code %errorlevel% (detail : Tools\Skins\RAPPORT_SKINS.txt) >> "%JOURNAL%"
call :verifier ChellesMenu
call :verifier ChellesWraith
call :verifier ChellesFun

rem ---------- 3) Commerces : cadeau du jour ----------
echo.
echo ===== 3/4 : commerces - cadeau du jour et week-end x2 (1 a 5 min) =====
call "%PROJET%\Plugins\ChellesCommerces\Compiler_le_plugin.bat" SANSPAUSE
echo [3/4] ChellesCommerces : code %errorlevel% >> "%JOURNAL%"
call :verifier ChellesCommerces

rem ---------- 4) Routes ----------
echo.
echo ===== 4/4 : routes en asphalte + croisements (5 a 15 min, Unreal s'ouvre et se ferme seul) =====
call "%PROJET%\Tools\Routes\AMELIORER_ROUTES.bat" < nul
if exist "%PROJET%\Tools\Routes\ameliorer_routes.log" (
    echo [4/4] Routes : voir Tools\Routes\ameliorer_routes.log >> "%JOURNAL%"
) else (
    echo [4/4] Routes : PAS de journal ameliorer_routes.log - a verifier >> "%JOURNAL%"
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
