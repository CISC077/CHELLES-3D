# =====================================================================
#  CHELLES 3D - Defis de la semaine : prepare le Discord
#  Lance par ENVOYER_LES_DEFIS_SUR_LE_VPS.bat. Peut etre relance sans risque.
#   1. cree le salon  #défis-de-la-semaine  dans la categorie CHELLES 3D (s'il manque)
#   2. cree le webhook "Défis CHELLES 3D" de ce salon (s'il manque)
#   3. ecrit son adresse dans serveur-defis\discord.env, que le .bat envoie au VPS
#      puis EFFACE du PC. L'adresse n'est jamais affichee.
#  Le token du bot est lu dans D:\SITE CHELLES3D\DISCORD\token.txt (jamais affiche).
#  Plan B sans bot : collez l'adresse d'un webhook dans serveur-defis\webhook.txt
# =====================================================================
$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$Dossier  = Split-Path -Parent $MyInvocation.MyCommand.Path
$Sortie   = Join-Path $Dossier 'serveur-defis\discord.env'
$PlanB    = Join-Path $Dossier 'serveur-defis\webhook.txt'
$GuildId  = '1568892043680288768'
$Api      = 'https://discord.com/api/v10'
$NomSalon = 'défis-de-la-semaine'
$NomCat   = '🏙️ CHELLES 3D'
$NomHook  = 'Défis CHELLES 3D'
$Modele   = '^https://(discord|discordapp)\.com/api/webhooks/\d+/[\w-]+$'

function Dire($texte, $couleur = 'White') { Write-Host $texte -ForegroundColor $couleur }
function Ecrire($lignes) {
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Sortie, (($lignes -join "`n") + "`n"), $utf8)
}

# ---------- Plan B : adresse collee a la main ----------
if (Test-Path $PlanB) {
    $url = (Get-Content $PlanB -Raw).Trim()
    if ($url -match $Modele) {
        Ecrire @("DISCORD_WEBHOOK=$url")
        Remove-Item $PlanB -Force
        Dire "Webhook pris dans webhook.txt (fichier efface du PC)." Green
        exit 0
    }
    Dire "webhook.txt ne contient pas une adresse de webhook Discord valable : on essaie avec le bot." DarkYellow
}

# ---------- Token du bot (jamais affiche) ----------
$FichierToken = Join-Path (Split-Path -Parent $Dossier) 'DISCORD\token.txt'
if (-not (Test-Path $FichierToken)) { Dire "Fichier introuvable : $FichierToken" Red; exit 1 }
$Token = (Get-Content $FichierToken -Raw).Trim()
if ($Token.Length -lt 50 -or $Token -like '*COLLE*') { Dire "token.txt ne contient pas le token du bot." Red; exit 1 }
$Entetes = @{ Authorization = "Bot $Token"; 'User-Agent' = 'DiscordBot (chelles3d, 1.0)' }

function Appel($methode, $chemin, $corps = $null) {
    for ($essai = 1; $essai -le 6; $essai++) {
        try {
            $p = @{ Method = $methode; Uri = "$Api$chemin"; Headers = $Entetes; ContentType = 'application/json; charset=utf-8' }
            if ($null -ne $corps) {
                $json = ConvertTo-Json -InputObject $corps -Depth 20 -Compress
                $p.Body = [System.Text.Encoding]::UTF8.GetBytes($json)
            }
            $res = Invoke-RestMethod @p
            return $res
        } catch {
            $code = 0
            if ($_.Exception.Response) { $code = [int]$_.Exception.Response.StatusCode }
            if ($code -eq 429) { Start-Sleep -Seconds (2 * $essai); continue }
            $detail = $_.ErrorDetails.Message
            if (-not $detail) { $detail = $_.Exception.Message }
            throw "Erreur $code sur $methode $chemin : $detail"
        }
    }
    throw "Discord limite trop les appels, relance dans une minute."
}

try {
    $Moi = Appel GET '/users/@me'
    Dire "Bot connecte : $($Moi.username)" Green
    try { $null = Appel GET "/guilds/$GuildId" }
    catch {
        $liste = Appel GET '/users/@me/guilds'
        if (-not $liste -or @($liste).Count -eq 0) { Dire "Le bot n'est sur aucun serveur." Red; exit 1 }
        $GuildId = @($liste)[0].id
    }
    $Salons = @(Appel GET "/guilds/$GuildId/channels")

    # ---------- 1. Salon ----------
    $salon = $Salons | Where-Object { $_.type -eq 0 -and $_.name -eq $NomSalon } | Select-Object -First 1
    if ($salon) { Dire "Salon deja la : #$NomSalon" DarkGray }
    else {
        $cat = $Salons | Where-Object { $_.type -eq 4 -and $_.name -eq $NomCat } | Select-Object -First 1
        if (-not $cat) { $cat = $Salons | Where-Object { $_.type -eq 4 -and $_.name -like '*CHELLES 3D*' } | Select-Object -First 1 }
        # Lecture seule pour tout le monde (@everyone = id du serveur) : seuls les annonces et le staff y ecrivent
        $corps = @{ name = $NomSalon; type = 0; topic = 'Un nouveau défi chaque lundi à 18 h : réussis-le dans le jeu et gagne de l XP !'
                    permission_overwrites = @(@{ id = "$GuildId"; type = 0; allow = '0'; deny = '2048' }) }
        if ($cat) { $corps.parent_id = "$($cat.id)" }
        $salon = Appel POST "/guilds/$GuildId/channels" $corps
        Dire "Salon cree : #$NomSalon" Green
    }

    # ---------- 2. Webhook ----------
    $hooks = @(Appel GET "/channels/$($salon.id)/webhooks")
    $hook = $hooks | Where-Object { $_.name -eq $NomHook -and $_.token } | Select-Object -First 1
    if ($hook) { Dire "Webhook deja la : $NomHook" DarkGray }
    else {
        $hook = Appel POST "/channels/$($salon.id)/webhooks" @{ name = $NomHook }
        Dire "Webhook cree : $NomHook" Green
    }
    $url = "https://discord.com/api/webhooks/$($hook.id)/$($hook.token)"
    if ($url -notmatch $Modele) { Dire "Adresse du webhook inattendue." Red; exit 1 }

    # ---------- 3. Fichier pour le VPS ----------
    $lignes = @("DISCORD_WEBHOOK=$url")
    $captures = $Salons | Where-Object { $_.type -eq 0 -and $_.name -eq 'captures-écran' } | Select-Object -First 1
    if ($captures) { $lignes += "SALON_PREUVES=$($captures.id)" }
    Ecrire $lignes
    Dire "Discord pret (l'adresse du webhook n'est pas affichee)." Green
    exit 0
} catch {
    Dire "Discord : $_" Red
    Dire "Le bot n'a peut-etre pas la permission 'Gerer les webhooks'. Plan B : voir LISEZMOI_DEFIS.txt" Yellow
    exit 1
}
