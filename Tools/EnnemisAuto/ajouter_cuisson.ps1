# CHELLES 3D - Ajoute dans Config\DefaultGame.ini les dossiers que le plugin ChellesEnnemis charge
# pendant le jeu, pour qu'ils soient aussi dans le jeu exporte (demo, jeu complet, serveur).
# Une copie de DefaultGame.ini est faite avant. Rien n'est ajoute deux fois.
param([string]$Projet)

$ini = Join-Path $Projet "Config\DefaultGame.ini"
if (-not (Test-Path $ini)) { Write-Host "DefaultGame.ini introuvable : $ini"; exit 1 }

$date = Get-Date -Format "yyyy-MM-dd_HHmm"
Copy-Item $ini "$ini.avant_ennemis_$date"

$dossiers = @(
    "/Game/FPS_Game/Blueprints/Bonus/AI",
    "/Game/Bandit_Sport/Mesh",
    "/Game/Bandit_Sport/Material"
)
$lignes = [System.Collections.Generic.List[string]](Get-Content $ini)
$section = "[/Script/UnrealEd.ProjectPackagingSettings]"
if (-not ($lignes -contains $section)) { $lignes.Add(""); $lignes.Add($section) }

$ajouts = 0
foreach ($d in $dossiers) {
    $ligne = '+DirectoriesToAlwaysCook=(Path="' + $d + '")'
    if ($lignes -contains $ligne) { continue }
    # juste apres la derniere ligne DirectoriesToAlwaysCook, sinon juste apres la section
    $pos = -1
    for ($i = 0; $i -lt $lignes.Count; $i++) { if ($lignes[$i].StartsWith("+DirectoriesToAlwaysCook=")) { $pos = $i } }
    if ($pos -lt 0) { $pos = $lignes.IndexOf($section) }
    $lignes.Insert($pos + 1, $ligne)
    $ajouts++
}
Set-Content -Path $ini -Value $lignes -Encoding ASCII
Write-Host "DefaultGame.ini : $ajouts dossier(s) ajoute(s) a la liste d'export (copie : DefaultGame.ini.avant_ennemis_$date)."
