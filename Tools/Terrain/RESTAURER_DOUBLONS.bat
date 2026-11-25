@echo off
chcp 65001 >nul
title CHELLES 3D - Remettre Rachid comme avant le retrait des doublons
rem Remet la derniere copie _Sauvegardes\avant_doublons_<date> de Rachid (Unreal ferme).
setlocal
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)
set "DERNIERE="
for /f "delims=" %%D in ('dir /b /ad /o-n "%PROJET%\_Sauvegardes\avant_doublons_*" 2^>nul') do (
    set "DERNIERE=%%D"
    goto trouve
)
:trouve
if "%DERNIERE%"=="" (
    echo Aucune copie trouvee dans _Sauvegardes : rien a remettre.
    pause
    exit /b 1
)
echo Copie trouvee : %DERNIERE%
copy /y "%PROJET%\_Sauvegardes\%DERNIERE%\Rachid.umap" "%PROJET%\Content\Maps\Rachid.umap" >nul
if exist "%PROJET%\_Sauvegardes\%DERNIERE%\Rachid_BuiltData.uasset" copy /y "%PROJET%\_Sauvegardes\%DERNIERE%\Rachid_BuiltData.uasset" "%PROJET%\Content\Maps\Rachid_BuiltData.uasset" >nul
echo TERMINE : Rachid est revenue comme avant.
pause
