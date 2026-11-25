@echo off
chcp 65001 >nul
title CHELLES 3D - Ajout de la cle IA des PNJ sur le VPS
set VPS=ubuntu@51.178.54.208
echo ==============================================================
echo   CHELLES 3D : ajout de la nouvelle cle dans l'IA des PNJ
echo   (la cle est deja sur le VPS, elle n'est jamais affichee)
echo ==============================================================
echo.
ssh %VPS% "set -e; cd ~/serveur-pnj; if [ -f ajout.env ]; then sed -i 's/\r$//' ajout.env; for v in $(cut -d= -f1 ajout.env); do sudo sed -i \"/^$v=/d\" /opt/chelles3d/pnj/.env; done; sudo sh -c 'cat /home/ubuntu/serveur-pnj/ajout.env >> /opt/chelles3d/pnj/.env'; shred -u ajout.env 2>/dev/null || rm -f ajout.env; echo 'Cle ajoutee, copie effacee.'; fi; sudo install -m 644 -o chelles -g chelles pnj.js /opt/chelles3d/pnj/pnj.js; sudo chown chelles:chelles /opt/chelles3d/pnj/.env; sudo chmod 600 /opt/chelles3d/pnj/.env; sudo systemctl restart chelles3d-pnj; sleep 2; sudo -u chelles bash -c 'cd /opt/chelles3d/pnj && node pnj.js --tester'; echo; systemctl is-active chelles3d-pnj chelles3d-chat chelles3d"
echo.
echo Test depuis le PC :
curl.exe -s "http://51.178.54.208:8081/api/pnj?categorie=Bousculade&genre=F&nb=3"
echo.
echo.
echo Faites une capture de cette fenetre et envoyez-la a Claude.
pause
