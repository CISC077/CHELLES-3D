@echo off
rem ------------------------------------------------------------------
rem  Fabrique les voix francaises des passants (fichiers .wav) a partir
rem  de Repliques.txt, avec les voix installees dans Windows.
rem  A relancer apres chaque modification de Repliques.txt.
rem  Unreal peut rester ouvert, mais relancez la partie (Play) ensuite.
rem ------------------------------------------------------------------
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Outils\Generer_les_voix.ps1"
pause
