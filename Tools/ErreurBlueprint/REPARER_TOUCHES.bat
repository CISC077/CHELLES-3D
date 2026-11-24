@echo off
chcp 65001 >nul
title CHELLES 3D - Reparer les touches (Input.ini)
REM 24/11/2026 : Saved\Config\Windows\Input.ini contient seulement 3 touches d'un projet modele
REM (Jump, ResetVR) et ecrase celles du jeu (Fire, Use, Reload, Crouch...) -> "unknown Action".
REM Ce script le met de cote (renomme). Unreal en recree un propre au prochain lancement.
REM Les vraies touches du jeu sont dans Config\DefaultInput.ini (non touche).
cd /d "%~dp0..\.."
tasklist /FI "IMAGENAME eq UE4Editor.exe" | find /I "UE4Editor.exe" >nul
if not errorlevel 1 (
  echo.
  echo  Unreal est encore ouvert. Fermez Unreal puis relancez ce fichier.
  echo.
  pause
  exit /b 1
)
set "F=Saved\Config\Windows\Input.ini"
if not exist "%F%" (
  echo  Rien a faire : %F% n'existe pas.
  pause
  exit /b 0
)
for /f %%d in ('powershell -NoProfile -Command "Get-Date -Format yyyy-MM-dd_HHmm"') do set "D=%%d"
ren "%F%" "Input.ini.casse_%D%"
if errorlevel 1 (
  echo  ERREUR : impossible de renommer %F%.
) else (
  echo  OK : %F% renomme en Input.ini.casse_%D%
  echo  Pour annuler : supprimer le nouveau Input.ini et renommer Input.ini.casse_%D% en Input.ini.
)
echo.
pause
