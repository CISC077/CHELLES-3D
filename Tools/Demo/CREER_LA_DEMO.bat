@echo off
rem ------------------------------------------------------------------
rem  CHELLES 3D - Cree la DEMO BETA (jeu Windows a donner aux amis).
rem  20 minutes de jeu puis ecran FIN DE LA DEMO (plugin ChellesDemo).
rem  Ne modifie pas le projet : le jeu exporte est cree dans un autre dossier.
rem  FERMEZ UNREAL avant de lancer ce fichier (double-clic).
rem  -IgnoreCookErrors : les vieux blueprints casses du pack FPS_Game (copie System, jamais utilisee)
rem  ne bloquent plus l'export. Les erreurs restent listees dans Erreurs_demo.txt.
rem  Duree : 1 a 3 heures la premiere fois (les fois suivantes c'est plus rapide).
rem ------------------------------------------------------------------
setlocal EnableExtensions
title CHELLES 3D - Creation de la demo beta

set "UE=C:\Program Files\Epic Games\UE_4.27"
if not exist "%UE%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo Unreal 4.27 introuvable dans "%UE%".
    set /p "UE=Collez le dossier d'installation d'Unreal 4.27 (ex: D:\Epic Games\UE_4.27) puis Entree : "
)
if not exist "%UE%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo Toujours introuvable. Abandon.
    pause
    exit /b 1
)

rem Le projet = deux dossiers au-dessus de Tools\Demo
for %%I in ("%~dp0..\..") do set "PROJET=%%~fI"
set "UPROJECT=%PROJET%\FPS_Chelles.uproject"
if not exist "%UPROJECT%" (
    echo Projet introuvable : "%UPROJECT%"
    echo Ce fichier doit rester dans CHELLES 3D\Tools\Demo.
    pause
    exit /b 1
)

tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)

if not exist "%PROJET%\Plugins\ChellesDemo\Binaries\Win64\UE4Editor-ChellesDemo.dll" (
    echo Le plugin ChellesDemo n'est pas encore compile.
    echo Lancez d'abord : Plugins\ChellesDemo\Compiler_le_plugin.bat
    pause
    exit /b 1
)

rem ---- Dossier de sortie (pas d'espace dans le nom, c'est plus sur)
set "SORTIE=%USERPROFILE%\Documents\CHELLES3D_DEMO"
if exist "D:\" set "SORTIE=D:\CHELLES3D_DEMO"
echo.
echo La demo sera creee dans : %SORTIE%
set "AUTRE="
set /p "AUTRE=Entree pour accepter, ou tapez un autre dossier (ex: E:\DEMO) : "
if not "%AUTRE%"=="" set "SORTIE=%AUTRE%"
set "DISQUE=%SORTIE:~0,1%"

rem ---- Place libre : ~60 Go sur C (fichiers de travail dans le projet), ~30 Go pour la demo
set "LIBRE_C=?"
set "LIBRE_D=?"
for /f %%G in ('powershell -NoProfile -Command "[math]::Floor((Get-PSDrive C).Free/1GB)"') do set "LIBRE_C=%%G"
for /f %%G in ('powershell -NoProfile -Command "[math]::Floor((Get-PSDrive %DISQUE%).Free/1GB)"') do set "LIBRE_D=%%G"
echo.
echo Place libre : C: = %LIBRE_C% Go   /   %DISQUE%: = %LIBRE_D% Go
echo Il faut environ 60 Go libres sur C: et 30 Go sur %DISQUE%: (la carte Rachid pese ~20 Go).
set "OK="
set /p "OK=Continuer ? (O/N) : "
if /i not "%OK%"=="O" exit /b 0

set "JOURNAL=%~dp0Journal"
if exist "%JOURNAL%" rmdir /s /q "%JOURNAL%"
if exist "%~dp0Erreurs_demo.txt" del "%~dp0Erreurs_demo.txt"
mkdir "%JOURNAL%"
if exist "%APPDATA%\Unreal Engine\AutomationTool\Logs" rmdir /s /q "%APPDATA%\Unreal Engine\AutomationTool\Logs"

