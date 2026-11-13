@echo off
rem ------------------------------------------------------------------
rem  CHELLES 3D - Etape 1 : prepare Unreal 4.27 "compile depuis les sources".
rem  Le Unreal du Launcher NE SAIT PAS fabriquer un serveur dedie : il faut
rem  cette version speciale d'Unreal (une seule fois, 3 a 6 heures).
rem  Elle ne remplace pas le Unreal du Launcher : vous continuez a travailler
rem  avec le Launcher, celle-ci sert seulement a fabriquer le serveur.
rem
rem  AVANT de lancer ce fichier (voir LISEZMOI.md, etapes 1 a 3) :
rem   - le code source 4.27 telecharge et DEZIPPE (ex: D:\UE_4.27_Source)
rem   - la "toolchain Linux" v19_clang-11.0.1-centos7 installee
rem   - environ 150 Go libres sur le disque du code source
rem ------------------------------------------------------------------
setlocal EnableExtensions
title CHELLES 3D - Preparation d'Unreal 4.27 (sources)

set "ICI=%~dp0"
set "MEMO=%ICI%chemin_unreal_source.txt"
set "SRC=D:\UE_4.27_Source"
if exist "%MEMO%" set /p SRC=<"%MEMO%"

echo.
echo Dossier du code source d'Unreal 4.27 (celui qui contient Setup.bat) :
echo   %SRC%
set "AUTRE="
set /p "AUTRE=Entree pour accepter, ou collez un autre dossier : "
if not "%AUTRE%"=="" set "SRC=%AUTRE%"
if "%SRC:~-1%"=="\" set "SRC=%SRC:~0,-1%"

if not exist "%SRC%\Setup.bat" (
    echo.
    echo Setup.bat introuvable dans "%SRC%".
    echo Avez-vous bien dezippe UnrealEngine-4.27.zip ? Le bon dossier contient
    echo Setup.bat, GenerateProjectFiles.bat et le dossier Engine.
    echo Astuce : si vous voyez un dossier "UnrealEngine-4.27" dedans, c'est lui le bon.
    pause
    exit /b 1
)
echo %SRC%> "%MEMO%"

rem ---- Toolchain Linux (indispensable pour fabriquer un serveur Linux depuis Windows)
if "%LINUX_MULTIARCH_ROOT%"=="" (
    echo.
    echo La toolchain Linux n'est pas installee ^(variable LINUX_MULTIARCH_ROOT vide^).
    echo Installez v19_clang-11.0.1-centos7.exe ^(lien dans LISEZMOI.md, etape 2^),
    echo puis FERMEZ cette fenetre et relancez ce fichier.
    pause
    exit /b 1
)
if not exist "%LINUX_MULTIARCH_ROOT%\x86_64-unknown-linux-gnu\bin" (
    echo.
    echo La toolchain Linux est declaree mais introuvable : "%LINUX_MULTIARCH_ROOT%"
    echo Reinstallez v19_clang-11.0.1-centos7.exe puis relancez ce fichier.
    pause
    exit /b 1
)
echo Toolchain Linux : %LINUX_MULTIARCH_ROOT%   OK

rem ---- Place libre
set "DISQUE=%SRC:~0,1%"
set "LIBRE=?"
for /f %%G in ('powershell -NoProfile -Command "[math]::Floor((Get-PSDrive %DISQUE%).Free/1GB)"') do set "LIBRE=%%G"
echo Place libre sur %DISQUE%: = %LIBRE% Go ^(il en faut environ 150^)
set "OK="
set /p "OK=Continuer ? (O/N) : "
if /i not "%OK%"=="O" exit /b 0

set "JOURNAL=%SRC%\Engine\Programs\UnrealBuildTool\Log.txt"

echo.
echo ===== 1/3 : telechargement des fichiers d'Unreal (environ 15 Go, 30 min a 2 h)
echo Si une fenetre demande d'installer des "prerequis", acceptez.
pushd "%SRC%"
call Setup.bat
if errorlevel 1 (
    popd
    echo ECHEC de Setup.bat. Faites une capture de cette fenetre et envoyez-la a Claude.
    pause
    exit /b 1
)

echo.
echo ===== 2/3 : preparation des fichiers de compilation (quelques minutes)
call GenerateProjectFiles.bat
if errorlevel 1 (
    popd
    echo ECHEC de GenerateProjectFiles.bat. Souvent : il manque un morceau de Visual Studio
    echo ^(voir LISEZMOI.md, etape 1^). Envoyez une capture de cette fenetre a Claude.
    pause
    exit /b 1
)

echo.
echo ===== 3/3 : compilation d'Unreal (2 a 5 heures, ne fermez pas cette fenetre)
echo Debut : %date% %time%
set "BUILD=Engine\Build\BatchFiles\Build.bat"
for %%T in (UE4Editor ShaderCompileWorker UnrealPak UnrealLightmass CrashReportClient) do (
    echo   - %%T ...
    call "%BUILD%" %%T Win64 Development -WaitMutex
    if errorlevel 1 (
        if /i "%%T"=="UE4Editor" (
            popd
            echo ECHEC de la compilation de %%T. Envoyez a Claude le fichier : %JOURNAL%
            pause
            exit /b 1
        )
        echo     ^(%%T a echoue, pas grave pour le serveur^)
    )
)
popd
echo Fin : %date% %time%

echo.
echo TERMINE. Unreal 4.27 ^(sources^) est pret dans :
echo   %SRC%
echo Etape suivante : double-clic sur 2_CREER_LE_SERVEUR.bat
pause
