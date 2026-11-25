@echo off
rem ------------------------------------------------------------------
rem  RETOUR EN ARRIERE - CHELLES 3D (double-clic)
rem  Remet le projet comme AVANT le dernier script lance
rem  (photo prise toute seule par la sauvegarde automatique).
rem  ENTREE = la photo la plus recente. Rien n'est efface pour de bon :
rem  ce qui est retire est mis de cote, et l'etat actuel est photographie
rem  avant, pour pouvoir annuler le retour.
rem ------------------------------------------------------------------
setlocal
title Retour en arriere CHELLES 3D

:verif_unreal
tasklist /FI "IMAGENAME eq UE4Editor.exe" 2>nul | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo [ATTENTION] Unreal est ouvert. Enregistre ce que tu veux garder,
    echo ferme Unreal completement, puis appuie sur une touche.
    pause >nul
    goto verif_unreal
)

call :trouver_python
if not defined PY (
    echo [ERREUR] Python introuvable ^(normalement fourni avec Unreal 4.27 dans
    echo C:\Program Files\Epic Games\UE_4.27\Engine\Binaries\ThirdParty\Python3\Win64^).
    pause
    exit /b 1
)

"%PY%" "%~dp0retour_en_arriere.py"
echo.
pause
exit /b 0

:trouver_python
set "PY="
for %%U in ("C:\Program Files\Epic Games\UE_4.27" "D:\Epic Games\UE_4.27" "C:\Epic Games\UE_4.27") do (
    if exist "%%~U\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" (
        set "PY=%%~U\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
        goto :eof
    )
)
for /f "delims=" %%P in ('where python 2^>nul') do (
    set "PY=%%P"
    goto :eof
)
goto :eof
