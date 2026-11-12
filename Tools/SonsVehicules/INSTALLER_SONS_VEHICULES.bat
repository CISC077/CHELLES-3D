@echo off
rem ------------------------------------------------------------------
rem  CHELLES 3D - Sons des vehicules (moteur, acceleration, klaxon)
rem  Compile 2 plugins : ChellesSonsVehicules (nouveau) et ChellesMenu
rem  (curseur VEHICULES ajoute dans OPTIONS > AUDIO).
rem  Fermez Unreal avant de lancer ce fichier (double-clic).
rem  Appuyez sur une touche quand chaque compilation dit TERMINE.
rem ------------------------------------------------------------------
setlocal
set "PROJET=%~dp0..\.."

echo.
echo ===== 1/2 : plugin ChellesSonsVehicules =====
call "%PROJET%\Plugins\ChellesSonsVehicules\Compiler_le_plugin.bat"

echo.
echo ===== 2/2 : plugin ChellesMenu (curseur VEHICULES) =====
call "%PROJET%\Plugins\ChellesMenu\Compiler_le_plugin.bat"

echo.
echo Fini. Ouvrez CHELLES 3D, lancez Play, montez dans un vehicule et appuyez sur H.
pause
