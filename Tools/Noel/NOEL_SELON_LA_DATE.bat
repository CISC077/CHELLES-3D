@echo off
rem Change la ligne Mode= de Plugins\ChellesNoel\Noel.txt (pas besoin de recompiler ni de fermer Unreal).
set "FICHIER=%~dp0..\..\Plugins\ChellesNoel\Noel.txt"
if not exist "%FICHIER%" (
    echo Fichier introuvable : %FICHIER%
    pause
    exit /b 1
)
powershell -NoProfile -Command "(Get-Content -LiteralPath $env:FICHIER) -replace '^Mode=.*','Mode=Auto' | Set-Content -LiteralPath $env:FICHIER -Encoding ASCII"
echo Noel suit maintenant la date du PC (1er decembre - 1er janvier).
echo (Si Play est deja lance : arretez-le et relancez-le.)
pause
