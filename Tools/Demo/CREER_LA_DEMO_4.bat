@echo off
rem ------------------------------------------------------------------
rem  CHELLES 3D - DEMO 4 (2026-11-25) : 40 minutes de jeu.
rem  Copie de CREER_LA_DEMO_3.bat. Sortie : D:\DEMO 4
rem  1. verifie qu'Unreal est ferme
rem  2. verifie que le code VIP (2468) est bien en place
rem  3. regle DefaultGame.ini (40 min, copie .avant_demo4 dans ce dossier)
rem  4. cuisson neuve + paquet Shipping, puis ZIP CHELLES3D_DEMO4.zip
rem  Ne supprime rien : les anciens journaux sont renommes.
rem ------------------------------------------------------------------
setlocal EnableExtensions
chcp 65001 >nul
title CHELLES 3D - Creation de la demo 4 (40 min, D:\DEMO 4)

set "UE=C:\Program Files\Epic Games\UE_4.27"
for %%I in ("%~dp0..\..") do set "PROJET=%%~fI"
set "UPROJECT=%PROJET%\FPS_Chelles.uproject"
set "SORTIE=D:\DEMO 4"

echo.
echo  ===== CHELLES 3D : creation de la DEMO 4 (40 minutes) =====
echo.

for %%P in (UE4Editor.exe UE4Editor-Cmd.exe AutomationTool.exe UnrealPak.exe UnrealBuildTool.exe) do (
    tasklist /fi "imagename eq %%P" | find /i "%%P" >nul
    if not errorlevel 1 (
        echo  %%P tourne encore. Ferme Unreal puis relance ce fichier.
        pause
        exit /b 1
    )
)
if not exist "%PROJET%\Plugins\ChellesDemo\Binaries\Win64\UE4Editor-ChellesDemo.dll" (
    echo  Le plugin ChellesDemo n'est pas compile. Abandon.
    pause
    exit /b 1
)

rem ---- Les plugins ChellesMenu / ChellesWraith doivent etre recompiles apres le code VIP (Tools\Personnages\CREER_PERSONNAGES.bat)
powershell -NoProfile -Command "$p='%PROJET%\Plugins'; $ok=$true; foreach($m in 'ChellesMenu','ChellesWraith'){ $d=Get-Item \"$p\$m\Binaries\Win64\UE4Editor-$m.dll\" -EA SilentlyContinue; $s=Get-ChildItem \"$p\$m\Source\" -Recurse -Include *.cpp,*.h | Sort-Object LastWriteTime | Select-Object -Last 1; if(-not $d -or $d.LastWriteTime -lt $s.LastWriteTime){ $ok=$false; Write-Host \"  $m n'est pas recompile depuis ses dernieres modifs.\" } }; if($ok){exit 0}else{exit 1}"
if errorlevel 1 (
    echo.
    echo  Lance d'abord Tools\Personnages\CREER_PERSONNAGES.bat ^(Unreal ferme^), puis relance ce fichier.
    pause
    exit /b 1
)

rem ---- Verif VIP : les 5 persos VIP doivent etre marques dans les fichiers du jeu
set "VIPOK=1"
for %%N in ("MR.SHEMS" "LUCIA" "BRENDA" "SOFIA" "LENA") do (
    findstr /s /i /r /c:"VIP.*%%~N" "%PROJET%\Plugins\ChellesWraith\*.txt" "%PROJET%\Plugins\ChellesMenu\*.txt" >nul 2>nul
    if errorlevel 1 (
        echo  ATTENTION : %%~N n'est pas encore marque VIP.
        set "VIPOK=0"
    )
)
findstr /i /r /c:"^CODE=2468" "%PROJET%\Plugins\ChellesMenu\VIP.txt" >nul 2>nul
if errorlevel 1 (
    echo  ATTENTION : Plugins\ChellesMenu\VIP.txt ne contient pas CODE=2468.
    set "VIPOK=0"
)
if "%VIPOK%"=="0" (
    echo.
    echo  Le verrou VIP n'a pas l'air complet : ces persos seraient LIBRES dans la demo.
    choice /c ON /m " Continuer quand meme"
    if errorlevel 2 exit /b 1
) else (
    echo  Verrou VIP present pour les 5 persos VIP.
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0preparer_demo4.ps1" -Projet "%PROJET%"
if errorlevel 1 (
    echo  Impossible de regler DefaultGame.ini. Abandon.
    pause
    exit /b 1
)
if not exist "%SORTIE%" mkdir "%SORTIE%"

for /f %%G in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "STAMP=%%G"
set "JOURNAL=%~dp0Journal_DEMO4_%STAMP%"
mkdir "%JOURNAL%"
set "ATLOGS=%APPDATA%\Unreal Engine\AutomationTool\Logs"
if exist "%ATLOGS%" ren "%ATLOGS%" "Logs_avant_%STAMP%"

echo.
echo  Debut : %date% %time%   (compte environ 1 heure, ne ferme pas cette fenetre)
call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%UPROJECT%" -noP4 -platform=Win64 -clientconfig=Shipping -build -nocompileeditor -cook -map=/Game/Maps/Rachid -stage -pak -compressed -prereqs -nodebuginfo -archive -archivedirectory="%SORTIE%" -unattended -utf8output
set "RESULTAT=%errorlevel%"
echo  Fin : %date% %time%

xcopy /e /i /y "%ATLOGS%" "%JOURNAL%" >nul 2>nul
for /f "delims=" %%F in ('dir /b /s "%JOURNAL%\*.txt" 2^>nul') do findstr /i /c:"Error:" /c:": error" /c:"Fatal" "%%F" >>"%JOURNAL%\Erreurs.txt" 2>nul

if not "%RESULTAT%"=="0" (
    echo  ===== ECHEC code %RESULTAT% - envoie a Claude le fichier %JOURNAL%\Erreurs.txt
    pause
    exit /b %RESULTAT%
)

set "JEU=%SORTIE%\WindowsNoEditor"
if not exist "%JEU%" (
    echo  Dossier du jeu introuvable : "%JEU%"
    pause
    exit /b 1
)

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
    echo CHELLES 3D - DEMO 4 ^(beta v0.4.0^)
    echo ==============================
    echo.
    echo Pour jouer : double-cliquez sur %EXE%
    echo La demo dure 40 minutes de jeu ^(le temps passe dans les menus ne compte pas^).
    echo.
    echo Personnages VIP ^(cartes dorees du lobby^) : ils se debloquent avec le code VIP.
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
echo  Jeu pret : %JEU%\%EXE%
echo  Creation du ZIP %SORTIE%\CHELLES3D_DEMO4.zip (quelques minutes)...
if exist "%SORTIE%\CHELLES3D_DEMO4.zip" ren "%SORTIE%\CHELLES3D_DEMO4.zip" "CHELLES3D_DEMO4_avant_%STAMP%.zip"
pushd "%SORTIE%"
tar -a -c -f CHELLES3D_DEMO4.zip WindowsNoEditor
popd
if exist "%SORTIE%\CHELLES3D_DEMO4.zip" (echo  ZIP pret : %SORTIE%\CHELLES3D_DEMO4.zip) else (echo  ZIP non cree : le jeu reste jouable dans %JEU%)
echo.
echo  ===== TERMINE =====
> "%~dp0Derniere_demo.txt" echo DEMO 4 OK %date% %time% : %JEU%\%EXE%
pause
exit /b 0
