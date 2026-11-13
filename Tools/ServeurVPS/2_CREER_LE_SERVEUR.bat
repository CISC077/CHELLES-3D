@echo off
rem ------------------------------------------------------------------
rem  CHELLES 3D - Etape 2 : fabrique le SERVEUR DEDIE LINUX (pour le VPS)
rem  et, dans la meme passe, le JEU WINDOWS a donner aux joueurs.
rem  Les deux sortent du MEME Unreal : c'est obligatoire pour qu'ils
rem  puissent se parler (sinon "version incompatible" a la connexion).
rem
rem  Prerequis : 1_PREPARER_UNREAL_SOURCE.bat termine sans erreur.
rem  FERMEZ UNREAL avant de lancer ce fichier. Duree : 1 a 4 heures.
rem  Ne modifie pas le projet, sauf : ajoute Source\VehicleGameServer.Target.cs
rem  (sans effet sur le Unreal du Launcher).
rem ------------------------------------------------------------------
setlocal EnableExtensions
title CHELLES 3D - Creation du serveur Linux

set "ICI=%~dp0"
set "MEMO=%ICI%chemin_unreal_source.txt"
if not exist "%MEMO%" (
    echo Lancez d'abord 1_PREPARER_UNREAL_SOURCE.bat.
    pause
    exit /b 1
)
set /p UE=<"%MEMO%"
if not exist "%UE%\Engine\Binaries\Win64\UE4Editor.exe" (
    echo Unreal ^(sources^) n'est pas compile dans "%UE%".
    echo Relancez 1_PREPARER_UNREAL_SOURCE.bat jusqu'au message TERMINE.
    pause
    exit /b 1
)
if "%LINUX_MULTIARCH_ROOT%"=="" (
    echo La toolchain Linux n'est pas installee. Voir LISEZMOI.md, etape 2.
    pause
    exit /b 1
)

rem Le projet = deux dossiers au-dessus de Tools\ServeurVPS
for %%I in ("%ICI%..\..") do set "PROJET=%%~fI"
set "UPROJECT=%PROJET%\FPS_Chelles.uproject"
if not exist "%UPROJECT%" (
    echo Projet introuvable : "%UPROJECT%"
    echo Ce fichier doit rester dans CHELLES 3D\Tools\ServeurVPS.
    pause
    exit /b 1
)

