@echo off
rem ------------------------------------------------------------------
rem  SAUVEGARDE du projet CHELLES 3D (double-clic).
rem  Tu donnes un NOM a la sauvegarde (ex : SAV-MENU-OPTION).
rem  Dossier cree :  CHELLES 3D_<NOM>_AAAA-MM-JJ_HHhMM
rem
rem  Copie Content, Config, Plugins, Source, Binaries, Build, Tools
rem  et les fichiers a la racine (.uproject...).
rem  Ne copie PAS : Intermediate, Saved, DerivedDataCache, .vs, _Quarantaine
rem  (Unreal les recree tout seul).
rem
rem  S'il existe deja une sauvegarde sur le meme disque, les fichiers qui
rem  n'ont pas change ne sont PAS recopies : ils sont partages avec elle
rem  (liens durs NTFS). Chaque sauvegarde reste complete toute seule et
rem  peut etre supprimee sans abimer les autres.
rem  Ne modifie RIEN dans le projet.
rem ------------------------------------------------------------------
setlocal EnableExtensions
title Sauvegarde CHELLES 3D

rem --- Dossier du projet = deux niveaux au-dessus de ce fichier (Tools\Sauvegarde)
for %%I in ("%~dp0..\..") do set "PROJET=%%~fI"
for %%I in ("%PROJET%\..") do set "PARENT=%%~fI"

if not exist "%PROJET%\*.uproject" (
    echo.
    echo [ERREUR] Je ne trouve pas le fichier .uproject dans :
    echo    %PROJET%
    echo Ce fichier doit rester dans le dossier Tools\Sauvegarde du projet.
    pause
    exit /b 1
)

echo ==================================================================
echo   SAUVEGARDE DU PROJET CHELLES 3D
echo ==================================================================
echo Projet : %PROJET%
echo.

rem --- 1. Unreal doit etre ferme (sinon des fichiers a moitie enregistres)
:verif_unreal
tasklist /FI "IMAGENAME eq UE4Editor.exe" 2>nul | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo [ATTENTION] Unreal Engine est encore ouvert.
    echo Enregistre ton travail ^(File ^> Save All^), ferme Unreal,
    echo puis appuie sur une touche pour continuer.
    pause >nul
    goto verif_unreal
)

rem --- 2. Nom de la sauvegarde
echo Donne un NOM a cette sauvegarde, sans espace (ex : SAV-MENU-OPTION).
echo ENTREE tout seul = nom BACKUP.
set "ETIQ="
set /p "ETIQ=Nom : "
if "%ETIQ%"=="" set "ETIQ=BACKUP"
set "ETIQ=%ETIQ: =-%"
for /f "usebackq delims=" %%D in (`powershell -NoProfile -Command "Get-Date -Format 'yyyy-MM-dd_HH\hmm'"`) do set "HORO=%%D"
set "NOM=CHELLES 3D_%ETIQ%_%HORO%"
echo.

rem --- 3. Disque de destination (on retient le dernier choix)
set "MEMO=%~dp0dernier_disque.txt"
set "LETTRE="
if exist "%MEMO%" set /p LETTRE=<"%MEMO%"
if defined LETTRE if not exist "%LETTRE%:\" set "LETTRE="
if defined LETTRE (
    echo Disque de sauvegarde : %LETTRE%:
    echo - ENTREE pour garder ce disque,
) else (
    echo Emplacement de sauvegarde : %PARENT%
    echo - ENTREE pour garder cet emplacement,
)
echo - ou tape la LETTRE d'un autre disque (ex : D) puis ENTREE.
set "CHOIX="
set /p "CHOIX=Ton choix : "
if not "%CHOIX%"=="" set "LETTRE=%CHOIX:~0,1%"
if defined LETTRE (
    if not exist "%LETTRE%:\" (
        echo [ERREUR] Le disque %LETTRE%: n'existe pas.
        pause
        exit /b 1
    )
    set "DEST=%LETTRE%:\%NOM%"
    > "%MEMO%" echo %LETTRE%
) else (
    set "DEST=%PARENT%\%NOM%"
)
if exist "%DEST%\" (
    echo [ERREUR] Ce dossier existe deja : %DEST%
    echo Attends une minute ou choisis un autre nom.
    pause
    exit /b 1
)
echo.
echo Destination : %DEST%
echo.

