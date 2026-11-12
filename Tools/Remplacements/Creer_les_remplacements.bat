@echo off
rem Cree les materiaux de remplacement (/Game/Remplacements) pour les packs absents.
rem UNREAL DOIT ETRE FERME. Rien n'est supprime.
set "PROJET=%~dp0..\.."
for %%I in ("%PROJET%") do set "PROJET=%%~fI"
tasklist /FI "IMAGENAME eq UE4Editor.exe" | find /I "UE4Editor.exe" >nul
if not errorlevel 1 (
  echo Unreal est encore ouvert. Ferme-le puis relance ce fichier.
  pause
  exit /b 1
)
rem Deux grosses textures de ModularFences a recopier depuis la quarantaine (copie, la quarantaine garde les siennes)
for %%F in (Tex_Decorative_Brick_D Tex_Decorative_Brick_N) do (
  if not exist "%PROJET%\Content\ModularFences\Textures\%%F.uasset" copy /Y "%PROJET%\_Quarantaine\Content\ModularFences\Textures\%%F.uasset" "%PROJET%\Content\ModularFences\Textures\%%F.uasset" >nul
)
echo Creation des materiaux de remplacement, patiente 1 a 3 minutes...
"C:\Program Files\Epic Games\UE_4.27\Engine\Binaries\Win64\UE4Editor-Cmd.exe" "%PROJET%\FPS_Chelles.uproject" -run=pythonscript -script="%PROJET%\Tools\Remplacements\creer_remplacements.py" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nosplash -stdout > "%PROJET%\Tools\Remplacements\remplacements.log" 2>&1
echo.
echo ===== Resultat =====
findstr /C:"[Remplacements]" "%PROJET%\Tools\Remplacements\remplacements.log"
echo.
echo Si tu vois "TERMINE", ouvre Unreal : les erreurs doivent avoir disparu.
echo Journal complet : Tools\Remplacements\remplacements.log
pause
