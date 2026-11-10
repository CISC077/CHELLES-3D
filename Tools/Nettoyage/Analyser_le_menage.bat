@echo off
setlocal enabledelayedexpansion
title Menage CHELLES 3D (lecture seule)
cd /d "%~dp0"
echo.
echo  Recherche des packs inutiles - RIEN ne sera supprime ni modifie.
echo  Cela peut prendre quelques minutes...
echo.

set "PY="

rem 1) Python fourni avec Unreal 4.27 (installation via le Launcher)
for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\4.27" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE=%%b"
if defined UE if exist "!UE!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" set "PY=!UE!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"

rem 2) Moteur enregistre sous l'identifiant du .uproject
if not defined PY (
  for /f "tokens=2,*" %%a in ('reg query "HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds" /v "{A4561E7F-4B02-2CA7-5A8A-77A8B82092E3}" 2^>nul ^| find "{"') do set "UE=%%b"
  if defined UE if exist "!UE!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" set "PY=!UE!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
)

rem 3) Emplacement par defaut
if not defined PY if exist "C:\Program Files\Epic Games\UE_4.27\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" set "PY=C:\Program Files\Epic Games\UE_4.27\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"

rem 4) Python installe sur Windows
if not defined PY ( where py >nul 2>nul && set "PY=py -3" )
if not defined PY ( where python >nul 2>nul && set "PY=python" )

if not defined PY (
  echo  Python introuvable. Dis-le a Claude, il trouvera une autre solution.
  pause
  exit /b 1
)

if "!PY:~0,2!"=="py" (
  !PY! -I "%~dp0analyse_menage.py"
) else (
  "!PY!" -I "%~dp0analyse_menage.py"
)

echo.
echo  Termine. Tu peux fermer cette fenetre et prevenir Claude.
pause
