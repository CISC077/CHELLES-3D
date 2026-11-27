@echo off
rem ------------------------------------------------------------------
rem  CHELLES 3D - Coupe l'appartement du joueur sans rien supprimer :
rem  met Actif=Non dans Plugins\ChellesAppartement\Appartement.txt.
rem  Pour le remettre : Actif=Oui dans ce fichier (Bloc-notes).
rem ------------------------------------------------------------------
setlocal
set "FICHIER=%~dp0..\..\Plugins\ChellesAppartement\Appartement.txt"
if not exist "%FICHIER%" (
    echo Appartement.txt introuvable.
    pause
    exit /b 1
)
powershell -NoProfile -Command "(Get-Content -LiteralPath '%FICHIER%') -replace '^Actif=.*','Actif=Non' | Set-Content -LiteralPath '%FICHIER%' -Encoding UTF8"
echo Appartement coupe (Actif=Non). Relancez le jeu.
pause
