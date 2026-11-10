@echo off
echo.
echo  Applique les corrections sures + les PROPOSITIONS "A VERIFIER" du rapport
echo  (redirections vers un fichier du meme nom trouve ailleurs dans le projet).
echo  Une copie du DefaultEngine.ini est faite avant. Rien n'est supprime.
echo.
set /p REP=  Tape OUI puis Entree pour continuer : 
if /i not "%REP%"=="OUI" (
  echo  Annule.
  pause
  exit /b 0
)
call "%~dp0Verifier_maintenant.bat" propositions
