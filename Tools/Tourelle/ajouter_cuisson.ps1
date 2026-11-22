# CHELLES 3D - Ajoute /Game/ScifiGun dans la liste d'export (Config\DefaultGame.ini) :
# la tourelle BP_Turet2026 est chargee par le jeu pendant la partie, elle doit etre dans le jeu exporte
# (demo, jeu complet, serveur). Une copie de DefaultGame.ini est faite avant. Rien n'est ajoute deux fois.
param([string]$Projet)

$ini = Join-Path $Projet "Config\DefaultGame.ini"
if (-not (Test-Path $ini)) { Write-Host "DefaultGame.ini introuvable : $ini"; exit 1 }

$ligne = '+DirectoriesToAlwaysCook=(Path="/Game/ScifiGun")'
$lignes = [System.Collections.Generic.List[string]](Get-Content $ini)
if ($lignes -contains $ligne) { Write-Host "DefaultGame.ini : /Game/ScifiGun deja dans la liste d'export."; exit 0 }

$date = Get-Date -Format "yyyy-MM-dd_HHmm"
Copy-Item $ini "$ini.avant_tourelle_$date"

$section = "[/Script/UnrealEd.ProjectPackagingSettings]"
if (-not ($lignes -contains $section)) { $lignes.Add(""); $lignes.Add($section) }
$pos = -1
for ($i = 0; $i -lt $lignes.Count; $i++) { if ($lignes[$i].StartsWith("+DirectoriesToAlwaysCook=")) { $pos = $i } }
if ($pos -lt 0) { $pos = $lignes.IndexOf($section) }
$lignes.Insert($pos + 1, $ligne)
Set-Content -Path $ini -Value $lignes -Encoding ASCII
Write-Host "DefaultGame.ini : /Game/ScifiGun ajoute a la liste d'export (copie : DefaultGame.ini.avant_tourelle_$date)."
