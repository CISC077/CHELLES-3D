@echo off
rem ------------------------------------------------------------------
rem  CHELLES 3D - Remettre le SUV (annule la quarantaine faite par INSTALLER_POLICE.bat)
rem  Remet Content\DriveableCar_basic et Plugins\ChellesConduite a leur place.
rem  Les voitures de police restent installees. Unreal doit etre FERME.
rem ------------------------------------------------------------------
setlocal enabledelayedexpansion
title CHELLES 3D - Remettre le SUV
cd /d "%~dp0"
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)
set "UE=C:\Program Files\Epic Games\UE_4.27"
for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\4.27" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE=%%b"
set "PY=!UE!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
if not exist "!PY!" (
    echo Python d'Unreal introuvable dans "!UE!". Dis-le a Claude.
    pause
    exit /b 1
)
"!PY!" -I "%~dp0police_installer.py" remettre
echo.
pause
