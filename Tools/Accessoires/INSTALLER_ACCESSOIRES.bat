@echo off
rem ------------------------------------------------------------------
rem  CHELLES 3D - Installe les ACCESSOIRES (lampe, jumelles, grappin, kit de soin)
rem   1) ajoute le pack d'icones Craft Resources Icons au jeu exporte (DefaultGame.ini)
rem   2) compile le plugin ChellesAccessoires
rem  Fermez Unreal avant de lancer ce fichier (double-clic).
rem ------------------------------------------------------------------
setlocal
set "PROJET=%~dp0..\.."
set "INI=%PROJET%\Config\DefaultGame.ini"

if exist "%INI%" (
    findstr /i /c:"/Game/CraftResourcesIcons" "%INI%" >nul
    if errorlevel 1 (
        copy /y "%INI%" "%INI%.avant_accessoires" >nul
        >>"%INI%" echo.
        >>"%INI%" echo [/Script/UnrealEd.ProjectPackagingSettings]
        >>"%INI%" echo +DirectoriesToAlwaysCook=(Path="/Game/CraftResourcesIcons")
        echo Icones ajoutees au jeu exporte. Copie de securite : DefaultGame.ini.avant_accessoires
    ) else (
        echo Icones deja dans DefaultGame.ini.
    )
)

call "%PROJET%\Plugins\ChellesAccessoires\Compiler_le_plugin.bat"
