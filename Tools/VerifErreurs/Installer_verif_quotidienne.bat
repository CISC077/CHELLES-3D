@echo off
title Installer la verification quotidienne - CHELLES 3D
echo.
echo  Installe une tache Windows qui verifie le projet CHELLES 3D tous les soirs.
echo  Heure par defaut : 23:00. Appuie sur Entree pour garder 23:00,
echo  ou tape une autre heure (exemple 21:30) puis Entree.
echo.
set "HEURE=23:00"
set /p HEURE=  Heure : 
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer_tache.ps1" -Heure "%HEURE%"
echo.
pause