echo.
echo ===== Etape 1/3 : compilation + cuisson + paquet (1 a 3 heures, ne fermez pas cette fenetre)
echo Debut : %date% %time%
call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%UPROJECT%" -noP4 -platform=Win64 -clientconfig=Shipping -build -nocompileeditor -cook -map=/Game/Maps/Rachid -stage -pak -compressed -prereqs -nodebuginfo -archive -archivedirectory="%SORTIE%" -IgnoreCookErrors -unattended -utf8output
set "RESULTAT=%errorlevel%"
echo Fin : %date% %time%

rem Copie les journaux pour Claude (lisibles sans capture d'ecran)
xcopy /e /i /y "%APPDATA%\Unreal Engine\AutomationTool\Logs" "%JOURNAL%" >nul 2>nul
for /f "delims=" %%F in ('dir /b /s "%JOURNAL%\*.txt" 2^>nul') do findstr /i /c:"Error:" /c:": error" /c:"Fatal" "%%F" >>"%~dp0Erreurs_demo.txt" 2>nul

if not "%RESULTAT%"=="0" (
    echo.
    echo ===== ECHEC de la creation de la demo ^(code %RESULTAT%^)
    echo Les erreurs sont dans Tools\Demo\Erreurs_demo.txt et le detail dans Tools\Demo\Journal.
    echo Envoyez ce message a Claude, il lira les journaux.
    pause
    exit /b 1
)
if exist "%~dp0Erreurs_demo.txt" del "%~dp0Erreurs_demo.txt"

set "JEU=%SORTIE%\WindowsNoEditor"
if not exist "%JEU%" (
    echo Dossier du jeu introuvable : "%JEU%"
    pause
    exit /b 1
)

echo.
echo ===== Etape 2/3 : ajout des fichiers lus par les plugins (repliques, voix, rues, reglages)
set "DEST=%JEU%\FPS_Chelles"
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

set "EXE="
for %%E in ("%JEU%\*.exe") do set "EXE=%%~nxE"

> "%JEU%\LISEZ-MOI.txt" (
    echo CHELLES 3D - DEMO BETA v0.1.0
    echo ==============================
    echo.
    echo Pour jouer : double-cliquez sur %EXE%
    echo La demo dure 20 minutes de jeu ^(le temps passe dans les menus ne compte pas^).
    echo.
    echo Si Windows affiche "Windows a protege votre ordinateur" :
    echo cliquez sur "Informations complementaires" puis "Executer quand meme".
    echo.
    echo Si un message parle de DirectX ou de Visual C++ :
    echo lancez Engine\Extras\Redist\en-us\UE4PrereqSetup_x64.exe une fois.
    echo.
    echo PC conseille : Windows 10/11 64 bits, carte graphique recente, 16 Go de memoire.
    echo.
    echo Version complete : https://www.facebook.com/TIKT0K3D/
)

echo.
echo ===== Etape 3/3 : taille de la demo
for /f %%G in ('powershell -NoProfile -Command "[math]::Round(((Get-ChildItem -LiteralPath '%JEU%' -Recurse -File | Measure-Object Length -Sum).Sum)/1GB,1)"') do set "TAILLE=%%G"
echo La demo pese %TAILLE% Go.
> "%~dp0Derniere_demo.txt" (
    echo Demo creee le %date% %time%
    echo Dossier : %JEU%
    echo Jeu : %EXE%
    echo Taille : %TAILLE% Go
)

echo.
set "ZIP="
set /p "ZIP=Faire un fichier ZIP a envoyer aux amis ? (O/N, peut prendre 30 min) : "
if /i "%ZIP%"=="O" (
    if exist "%SORTIE%\CHELLES3D_Demo_Beta.zip" del "%SORTIE%\CHELLES3D_Demo_Beta.zip"
    tar -a -c -f "%SORTIE%\CHELLES3D_Demo_Beta.zip" -C "%SORTIE%" WindowsNoEditor
    if errorlevel 1 (
        echo Le ZIP a echoue. Utilisez 7-Zip : clic droit sur WindowsNoEditor ^> 7-Zip ^> Ajouter a l'archive.
    ) else (
        echo ZIP cree : %SORTIE%\CHELLES3D_Demo_Beta.zip
    )
)

echo.
echo TERMINE. Testez la demo vous-meme avant de l'envoyer :
echo   %JEU%\%EXE%
start "" "%JEU%"
pause
