@echo off
chcp 65001 >nul
title CHELLES 3D - Annoncer le defi maintenant
set VPS=ubuntu@51.178.54.208
echo ==============================================================
echo   CHELLES 3D : envoie l'annonce du defi de la semaine MAINTENANT
echo   (normalement elle part toute seule le lundi a 18 h)
echo ==============================================================
echo.
ssh %VPS% "sudo -u chelles bash -c 'cd /opt/chelles3d/defis && node defis.js --tester && echo && node defis.js --annoncer'"
echo.
pause
