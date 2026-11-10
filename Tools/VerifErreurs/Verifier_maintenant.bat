@echo off
setlocal enabledelayedexpansion
title Verification des erreurs - CHELLES 3D
cd /d "%~dp0"
set "MODE=%~1"
if "%MODE%"=="" set "MODE=normal"

set "PY="
for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\4.27" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE=%%b"
if defined UE if exist "!UE!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" set "PY=!UE!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
if not defined PY if exist "C:\Program Files\Epic Games\UE_4.27\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" set "PY=C:\Program Files\Epic Games\UE_4.27\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
if not defined PY ( where py >nul 2>nul && set "PY=py -3" )
if not defined PY ( where python >nul 2>nul && set "PY=python" )
if not defined PY (
  echo  Python introuvable. Dis-le a Claude.
  if /i not "%MODE%"=="auto" pause
  exit /b 1
)

if not exist "%~dp0Rapport" mkdir "%~dp0Rapport"
if "!PY:~0,2!"=="py" (
  !PY! -I "%~dp0verifier_erreurs.py" %MODE%
) else (
  "!PY!" -I "%~dp0verifier_erreurs.py" %MODE%
)

if /i not "%MODE%"=="auto" (
  echo.
  echo  Termine. Le rapport est dans Tools\VerifErreurs\Rapport\DERNIER_RAPPORT.txt
  pause
)
