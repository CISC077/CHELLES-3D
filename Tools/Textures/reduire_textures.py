"""A LANCER DANS UNREAL : File > Execute Python Script... > ce fichier (il demande Oui/Non),
OU, Unreal ferme : Textures_1_SIMULATION.bat (liste seulement) puis Textures_2_REDUIRE.bat.

REDUIT LES TEXTURES TROP GROSSES (4K, 8K...) utilisees par la carte Maps/Rachid.
C'est ce qui provoque l'alerte rouge "TEXTURE STREAMING POOL OVER ... GiB" et les ralentissements.

Comment ca marche :
  - le script cherche toutes les textures dont la carte Rachid a besoin (meme indirectement :
    batiments, materiaux, personnages, voitures, effets...) ;
  - il garde celles qui depassent LIMITE pixels (2048 par defaut) ;
  - en SIMULATION il affiche seulement le resultat (nombre de textures, gain estime) ;
  - en mode REDUIRE il regle "Maximum Texture Size" de chaque texture sur LIMITE, puis enregistre.

C'est SANS RISQUE : l'image d'origine reste dans le fichier, Unreal en fait seulement une copie plus petite
pour le jeu. Les anciennes valeurs sont notees dans Tools/Textures/journal_textures.json ;
restaurer_textures.py remet tout comme avant.

Les textures d'interface (menu, icones) ne sont pas touchees, pour que le menu reste net.
Duree : quelques minutes a une demi-heure la premiere fois (Unreal recompresse les textures).
Journal : Tools/out/textures.log
"""
import datetime, json, os, traceback
import unreal

# ----------------------------------------------------------------------------------------------- reglages
LIMITE = 2048                     # taille maxi en pixels (2048 = 2K). Mettre 1024 pour alleger encore plus.
CARTE = "/Game/Maps/Rachid"       # carte du jeu
DOSSIERS_EN_PLUS = []             # ex. ["/Game/Characters"] : dossiers a traiter en plus de ce qu'utilise la carte
GROUPES_IGNORES = ("UI", "PIXELS2D", "COLORLOOKUPTABLE", "HIERARCHICALLOD", "RENDERTARGET")

# ----------------------------------------------------------------------------------------------- chemins
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DOSSIER_OUTIL = os.path.join(PROJET, "Tools", "Textures")
JOURNAL_JSON = os.path.join(DOSSIER_OUTIL, "journal_textures.json")
DOSSIER_LOG = os.path.join(PROJET, "Tools", "out")
FICHIER_LOG = os.path.join(DOSSIER_LOG, "textures.log")

_lignes = []


def log(msg):
    unreal.log("[Textures] " + msg)
    _lignes.append(msg)


def ecrire_log():
    try:
        os.makedirs(DOSSIER_LOG, exist_ok=True)
        with open(FICHIER_LOG, "a", encoding="utf-8") as f:
            f.write("\n===== %s =====\n" % datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
            f.write("\n".join(_lignes) + "\n")
    except Exception:
        pass


# ----------------------------------------------------------------------------------------------- recherche
def paquets_de_la_carte():
    """Tous les paquets /Game dont la carte depend (dependances dures et souples, en cascade)."""
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    reg.search_all_assets(True)   # hors editeur, le registre n'est pas rempli tout seul
    opts = unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=True,
        include_hard_package_references=True,
        include_searchable_names=False,
        include_soft_management_references=False,
        include_hard_management_references=False)
    vus = set()
    a_voir = [CARTE]
    while a_voir:
        p = a_voir.pop()
        if p in vus:
            continue
        vus.add(p)
        try:
            deps = reg.get_dependencies(unreal.Name(p), opts) or []
        except Exception:
            deps = []
        for d in deps:
            d = str(d)
            if d.startswith("/Game/") and d not in vus:
                a_voir.append(d)
    return reg, vus


def textures_candidates():
    reg, paquets = paquets_de_la_carte()
    log("Paquets utilises par %s : %d" % (CARTE, len(paquets)))
    chemins = set()
    for p in paquets:
        try:
            for ad in reg.get_assets_by_package_name(unreal.Name(p)) or []:
                if str(ad.asset_class) == "Texture2D":
                    chemins.add(str(ad.object_path))
        except Exception:
            pass
    for dossier in DOSSIERS_EN_PLUS:
        for ad in reg.get_assets_by_path(unreal.Name(dossier), recursive=True) or []:
            if str(ad.asset_class) == "Texture2D":
                chemins.add(str(ad.object_path))
    return sorted(chemins)


def octets_par_pixel(tex):
    """Estimation grossiere de la memoire video par pixel selon la compression."""
    try:
        c = str(tex.get_editor_property("compression_settings")).upper()
    except Exception:
        c = ""
    if "HDR" in c:
        return 8.0
    if "VECTORDISPLACEMENT" in c or "USERINTERFACE" in c or "EDITORICON" in c:
        return 4.0
    return 1.0   # DXT5 / BC5 / BC7 / G8 (DXT1 = 0,5 : l'estimation reste prudente)


