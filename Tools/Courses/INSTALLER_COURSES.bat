@echo off
chcp 65001 >nul
title CHELLES 3D - Installation des COURSES DE RUE
rem ------------------------------------------------------------------
rem  1) compile le plugin ChellesCourses (Unreal doit etre FERME)
rem  2) envoie le classement des courses sur le VPS (serveur du chat)
rem  Ne modifie aucun autre plugin. Double-clic pour lancer.
rem ------------------------------------------------------------------
setlocal
set "PROJET=%~dp0..\.."
set "PLUGIN=%PROJET%\Plugins\ChellesCourses"
set VPS=ubuntu@51.178.54.208

echo ==============================================================
echo   CHELLES 3D : installation des COURSES DE RUE
echo ==============================================================
echo.
if not exist "%PLUGIN%\ChellesCourses.uplugin" (
  echo [ERREUR] Plugin introuvable : "%PLUGIN%"
  pause
  exit /b 1
)
tasklist /fi "imagename eq UE4Editor.exe" | find /i "UE4Editor.exe" >nul
if not errorlevel 1 (
  echo Unreal est ouvert : fermez-le d'abord, puis relancez ce fichier.
  pause
  exit /b 1
)

echo Etape 1/2 : compilation du plugin ChellesCourses (1 a 5 minutes)...
echo   A la fin, appuyez sur une touche quand "TERMINE" s'affiche.
echo.
call "%PLUGIN%\Compiler_le_plugin.bat"
if not exist "%PLUGIN%\Binaries\Win64\UE4Editor-ChellesCourses.dll" (
  echo.
  echo [ERREUR] La compilation n'a pas marche : le journal est dans
  echo   "%PLUGIN%\Journal_compilation.txt"
  echo   Envoyez-le a Claude.
  pause
  exit /b 1
)

echo.
echo Etape 2/2 : classement de tous les joueurs sur le VPS...
where ssh >nul 2>nul || (echo [ATTENTION] ssh introuvable : le classement en ligne n'est pas installe, le jeu garde les records sur ce PC. & pause & exit /b 0)
ssh %VPS% "mkdir -p ~/serveur-courses"
scp "%~dp0VPS\courses.js" "%~dp0VPS\brancher_courses.js" "%~dp0VPS\installer_courses.sh" %VPS%:serveur-courses/
if errorlevel 1 (
  echo [ATTENTION] Envoi au VPS impossible. Les courses marchent quand meme, avec les records de ce PC.
  echo   Relancez ce fichier plus tard pour le classement en ligne.
  pause
  exit /b 0
)
ssh -t %VPS% "sed -i 's/\r$//' ~/serveur-courses/*.sh; sudo bash ~/serveur-courses/installer_courses.sh"

echo.
echo ==============================================================
echo   TERMINE. Ouvrez CHELLES 3D, lancez la carte Rachid :
echo   - colonnes cyan = departs des courses
echo   - touche DEBUT (Home) sur un depart = partir
echo   - touche DEBUT ailleurs = ecran COURSES (records, classement)
echo ==============================================================
pause
