@echo off
rem Utilise par les fichiers TESTER_... : ecrit Mode=%1 dans Plugins\ChellesFetes\Fetes.txt
set "FICHIER=%~dp0..\..\Plugins\ChellesFetes\Fetes.txt"
for %%I in ("%FICHIER%") do set "FICHIER=%%~fI"
if not exist "%FICHIER%" (
    echo Fetes.txt introuvable : "%FICHIER%"
    pause
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$f=$env:FICHIER; (Get-Content -LiteralPath $f) -replace '^Mode=.*','Mode=%~1' | Set-Content -LiteralPath $f -Encoding ASCII"
echo.
echo Fetes.txt : Mode=%~1
echo Ouvrez CHELLES 3D (ou faites Play) pour voir le resultat.
echo (Si le jeu est deja lance : console (touche a gauche du 1) puis  Chelles.Fetes Relire)
echo.
pause
