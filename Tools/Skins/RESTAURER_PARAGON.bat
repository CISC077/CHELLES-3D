@echo off
title CHELLES 3D - Remettre les heros Paragon
rem Remet ce qu'ALLEGER_PARAGON.bat a mis de cote (26/11/2026). Unreal ferme.
setlocal
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "CONTENT=%PROJET%\Content"
set "SAUVE=%PROJET%\_Sauvegardes\paragon_retires"
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)
if not exist "%SAUVE%\liste.txt" (
    echo Rien a remettre.
    pause
    exit /b 0
)
for /f "usebackq tokens=1,*" %%A in ("%SAUVE%\liste.txt") do (
    if "%%A"=="DOSSIER" if exist "%SAUVE%\%%B" if not exist "%CONTENT%\%%B" move "%SAUVE%\%%B" "%CONTENT%\" >nul && echo   %%B remis
    if "%%A"=="FICHIER" if exist "%SAUVE%\%%B" move "%SAUVE%\%%B" "%CONTENT%\%%B" >nul && echo   %%B remis
)
del "%SAUVE%\liste.txt"
set "W=%PROJET%\Plugins\ChellesWraith\Wraith.txt"
if exist "%W%.avant_paragon" (
    copy /y "%W%.avant_paragon" "%W%" >nul
    del "%W%.avant_paragon"
    echo   Wraith.txt remis
)
echo.
echo TERMINE : tout est revenu comme avant.
pause