rem --- 4. Place necessaire ?
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0preparer_sauvegarde.ps1" -Etape Verifier -Projet "%PROJET%" -Destination "%DEST%"
if errorlevel 1 (
    echo.
    echo [ERREUR] Pas assez de place libre pour la sauvegarde.
    echo Libere de la place, supprime une vieille sauvegarde,
    echo ou relance et choisis un autre disque.
    pause
    exit /b 1
)
echo.
set "OK="
set /p "OK=Lancer la sauvegarde ? (O/N) : "
if /i not "%OK%"=="O" (
    echo Sauvegarde annulee.
    pause
    exit /b 0
)

rem --- 5. Copie
mkdir "%DEST%" 2>nul
if not exist "%DEST%\" (
    echo [ERREUR] Impossible de creer le dossier. Le nom contient peut-etre
    echo un caractere interdit. Utilise seulement lettres, chiffres et tirets.
    pause
    exit /b 1
)
set "JOURNAL=%DEST%\journal_sauvegarde.txt"
rem Ce marqueur est efface a la fin. S'il reste, la sauvegarde est incomplete.
echo Sauvegarde en cours ou interrompue. Ne pas restaurer. > "%DEST%\SAUVEGARDE_INCOMPLETE.txt"

echo.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0preparer_sauvegarde.ps1" -Etape Lier -Projet "%PROJET%" -Destination "%DEST%"

set "OPTIONS=/R:2 /W:2 /MT:16 /NP /NFL /NDL /COPY:DAT /DCOPY:T"
set "ECHEC="

echo.
echo [fichiers] racine du projet (.uproject, .bat, ...)
robocopy "%PROJET%" "%DEST%" *.* %OPTIONS% /XF SAUVEGARDE_INCOMPLETE.txt journal_sauvegarde.txt /LOG+:"%JOURNAL%" >nul
if errorlevel 8 set "ECHEC=1"

for %%F in (Config Source Binaries Build Tools Plugins Content) do call :copier %%F

if defined ECHEC goto echec

rem --- 6. Fiche d'information + script de restauration dans la sauvegarde
(
    echo SAUVEGARDE CHELLES 3D
    echo NOM=%ETIQ%
    echo DATE=%HORO%
    echo PROJET=%PROJET%
    echo.
    echo Pour remettre le projet dans cet etat :
    echo double-clic sur RESTAURER_CETTE_SAUVEGARDE.bat dans ce dossier.
) > "%DEST%\SAUVEGARDE_INFO.txt"
copy /y "%~dp0RESTAURER.bat" "%DEST%\RESTAURER_CETTE_SAUVEGARDE.bat" >nul
del "%DEST%\SAUVEGARDE_INCOMPLETE.txt" 2>nul

echo.
echo ==================================================================
echo   SAUVEGARDE "%ETIQ%" TERMINEE
echo ==================================================================
echo Dossier : %DEST%
echo.
echo Pour revenir a cet etat plus tard :
echo    double-clic sur RESTAURER_CETTE_SAUVEGARDE.bat dans ce dossier
echo    (ou sur Tools\Sauvegarde\RESTAURER.bat dans le projet).
echo.
echo Note : Windows affiche la taille complete de chaque sauvegarde, meme
echo si les fichiers identiques sont partages et ne prennent pas de place
echo en double. Regarde plutot la place libre du disque.
echo.
pause
exit /b 0

:echec
echo.
echo [ERREUR] Certains fichiers n'ont pas pu etre copies.
echo La sauvegarde est INCOMPLETE. Regarde la fin du journal :
echo    %JOURNAL%
pause
exit /b 1

rem ------------------------------------------------------------------
:copier
if not exist "%PROJET%\%~1\" exit /b 0
echo [dossier] %~1 ...
if /i "%~1"=="Plugins" (
    rem dans Plugins on ne garde que les sources et les binaires
    robocopy "%PROJET%\%~1" "%DEST%\%~1" /E %OPTIONS% /XD Intermediate /LOG+:"%JOURNAL%" >nul
) else (
    robocopy "%PROJET%\%~1" "%DEST%\%~1" /E %OPTIONS% /LOG+:"%JOURNAL%" >nul
)
if errorlevel 8 (
    echo    [ERREUR] probleme pendant la copie de %~1
    set "ECHEC=1"
)
exit /b 0
