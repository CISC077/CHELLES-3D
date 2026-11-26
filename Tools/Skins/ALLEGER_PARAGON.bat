@echo off
title CHELLES 3D - Alleger les heros Paragon
rem =====================================================================
rem  ALLEGER LE JEU (26/11/2026) - UN SEUL FICHIER A LANCER, Unreal ferme
rem  Choix de Rachid : garder seulement WUKONG et les heroines Paragon
rem  (Aurora, Countess, Phase, Shinbi, Terra, Yin, Zinx).
rem  Retires : Boris, Dekker, Greystone, Iggy Scorch, Kwang, Steel, Wraith
rem  + les "statues" sans os inutilisees des persos Mixamo (base, base_basic_pbr,
rem    base_basic_shaded de LENA, SOFIA, LUCIA, BRENDA, Shems, Rebecca).
rem  Un heros utilise ailleurs dans le jeu (carte, blueprint) est GARDE.
rem  Rien n'est efface : tout part dans _Sauvegardes\paragon_retires.
rem  Pour revenir en arriere : RESTAURER_PARAGON.bat
rem =====================================================================
setlocal EnableDelayedExpansion
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
set "UE=C:\Program Files\Epic Games\UE_4.27"
set "CONTENT=%PROJET%\Content"
set "SAUVE=%PROJET%\_Sauvegardes\paragon_retires"
set "LISTE=%PROJET%\Tools\out\paragon_a_retirer.txt"
if not exist "%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" (
    echo Unreal 4.27 introuvable dans "%UE%".
    pause
    exit /b 1
)
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
    echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
    pause
    exit /b 1
)
echo.
echo GARDES  : Wukong, Aurora, Countess, Phase, Shinbi, Terra, Yin, Zinx
echo RETIRES : Boris, Dekker, Greystone, Iggy Scorch, Kwang, Steel, Wraith
echo           + statues inutilisees des persos Mixamo
echo Tout est mis de cote dans _Sauvegardes\paragon_retires (rien n'est efface).
echo.
choice /c ON /m "Continuer (O = oui, N = non)"
if errorlevel 2 exit /b 0

if not exist "%PROJET%\Tools\out" mkdir "%PROJET%\Tools\out"
if exist "%LISTE%" del "%LISTE%"
echo.
echo ===== ETAPE 1/2 : qui utilise ces heros ? (3 a 15 minutes) =====
copy /y "%PROJET%\Tools\Skins\analyser_paragon.py" "%TEMP%\chelles_paragon.py" >nul
"%UE%\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%PROJET%\FPS_Chelles.uproject" -run=pythonscript -script="%TEMP%\chelles_paragon.py" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nosplash -stdout > "%PROJET%\Tools\out\paragon_unreal.log" 2>&1
if not exist "%LISTE%" (
    echo L'analyse n'a pas abouti : rien n'a ete deplace.
    echo Envoyez a Claude le fichier Tools\out\paragon_unreal.log
    pause
    exit /b 1
)
type "%LISTE%" | findstr /b "#"

echo.
echo ===== ETAPE 2/2 : mise de cote =====
if not exist "%SAUVE%" mkdir "%SAUVE%"
set "N=0"
for /f "usebackq tokens=1,*" %%A in ("%LISTE%") do (
    if "%%A"=="OK" (
        if exist "%CONTENT%\%%B" (
            if exist "%SAUVE%\%%B" (
                echo %%B est deja dans la sauvegarde : laisse en place.
            ) else (
                move "%CONTENT%\%%B" "%SAUVE%\" >nul && (
                    echo DOSSIER %%B>>"%SAUVE%\liste.txt"
                    echo   %%B mis de cote
                    set /a N+=1
                )
            )
        )
    )
    if "%%A"=="FICHIER" (
        if exist "%CONTENT%\%%B" (
            for %%D in ("%SAUVE%\%%B") do if not exist "%%~dpD" mkdir "%%~dpD"
            move "%CONTENT%\%%B" "%SAUVE%\%%B" >nul && (
                echo FICHIER %%B>>"%SAUVE%\liste.txt"
                echo   %%B mis de cote
                set /a N+=1
            )
        )
    )
)
echo.
echo TERMINE : !N! element^(s^) mis de cote. Vous pouvez ouvrir CHELLES 3D.
echo Pour tout remettre : Tools\Skins\RESTAURER_PARAGON.bat
pause
exit /b 0