for %%P in (UE4Editor.exe UE4Editor-Cmd.exe AutomationTool.exe UnrealPak.exe UnrealBuildTool.exe) do (
    tasklist /fi "imagename eq %%P" | find /i "%%P" >nul
    if not errorlevel 1 (
        echo %%P tourne encore : fermez Unreal ^(ou attendez la fin d'un export^), puis relancez.
        pause
        exit /b 1
    )
)

rem ---- La cible serveur
if not exist "%PROJET%\Source\VehicleGameServer.Target.cs" (
    copy /y "%ICI%Source\VehicleGameServer.Target.cs" "%PROJET%\Source\" >nul
    echo Cible serveur ajoutee : Source\VehicleGameServer.Target.cs
)

rem ---- Sortie
set "SORTIE=%USERPROFILE%\Documents\CHELLES3D_SERVEUR"
if exist "D:\" set "SORTIE=D:\CHELLES3D_SERVEUR"
echo.
echo Le serveur et le jeu seront crees dans : %SORTIE%
set "AUTRE="
set /p "AUTRE=Entree pour accepter, ou tapez un autre dossier : "
if not "%AUTRE%"=="" set "SORTIE=%AUTRE%"

echo.
echo Le jeu Windows fabrique en meme temps (celui a donner aux joueurs) :
echo   D = DEMO 20 minutes      C = JEU COMPLET
set "VERSION=D"
set /p "VERSION=Votre choix (D/C) : "

echo.
echo Fabriquer aussi le jeu Windows ? Recommande : O
echo (le jeu deja donne aux amis, fait avec le Launcher, risque de ne pas pouvoir se connecter)
set "CLIENT=O"
set /p "CLIENT=(O/N) : "
set "OPTIONS_CLIENT=-platform=Win64 -clientconfig=Shipping"
if /i "%CLIENT%"=="N" set "OPTIONS_CLIENT=-platform=Win64 -noclient"

set "JOURNAL=%ICI%Journal"
if exist "%JOURNAL%" rmdir /s /q "%JOURNAL%"
if exist "%ICI%Erreurs_serveur.txt" del "%ICI%Erreurs_serveur.txt"
mkdir "%JOURNAL%"
if exist "%APPDATA%\Unreal Engine\AutomationTool\Logs" rmdir /s /q "%APPDATA%\Unreal Engine\AutomationTool\Logs"

set "PREPA=%ICI%..\JeuComplet\preparer_config.ps1"
if /i "%VERSION%"=="C" (
    if exist "%PREPA%" (
        powershell -NoProfile -ExecutionPolicy Bypass -File "%PREPA%" -Mode Complet -Projet "%PROJET%"
    ) else (
        echo Tools\JeuComplet\preparer_config.ps1 introuvable : ce sera la DEMO.
    )
)

echo.
echo ===== Etape 1/3 : compilation + cuisson + paquet (1 a 4 heures, ne fermez pas cette fenetre)
echo La premiere fois, Unreal recompile aussi les plugins Chelles pour cette version : c'est normal.
echo Debut : %date% %time%
call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%UPROJECT%" -noP4 %OPTIONS_CLIENT% -server -serverplatform=Linux -serverconfig=Development -build -cook -map=/Game/Maps/Rachid -stage -pak -compressed -prereqs -nodebuginfo -archive -archivedirectory="%SORTIE%" -IgnoreCookErrors -unattended -utf8output
set "RESULTAT=%errorlevel%"
echo Fin : %date% %time%

if /i "%VERSION%"=="C" if exist "%PREPA%" powershell -NoProfile -ExecutionPolicy Bypass -File "%PREPA%" -Mode Remettre -Projet "%PROJET%"

xcopy /e /i /y "%APPDATA%\Unreal Engine\AutomationTool\Logs" "%JOURNAL%" >nul 2>nul
for /f "delims=" %%F in ('dir /b /s "%JOURNAL%\*.txt" 2^>nul') do findstr /i /c:"Error:" /c:": error" /c:"Fatal" "%%F" >>"%ICI%Erreurs_serveur.txt" 2>nul

if not "%RESULTAT%"=="0" (
    echo.
    echo ===== ECHEC ^(code %RESULTAT%^)
    echo Erreurs : Tools\ServeurVPS\Erreurs_serveur.txt   Detail : Tools\ServeurVPS\Journal
    echo Dites-le a Claude, il lira les journaux.
    pause
    exit /b 1
)
if exist "%ICI%Erreurs_serveur.txt" del "%ICI%Erreurs_serveur.txt"

set "SERVEUR=%SORTIE%\LinuxServer"
if not exist "%SERVEUR%" (
    echo Dossier du serveur introuvable : "%SERVEUR%"
    pause
    exit /b 1
)

echo.
echo ===== Etape 2/3 : ajout des fichiers lus par les plugins (repliques, rues, voix)
call :AJOUTER "%SERVEUR%\FPS_Chelles"
if exist "%SORTIE%\WindowsNoEditor" call :AJOUTER "%SORTIE%\WindowsNoEditor\FPS_Chelles"

echo.
echo ===== Etape 3/3 : archive a envoyer sur le VPS
if exist "%SORTIE%\CHELLES3D_Serveur_Linux.tar.gz" del "%SORTIE%\CHELLES3D_Serveur_Linux.tar.gz"
tar -czf "%SORTIE%\CHELLES3D_Serveur_Linux.tar.gz" -C "%SORTIE%" LinuxServer
if errorlevel 1 (
    echo La creation de l'archive a echoue.
    pause
    exit /b 1
)
copy /y "%ICI%VPS\installer_serveur.sh" "%SORTIE%\" >nul
for /f %%G in ('powershell -NoProfile -Command "[math]::Round((Get-Item -LiteralPath '%SORTIE%\CHELLES3D_Serveur_Linux.tar.gz').Length/1GB,2)"') do set "TAILLE=%%G"
> "%ICI%Dernier_serveur.txt" (
    echo Serveur cree le %date% %time%
    echo Archive : %SORTIE%\CHELLES3D_Serveur_Linux.tar.gz ^(%TAILLE% Go^)
    echo Jeu Windows : %SORTIE%\WindowsNoEditor
)

echo.
echo TERMINE.
echo  A envoyer sur le VPS avec FileZilla (dossier /home/ubuntu) :
echo     %SORTIE%\CHELLES3D_Serveur_Linux.tar.gz   (%TAILLE% Go)
echo     %SORTIE%\installer_serveur.sh
echo  Puis suivez LISEZMOI.md, etape 6.
start "" "%SORTIE%"
pause
exit /b 0

:AJOUTER
set "DEST=%~1"
robocopy "%PROJET%\Plugins" "%DEST%\Plugins" *.txt /S /XD Intermediate Binaries Source Config Sauvegarde* /XF Journal_compilation.txt /NFL /NDL /NJH /NJS /NP >nul
if exist "%PROJET%\Plugins\ChellesParoles\Voix" robocopy "%PROJET%\Plugins\ChellesParoles\Voix" "%DEST%\Plugins\ChellesParoles\Voix" /E /NFL /NDL /NJH /NJS /NP >nul
if exist "%PROJET%\Tools\out\osm_ways.json" (
    mkdir "%DEST%\Tools\out" 2>nul
    copy /y "%PROJET%\Tools\out\osm_ways.json" "%DEST%\Tools\out\" >nul
)
if exist "%PROJET%\Content\ChellesMenu\Bible_Chelles.txt" (
    mkdir "%DEST%\Content\ChellesMenu" 2>nul
    copy /y "%PROJET%\Content\ChellesMenu\Bible_Chelles.txt" "%DEST%\Content\ChellesMenu\" >nul
)
exit /b 0
