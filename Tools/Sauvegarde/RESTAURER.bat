@echo off
rem ------------------------------------------------------------------
rem  RESTAURATION du projet CHELLES 3D a partir d'une sauvegarde
rem  faite par SAUVEGARDER.bat.
rem
rem  Deux facons de l'utiliser :
rem   - double-clic sur RESTAURER_CETTE_SAUVEGARDE.bat DANS le dossier
rem     de sauvegarde (CHELLES 3D_BACKUP_...) : restaure celle-la ;
rem   - double-clic sur Tools\Sauvegarde\RESTAURER.bat dans le projet :
rem     affiche la liste des sauvegardes et tu choisis.
rem
rem  ATTENTION : le projet redevient EXACTEMENT comme dans la sauvegarde.
rem  Ce qui a ete ajoute ou modifie depuis dans Content, Config, Plugins,
rem  Source, Binaries, Build et Tools est EFFACE.
rem  Intermediate, Saved, DerivedDataCache et _Quarantaine ne sont pas touches.
rem ------------------------------------------------------------------
setlocal EnableExtensions EnableDelayedExpansion
title Restauration CHELLES 3D

rem --- Ce script se copie dans TEMP et se relance de la-bas, car il va
rem --- peut-etre remplacer son propre fichier pendant la restauration.
if not "%~1"=="--depuis" (
    copy /y "%~f0" "%TEMP%\RESTAURER_CHELLES_3D.bat" >nul
    call "%TEMP%\RESTAURER_CHELLES_3D.bat" --depuis "%~dp0."
    exit /b
)
for %%I in ("%~2") do set "ICI=%%~fI"

echo ==================================================================
echo   RESTAURATION DU PROJET CHELLES 3D
echo ==================================================================
echo.

rem --- 1. Trouver la sauvegarde (SRC) et le projet (PROJET)
if exist "%ICI%\SAUVEGARDE_INFO.txt" (
    rem Lance depuis le dossier de sauvegarde
    set "SRC=%ICI%"
    for /f "usebackq tokens=1,* delims==" %%A in ("%ICI%\SAUVEGARDE_INFO.txt") do (
        if /i "%%A"=="PROJET" set "PROJET=%%B"
    )
    goto verif_projet
)

rem Lance depuis Tools\Sauvegarde du projet : liste des sauvegardes
rem (a cote du projet et a la racine de chaque disque)
for %%I in ("%ICI%\..\..") do set "PROJET=%%~fI"
for %%I in ("%PROJET%\..") do set "PARENT=%%~fI"
set /a N=0
echo Sauvegardes trouvees :
echo.
call :lister "%PARENT%"
for %%L in (C D E F G H I J K L M N O P Q R S T U V W X Y Z) do if exist "%%L:\" call :lister "%%L:"
if %N%==0 (
    echo    aucune sauvegarde trouvee.
    echo.
    echo Si ta sauvegarde est ailleurs, ouvre son dossier et
    echo double-clique sur RESTAURER_CETTE_SAUVEGARDE.bat.
    pause
    exit /b 1
)
echo.
set "CHOIX="
set /p "CHOIX=Tape le NUMERO de la sauvegarde a restaurer puis ENTREE : "
if not defined B%CHOIX% (
    echo Numero invalide. Abandon.
    pause
    exit /b 1
)
set "SRC=!B%CHOIX%!"

:verif_projet
if not exist "%PROJET%\" (
    echo Le dossier du projet n'existe plus : %PROJET%
    set /p "PROJET=Colle le chemin du dossier du projet puis ENTREE : "
)
if not exist "%PROJET%\" (
    echo Dossier introuvable. Abandon.
    pause
    exit /b 1
)

rem --- 2. Verifier la sauvegarde
if exist "%SRC%\SAUVEGARDE_INCOMPLETE.txt" (
    echo [ERREUR] Cette sauvegarde est INCOMPLETE ^(copie interrompue^).
    echo Je refuse de l'utiliser : %SRC%
    pause
    exit /b 1
)
if not exist "%SRC%\Content\" goto sauvegarde_invalide
if not exist "%SRC%\*.uproject" goto sauvegarde_invalide

rem --- 3. Unreal doit etre ferme
:verif_unreal
tasklist /FI "IMAGENAME eq UE4Editor.exe" 2>nul | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo [ATTENTION] Unreal Engine est ouvert. Ferme-le completement,
    echo puis appuie sur une touche pour continuer.
    pause >nul
    goto verif_unreal
)

