@echo off
chcp 65001 >nul
title CHELLES 3D - Defis de la semaine sur le VPS
set VPS=ubuntu@51.178.54.208
cd /d "%~dp0"
echo ==============================================================
echo   CHELLES 3D : DEFIS DE LA SEMAINE (Discord + jeu) sur le VPS
echo ==============================================================
where ssh >nul 2>nul || (echo [ERREUR] ssh introuvable. & pause & exit /b 1)
echo.
echo Etape 1/3 : preparation du Discord (salon + webhook)...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0preparer_discord_defis.ps1"
if errorlevel 1 (
  echo.
  echo [ATTENTION] Discord pas pret : le jeu aura le defi, mais pas d'annonce sur Discord.
  echo Lisez LISEZMOI_DEFIS.txt, plan B. Appuyez sur une touche pour continuer quand meme.
  pause >nul
)
echo.
echo Etape 2/3 : envoi des fichiers...
ssh %VPS% "mkdir -p ~/serveur-defis && chmod 700 ~/serveur-defis"
scp serveur-defis\defis.js serveur-defis\defis.json serveur-defis\package.json serveur-defis\installer_defis.sh %VPS%:serveur-defis/
if errorlevel 1 (echo [ERREUR] Envoi impossible. & pause & exit /b 1)
if exist serveur-defis\discord.env (
  scp serveur-defis\discord.env %VPS%:serveur-defis/discord.env
  if errorlevel 1 (echo [ERREUR] Envoi du webhook impossible. & del /f /q serveur-defis\discord.env & pause & exit /b 1)
  rem L'adresse du webhook ne reste PAS sur le PC : seulement sur le VPS.
  del /f /q serveur-defis\discord.env
  echo Webhook envoye puis efface du PC.
)
echo.
echo Etape 3/3 : installation sur le VPS...
ssh -t %VPS% "sed -i 's/\r$//' ~/serveur-defis/*.sh; sudo bash ~/serveur-defis/installer_defis.sh"
echo.
echo Test depuis le PC :
curl.exe -s "http://51.178.54.208:8082/api/defis/semaine"
echo.
echo.
echo Faites une capture de cette fenetre et envoyez-la a Claude.
pause
