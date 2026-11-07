# CHELLES 3D - Fabrique les voix francaises des passants a partir de Repliques.txt.
# Utilise les voix deja installees dans Windows (aucune cle API, aucun abonnement, pas d'Internet).
# Resultat : un fichier .wav par phrase et par voix dans le dossier Voix, plus Voix\index.txt.
# (Fichier volontairement sans accents : PowerShell 5 lit mal les accents dans les scripts.)

$ErrorActionPreference = 'Stop'
$Plugin = Split-Path -Parent $PSScriptRoot
$Repliques = Join-Path $Plugin 'Repliques.txt'
$DossierVoix = Join-Path $Plugin 'Voix'

if (-not (Test-Path $Repliques)) { Write-Host "Repliques.txt introuvable dans $Plugin"; exit 1 }

# --- Lecture des phrases --------------------------------------------------------------
$Phrases = @()
$Categorie = 'Salut'
foreach ($Ligne in (Get-Content -LiteralPath $Repliques -Encoding UTF8)) {
    $L = $Ligne.Trim()
    if ($L -eq '' -or $L.StartsWith('#')) { continue }
    if ($L.StartsWith('[') -and $L.EndsWith(']')) { $Categorie = $L.Substring(1, $L.Length - 2).Trim(); continue }
    $Phrases += [pscustomobject]@{ Categorie = $Categorie; Texte = $L }
}
Write-Host ("{0} phrases lues dans Repliques.txt" -f $Phrases.Count)

# Debit de parole par categorie (on parle plus vite quand on s'enerve).
function Debit($Cat) {
    switch ($Cat) { 'Bousculade' { 1.15 } 'Vehicule' { 1.3 } default { 1.0 } }
}

# --- On repart d'un dossier Voix vide --------------------------------------------------
if (Test-Path $DossierVoix) { Remove-Item -LiteralPath $DossierVoix -Recurse -Force }
New-Item -ItemType Directory -Path $DossierVoix | Out-Null
$Index = New-Object System.Collections.Generic.List[string]

function Nettoyer($Texte) { return ($Texte -replace '\|', ' ') }

# --- 1) Voix modernes de Windows 10/11 (Hortense, Julie, Paul...) ----------------------
$Fait = $false
try {
    Add-Type -AssemblyName System.Runtime.WindowsRuntime
    $AsTask = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {
        $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]
    [void][Windows.Media.SpeechSynthesis.SpeechSynthesizer, Windows.Media.SpeechSynthesis, ContentType=WindowsRuntime]
    $TypeFlux = [Windows.Media.SpeechSynthesis.SpeechSynthesisStream, Windows.Media.SpeechSynthesis, ContentType=WindowsRuntime]

    $Voix = @([Windows.Media.SpeechSynthesis.SpeechSynthesizer]::AllVoices | Where-Object { $_.Language -like 'fr*' })
    if ($Voix.Count -gt 0) {
        $Synthe = New-Object Windows.Media.SpeechSynthesis.SpeechSynthesizer
        for ($n = 0; $n -lt $Voix.Count; $n++) {
            $V = $Voix[$n]
            $Genre = if ("$($V.Gender)" -eq 'Female') { 'F' } else { 'M' }
            Write-Host ("Voix {0} : {1} ({2})" -f $n, $V.DisplayName, $Genre)
            $Synthe.Voice = $V
            $i = 0
            foreach ($P in $Phrases) {
                $i++
                try { $Synthe.Options.SpeakingRate = (Debit $P.Categorie) } catch { }
                $Tache = $AsTask.MakeGenericMethod($TypeFlux).Invoke($null, @($Synthe.SynthesizeTextToStreamAsync($P.Texte)))
                $Tache.Wait(-1) | Out-Null
                $Flux = $Tache.Result
                $Lecture = [System.IO.WindowsRuntimeStreamExtensions]::AsStreamForRead($Flux)
                $Nom = "{0}_{1}_{2:D3}.wav" -f $P.Categorie, $n, $i
                $Fichier = [System.IO.File]::Create((Join-Path $DossierVoix $Nom))
                $Lecture.CopyTo($Fichier)
                $Fichier.Close(); $Lecture.Close(); $Flux.Dispose()
                $Index.Add(("{0}|{1}|{2}|{3}|{4}" -f $P.Categorie, $Genre, $n, $Nom, (Nettoyer $P.Texte)))
            }
        }
        $Fait = $true
    }
} catch {
    Write-Host "Voix modernes indisponibles ($($_.Exception.Message)), essai avec les voix classiques..."
    $Index.Clear()
}

# --- 2) Sinon, voix classiques (SAPI) --------------------------------------------------
if (-not $Fait) {
    Add-Type -AssemblyName System.Speech
    $Synthe = New-Object System.Speech.Synthesis.SpeechSynthesizer
    $Voix = @($Synthe.GetInstalledVoices() | Where-Object { $_.Enabled -and $_.VoiceInfo.Culture.Name -like 'fr*' })
    if ($Voix.Count -eq 0) {
        Write-Host ""
        Write-Host "AUCUNE VOIX FRANCAISE dans Windows."
        Write-Host "Ajoutez-en une : Parametres > Heure et langue > Voix (ou Langue) > Francais > Ajouter des voix,"
        Write-Host "puis relancez Generer_les_voix.bat. En attendant, les phrases s'afficheront en texte dans le jeu."
        exit 1
    }
    $Format = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(22050, [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen, [System.Speech.AudioFormat.AudioChannel]::Mono)
    for ($n = 0; $n -lt $Voix.Count; $n++) {
        $Info = $Voix[$n].VoiceInfo
        $Genre = if ("$($Info.Gender)" -eq 'Female') { 'F' } else { 'M' }
        Write-Host ("Voix {0} : {1} ({2})" -f $n, $Info.Name, $Genre)
        $Synthe.SelectVoice($Info.Name)
        $i = 0
        foreach ($P in $Phrases) {
            $i++
            $Synthe.Rate = [int]((Debit $P.Categorie) * 10 - 10)
            $Nom = "{0}_{1}_{2:D3}.wav" -f $P.Categorie, $n, $i
            $Synthe.SetOutputToWaveFile((Join-Path $DossierVoix $Nom), $Format)
            $Synthe.Speak($P.Texte)
            $Synthe.SetOutputToNull()
            $Index.Add(("{0}|{1}|{2}|{3}|{4}" -f $P.Categorie, $Genre, $n, $Nom, (Nettoyer $P.Texte)))
        }
    }
}

[System.IO.File]::WriteAllLines((Join-Path $DossierVoix 'index.txt'), $Index, (New-Object System.Text.UTF8Encoding($true)))
Write-Host ""
Write-Host ("TERMINE : {0} enregistrements dans {1}" -f $Index.Count, $DossierVoix)