def groupe_ignore(tex):
    try:
        g = str(tex.get_editor_property("lod_group")).upper()
    except Exception:
        return False
    return any(x in g for x in GROUPES_IGNORES)


def analyser(chemins):
    a_reduire = []   # (chemin, tex, largeur, hauteur, ancien_max, gain_octets)
    erreurs = 0
    with unreal.ScopedSlowTask(len(chemins), "Analyse des textures de la carte...") as t:
        t.make_dialog(True)
        for ch in chemins:
            if t.should_cancel():
                log("Analyse annulee.")
                return None, erreurs
            t.enter_progress_frame(1, ch)
            try:
                tex = unreal.load_asset(ch)
                if not isinstance(tex, unreal.Texture2D) or groupe_ignore(tex):
                    continue
                ancien = int(tex.get_editor_property("max_texture_size"))
                w, h = tex.blueprint_get_size_x(), tex.blueprint_get_size_y()
                if max(w, h) <= LIMITE:
                    continue
                if 0 < ancien <= LIMITE:
                    continue
                f = float(LIMITE) / max(w, h)
                bpp = octets_par_pixel(tex) * 1.333   # + mipmaps
                gain = w * h * bpp - (w * f) * (h * f) * bpp
                a_reduire.append((ch, tex, w, h, ancien, gain))
            except Exception:
                erreurs += 1
    return a_reduire, erreurs


# ----------------------------------------------------------------------------------------------- application
def charger_journal():
    try:
        with open(JOURNAL_JSON, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return {}


def appliquer(a_reduire):
    journal = charger_journal()
    faites = 0
    with unreal.ScopedSlowTask(len(a_reduire), "Reduction des textures (Unreal recompresse)...") as t:
        t.make_dialog(True)
        for ch, tex, w, h, ancien, gain in a_reduire:
            if t.should_cancel():
                log("Arret demande : %d texture(s) deja reduites (elles restent reduites)." % faites)
                break
            t.enter_progress_frame(1, "%s (%dx%d -> %d)" % (ch, w, h, LIMITE))
            try:
                if ch not in journal:
                    journal[ch] = ancien
                tex.set_editor_property("max_texture_size", LIMITE)
                unreal.EditorAssetLibrary.save_loaded_asset(tex, False)
                faites += 1
            except Exception as e:
                log("  ECHEC %s : %s" % (ch, e))
            if faites % 25 == 0:   # on note au fur et a mesure, au cas ou Unreal se ferme
                ecrire_journal(journal)
    ecrire_journal(journal)
    return faites


def ecrire_journal(journal):
    os.makedirs(DOSSIER_OUTIL, exist_ok=True)
    with open(JOURNAL_JSON, "w", encoding="utf-8") as f:
        json.dump(journal, f, indent=1, ensure_ascii=False)


# ----------------------------------------------------------------------------------------------- principal
def main():
    log("Recherche des textures plus grandes que %d px..." % LIMITE)
    chemins = textures_candidates()
    log("Textures trouvees dans la carte : %d" % len(chemins))
    a_reduire, erreurs = analyser(chemins)
    if a_reduire is None:
        return
    a_reduire.sort(key=lambda x: -x[5])
    gain = sum(x[5] for x in a_reduire) / (1024.0 ** 3)
    log("A reduire : %d texture(s), gain estime en memoire video : %.2f Go (%d illisible(s))"
        % (len(a_reduire), gain, erreurs))
    for ch, tex, w, h, ancien, g in a_reduire[:40]:
        log("  %5dx%-5d  %6.0f Mo  %s" % (w, h, g / 1048576.0, ch))
    if len(a_reduire) > 40:
        log("  ... et %d autre(s)." % (len(a_reduire) - 40))
    if not a_reduire:
        log("Aucune texture au-dessus de %d px : rien a faire." % LIMITE)
        return
    mode = os.environ.get("CHELLES_TEXTURES", "").lower()
    if not mode:   # lance depuis l'editeur (File > Execute Python Script) : on demande Oui/Non
        question = ("%d texture(s) depassent %d px.\nGain estime : environ %.1f Go de memoire video.\n\n"
                    "Les reduire maintenant ? (restaurer_textures.py remet tout comme avant)" % (len(a_reduire), LIMITE, gain))
        if unreal.EditorDialog.show_message("Reduire les textures", question,
                                            unreal.AppMsgType.YES_NO) == unreal.AppReturnType.YES:
            mode = "reduire"
    if mode != "reduire":
        log("SIMULATION : rien n'a ete modifie. Pour appliquer : Textures_2_REDUIRE.bat")
        return
    faites = appliquer(a_reduire)
    log("Termine : %d texture(s) reduites a %d px. Ouvre Unreal et lance Play pour verifier." % (faites, LIMITE))

try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())
    unreal.log_error(traceback.format_exc())
finally:
    ecrire_log()
