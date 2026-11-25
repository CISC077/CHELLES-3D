@echo off
chcp 65001 >nul
title CHELLES 3D - IA des PNJ sur le VPS
set VPS=ubuntu@51.178.54.208
cd /d "%~dp0"
echo ==============================================================
echo   CHELLES 3D : envoi de l'IA des PNJ (passants) sur le VPS
echo ==============================================================
where ssh >nul 2>nul || (echo [ERREUR] ssh introuvable. & pause & exit /b 1)
echo Etape 1/2 : envoi des fichiers...
ssh %VPS% "mkdir -p ~/serveur-pnj && chmod 700 ~/serveur-pnj"
scp serveur-pnj\pnj.js serveur-pnj\installer_pnj.sh %VPS%:serveur-pnj/
if errorlevel 1 (echo [ERREUR] Envoi impossible. & pause & exit /b 1)
if exist serveur-pnj\cles.env (
  scp serveur-pnj\cles.env %VPS%:serveur-pnj/cles.env
  if errorlevel 1 (echo [ERREUR] Envoi des cles impossible. & pause & exit /b 1)
  rem Les cles ne restent PAS sur le PC : seulement sur le VPS.
  del /f /q serveur-pnj\cles.env
  echo Cles envoyees puis effacees du PC.
)
echo.
echo Etape 2/2 : installation sur le VPS...
ssh -t %VPS% "sed -i 's/\r$//' ~/serveur-pnj/*.sh; sudo bash ~/serveur-pnj/installer_pnj.sh"
echo.
pause
