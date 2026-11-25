# CHELLES 3D - Demo 4 (2026-11-25) : regle Config\DefaultGame.ini pour la demo de 40 minutes.
# Sauvegarde d'abord l'original en Tools\Demo\DefaultGame.ini.avant_demo4 (une seule fois).
param([string]$Projet)
$ErrorActionPreference = 'Stop'
$ini = Join-Path $Projet 'Config\DefaultGame.ini'
$copie = Join-Path $PSScriptRoot 'DefaultGame.ini.avant_demo4'
if (-not (Test-Path $copie)) { Copy-Item $ini $copie }
$t = [IO.File]::ReadAllText($ini)
$t = $t -replace '(?m)^Active=.*$', 'Active=True'
$t = $t -replace '(?m)^DureeMinutes=.*$', 'DureeMinutes=40'
$t = $t -replace '(?m)^ProjectVersion=.*$', 'ProjectVersion=0.4.0-beta'
$t = $t -replace 'Demo beta"\)', 'Demo 4 beta")'
# Ligne cassee trouvee le 25/11 : parenthese fermante manquante
$t = $t -replace '\(Path="/Game/CraftResourcesIcons"\s*$', "(Path=""/Game/CraftResourcesIcons"")`r`n"
$t = $t -replace '\(Path="/Game/CraftResourcesIcons"\r?\n', "(Path=""/Game/CraftResourcesIcons"")`r`n"
$enc = New-Object System.Text.UTF8Encoding($true)
[IO.File]::WriteAllText($ini, $t, $enc)
if ($t -notmatch '(?m)^DureeMinutes=40\s*$') { Write-Host 'DureeMinutes=40 introuvable dans DefaultGame.ini'; exit 1 }
Write-Host 'DefaultGame.ini regle : demo de 40 minutes, version 0.4.0-beta.'
exit 0
