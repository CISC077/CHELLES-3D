# Appele par SAUVEGARDER.bat.
#  -Etape Verifier : cherche la sauvegarde precedente sur le meme disque,
#                    calcule la place necessaire et la place libre.
#  -Etape Lier     : dans la nouvelle sauvegarde, cree des "liens durs"
#                    (hard links NTFS) vers les fichiers de la sauvegarde
#                    precedente qui n'ont PAS change. Ils ne prennent pas
#                    de place en plus. Robocopy copie ensuite le reste.
param([string]$Projet, [string]$Destination, [string]$Etape = 'Verifier')

$dossiers = 'Content','Config','Plugins','Source','Binaries','Build','Tools'
$Projet = $Projet.TrimEnd('\')
$Destination = $Destination.TrimEnd('\')

function Get-FichiersProjet {
    Get-ChildItem -LiteralPath $Projet -File -Force -ErrorAction SilentlyContinue
    foreach ($d in $dossiers) {
        $p = Join-Path $Projet $d
        if (Test-Path -LiteralPath $p) {
            Get-ChildItem -LiteralPath $p -Recurse -File -Force -ErrorAction SilentlyContinue |
                Where-Object { $_.FullName -notmatch '\\Plugins\\[^\\]+\\Intermediate\\' }
        }
    }
}

function Get-Precedente {
    $parent = Split-Path -Parent $Destination
    $lecteur = [System.IO.Path]::GetPathRoot($Destination)
    $fmt = (New-Object System.IO.DriveInfo($lecteur)).DriveFormat
    if ($fmt -ne 'NTFS') { return $null }
    $c = Get-ChildItem -LiteralPath $parent -Directory -Filter 'CHELLES 3D_*' -ErrorAction SilentlyContinue |
        Where-Object {
            $_.FullName -ne $Destination -and
            (Test-Path -LiteralPath (Join-Path $_.FullName 'SAUVEGARDE_INFO.txt')) -and
            -not (Test-Path -LiteralPath (Join-Path $_.FullName 'SAUVEGARDE_INCOMPLETE.txt'))
        } |
        Sort-Object { (Get-Item -LiteralPath (Join-Path $_.FullName 'SAUVEGARDE_INFO.txt')).LastWriteTimeUtc } -Descending |
        Select-Object -First 1
    if ($c) { return $c.FullName } else { return $null }
}

function Est-Identique($f, $prec) {
    $rel = $f.FullName.Substring($Projet.Length)
    $q = $prec + $rel
    $fi = New-Object System.IO.FileInfo($q)
    if (-not $fi.Exists) { return $null }
    if ($fi.Length -ne $f.Length) { return $null }
    if ($fi.LastWriteTimeUtc -ne $f.LastWriteTimeUtc) { return $null }
    return $rel
}

$prec = Get-Precedente

if ($Etape -eq 'Verifier') {
    Write-Host "Calcul de la taille du projet (cela peut prendre une minute)..."
    [double]$total = 0; [double]$nouveau = 0
    foreach ($f in Get-FichiersProjet) {
        $total += $f.Length
        if (-not $prec -or -not (Est-Identique $f $prec)) { $nouveau += $f.Length }
    }
    $lecteur = [System.IO.Path]::GetPathRoot($Destination)
    $libre = (New-Object System.IO.DriveInfo($lecteur)).AvailableFreeSpace
    Write-Host ""
    if ($prec) {
        Write-Host ("Sauvegarde precedente trouvee : " + (Split-Path -Leaf $prec))
        Write-Host "Seuls les fichiers modifies depuis seront recopies."
    } else {
        Write-Host "Aucune sauvegarde precedente sur ce disque : copie complete."
    }
    Write-Host ("Taille du projet        : {0:N1} Go" -f ($total / 1GB))
    Write-Host ("Place a utiliser        : {0:N1} Go" -f ($nouveau / 1GB))
    Write-Host ("Place libre sur {0}     : {1:N1} Go" -f $lecteur, ($libre / 1GB))
    if ($libre -lt ($nouveau + 2GB)) { exit 1 }
    exit 0
}

if ($Etape -eq 'Lier') {
    if (-not $prec) { exit 0 }
    Add-Type -Namespace Chelles -Name Lien -MemberDefinition @'
[DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
public static extern bool CreateHardLink(string lpFileName, string lpExistingFileName, IntPtr lpSecurityAttributes);
'@
    Write-Host ("Reprise des fichiers identiques depuis " + (Split-Path -Leaf $prec) + " ...")
    $n = 0; $rates = 0
    foreach ($f in Get-FichiersProjet) {
        $rel = Est-Identique $f $prec
        if (-not $rel) { continue }
        $cible = $Destination + $rel
        $dir = Split-Path -Parent $cible
        if (-not (Test-Path -LiteralPath $dir)) { [void][System.IO.Directory]::CreateDirectory($dir) }
        if (Test-Path -LiteralPath $cible) { continue }
        if ([Chelles.Lien]::CreateHardLink('\\?\' + $cible, '\\?\' + $prec + $rel, [IntPtr]::Zero)) { $n++ } else { $rates++ }
    }
    Write-Host ("{0} fichiers repris sans recopie ({1} seront recopies normalement)." -f $n, $rates)
    exit 0
}