rem --- 4. Confirmation
echo.
echo Sauvegarde : %SRC%
echo Projet     : %PROJET%
echo.
echo ATTENTION : le projet va redevenir EXACTEMENT comme dans la sauvegarde.
echo Tout ce que tu as ajoute ou modifie depuis sera EFFACE.
echo (Si tu veux garder l'etat actuel, annule et lance d'abord SAUVEGARDER.bat.)
echo.
set "REP="
set /p "REP=Pour confirmer, tape OUI en majuscules puis ENTREE : "
if not "%REP%"=="OUI" (
    echo Restauration annulee. Rien n'a ete modifie.
    pause
    exit /b 0
)

rem --- 4b. Les photos du RETOUR EN ARRIERE automatique partagent leurs fichiers
rem --- avec le projet : une restauration complete les rendrait fausses, on les efface.
for %%I in ("%PROJET%\..") do set "PARENT_PROJET=%%~fI"
if exist "%PARENT_PROJET%\CHELLES3D_RetourArriere\" (
    echo Effacement des photos du retour en arriere automatique ^(elles ne correspondent plus^)...
    rmdir /s /q "%PARENT_PROJET%\CHELLES3D_RetourArriere"
)

rem --- 5. Restauration
set "JOURNAL=%SRC%\journal_restauration.txt"
set "OPTIONS=/R:2 /W:2 /MT:16 /NP /NFL /NDL /COPY:DAT /DCOPY:T"
set "ECHEC="
echo.
echo [fichiers] racine du projet
robocopy "%SRC%" "%PROJET%" *.* %OPTIONS% /XF SAUVEGARDE_INFO.txt SAUVEGARDE_INCOMPLETE.txt RESTAURER_CETTE_SAUVEGARDE.bat journal_sauvegarde.txt journal_restauration.txt /LOG+:"%JOURNAL%" >nul
if errorlevel 8 set "ECHEC=1"

for %%F in (Config Source Binaries Build Tools Plugins Content) do call :restaurer %%F

echo.
if defined ECHEC (
    echo [ERREUR] Certains fichiers n'ont pas pu etre remis.
    echo Regarde la fin du journal : %JOURNAL%
    echo Tu peux relancer la restauration, elle reprendra la ou il faut.
    pause
    exit /b 1
)
echo ==================================================================
echo   RESTAURATION TERMINEE
echo ==================================================================
for %%I in ("%SRC%") do echo Le projet est revenu a l'etat de la sauvegarde %%~nxI.
echo Tu peux rouvrir Unreal. Le premier lancement peut etre un peu plus
echo long ^(Unreal recalcule ses fichiers temporaires^).
pause
exit /b 0

:sauvegarde_invalide
echo [ERREUR] Ce dossier ne ressemble pas a une sauvegarde complete :
echo    %SRC%
pause
exit /b 1

rem ------------------------------------------------------------------
:lister
for /d %%B in ("%~1\CHELLES 3D_*") do (
    if exist "%%~fB\SAUVEGARDE_INFO.txt" (
        set /a N+=1
        set "B!N!=%%~fB"
        if exist "%%~fB\SAUVEGARDE_INCOMPLETE.txt" (
            echo    !N!. %%~fB   [INCOMPLETE - ne pas utiliser]
        ) else (
            echo    !N!. %%~fB
        )
    )
)
exit /b 0

rem ------------------------------------------------------------------
:restaurer
if not exist "%SRC%\%~1\" exit /b 0
echo [dossier] %~1 ...
rem /MIR = copie miroir : remet les fichiers de la sauvegarde ET efface
rem ceux qui n'y sont pas. Les dossiers Intermediate des plugins sont ignores.
if /i "%~1"=="Plugins" (
    robocopy "%SRC%\%~1" "%PROJET%\%~1" /MIR %OPTIONS% /XD Intermediate /LOG+:"%JOURNAL%" >nul
) else (
    robocopy "%SRC%\%~1" "%PROJET%\%~1" /MIR %OPTIONS% /LOG+:"%JOURNAL%" >nul
)
if errorlevel 8 (
    echo    [ERREUR] probleme pendant la restauration de %~1
    set "ECHEC=1"
)
exit /b 0
