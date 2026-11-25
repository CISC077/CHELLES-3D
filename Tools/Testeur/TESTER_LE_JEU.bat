@echo off
setlocal enabledelayedexpansion
title Testeur automatique - CHELLES 3D
rem ------------------------------------------------------------------
rem  Agent 5 "Testeur" : lance le jeu tout seul, parcourt la carte Rachid,
rem  ouvre les menus, change de personnage / vehicule, puis ecrit le rapport
rem  Tools\Testeur\Rapport\DERNIER_RAPPORT_TEST.txt (il s'ouvre a la fin).
rem  Ne touche pas aux mains pendant le test (3 a 6 minutes).
rem  "auto" = lance par la tache du soir (pas de pause a la fin).
rem ------------------------------------------------------------------
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

echo.
echo  Le jeu va s'ouvrir tout seul et se promener dans la carte.
echo  Ne touche ni au clavier ni a la souris jusqu'a ce qu'il se ferme.
echo.
if "!PY:~0,2!"=="py" (
  !PY! -I "%~dp0tester_le_jeu.py" %MODE%
) else (
  "!PY!" -I "%~dp0tester_le_jeu.py" %MODE%
)
set "RESULTAT=%errorlevel%"

if /i not "%MODE%"=="auto" (
  echo.
  echo  Termine. Le rapport est dans Tools\Testeur\Rapport\DERNIER_RAPPORT_TEST.txt
  pause
)
exit /b %RESULTAT%
