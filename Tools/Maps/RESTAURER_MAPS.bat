@echo off
chcp 65001 >nul
title CHELLES 3D - Remettre les cartes supprimees
rem Remet dans le projet les cartes du DERNIER dossier _Sauvegardes\avant_maps_<date>. Unreal FERME.
setlocal
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "DERNIER="
for /f "delims=" %%D in ('dir /b /ad /o:n "%PROJET%\_Sauvegardes\avant_maps_*" 2^>nul') do set "DERNIER=%%D"
if not defined DERNIER (
    echo Aucune sauvegarde avant_maps_ trouvee dans _Sauvegardes.
    pause
    exit /b 1
)
echo Remettre les cartes de _Sauvegardes\%DERNIER% ?
set "REPONSE=N"
set /p "REPONSE=O = oui, N = non, puis Entree : "
if /i not "%REPONSE%"=="O" exit /b 0
robocopy "%PROJET%\_Sauvegardes\%DERNIER%" "%PROJET%" /E /XF maps_supprimees.json /NJH /NJS /NDL /NFL
echo TERMINE : les cartes sont revenues ^(la copie de secours reste aussi dans _Sauvegardes^).
pause
