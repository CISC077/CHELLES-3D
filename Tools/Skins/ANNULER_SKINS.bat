@echo off
title CHELLES 3D - Annuler skins et habillage
rem Remet les fichiers sources d'avant le 25/11 (copies *.avant_skins), puis recompile.
setlocal
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
for %%F in (
  "Plugins\ChellesWraith\Source\ChellesWraith\Private\WraithChelles.cpp"
  "Plugins\ChellesWraith\Source\ChellesWraith\Public\WraithChelles.h"
  "Plugins\ChellesMenu\Source\ChellesMenu\Private\HabillageChelles.cpp"
  "Plugins\ChellesMenu\Source\ChellesMenu\Public\HabillageChelles.h"
) do (
  if exist "%PROJET%\%%~F.avant_skins" copy /y "%PROJET%\%%~F.avant_skins" "%PROJET%\%%~F" >nul && echo remis : %%~F
)
echo.
echo Les fichiers supprimes (doublons de LUCIA) ne reviennent pas : ils etaient inutilisables.
echo Relancez REPARER_SKINS.bat pour recompiler (il refera aussi la verification).
pause
