# -*- coding: utf-8 -*-
"""
CHELLES 3D - Cycle jour / nuit avec le pack GoodSky, nuit en teintes orangees.

Ce que fait le script, dans le niveau OUVERT :
  1. Cree des copies "orangees la nuit" des courbes de couleur de GoodSky dans
     /Game/Chelles/CycleJourNuit/ (les courbes du pack ne sont PAS modifiees).
  2. Cree BP_GoodSky_Chelles (enfant de BP_GoodSky, le pack n'est PAS modifie)
     qui utilise ces courbes.
  3. Pose BP_GoodSky_Chelles dans le niveau, branche la lumiere directionnelle
     (le soleil), active le cycle jour/nuit automatique en jeu.
  4. Cache EN JEU l'ancien ciel (BP_Sky_Sphere, SkyAtmosphere) sans le supprimer.

Lancer : File > Execute Python Script... > ce fichier.
Le niveau n'est PAS enregistre tout seul : Ctrl+S si le resultat plait.
Pour tout annuler : retirer_cycle_jour_nuit.py
"""
import os
import unreal

# ---------------- Reglages (modifiables) ----------------
DUREE_JOURNEE_MINUTES = 12.0     # duree d'une journee complete en jeu (minutes reelles)
HEURE_DE_DEPART = 17.0           # heure au lancement du jeu (17 = fin d'apres-midi)
ORANGE_NUIT = (1.0, 0.42, 0.12)  # teinte de la nuit (rouge, vert, bleu)
FORCE_ORANGE = 1.0               # 1 = orange franc, 0.5 = moitie orange / moitie d'origine
LUMINOSITE_NUIT = 1.3            # 1 = meme luminosite qu'a l'origine, 1.3 = un peu plus lumineux
# ---------------------------------------------------------

GOODSKY_BP = "/Game/GoodSky/Blueprint/BP_GoodSky"
DOSSIER = "/Game/Chelles/CycleJourNuit"
NOM_BP = "BP_GoodSky_Chelles"
LABEL = "Chelles_CycleJourNuit"
TAG = "ChellesCycleJourNuit"

VAR_PRESET = "SkyPreset"
VAR_LUMIERE = u"└ Directional light actor ( For Custom Mode )"
VAR_HEURE = u"└ Time of Day ( For Custom Mode )"
VAR_DUREE = u"└ How long is a day in the game? ( Unit : Minute )"
VAR_CYCLE = "Enable Auto Day / Night Cycle In Game?"
VAR_TIME_OF_DAY = "Enable Time of Day"
VARS_COURBES = ["Curve_DomeColor", "Curve_BackGroundHorizonColor", "Curve_BackGroundSkyDomeColor",
                "Curve_AllOverlayColor", "Curve_BaseCloudColor", "Curve_BaseCloudColorAndIntensity"]

eal = unreal.EditorAssetLibrary
ell = unreal.EditorLevelLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
RAPPORT = []


def log(m):
    RAPPORT.append(m)
    unreal.log_warning("[CycleJourNuit] " + m)


def classe_bp(chemin):
    nom = chemin.split("/")[-1]
    cls = None
    try:
        cls = eal.load_blueprint_class(chemin)
    except Exception:
        cls = None
    return cls or unreal.load_object(None, chemin + "." + nom + "_C")


def lire(obj, nom):
    try:
        return obj.get_editor_property(nom)
    except Exception:
        return None


def ecrire(obj, nom, valeur, cdo=None):
    """Ecrit sur l'acteur ; si refuse, ecrit sur les valeurs par defaut du Blueprint enfant."""
    try:
        obj.set_editor_property(nom, valeur)
        return True
    except Exception as e1:
        if cdo is not None:
            try:
                cdo.set_editor_property(nom, valeur)
                return True
            except Exception as e2:
                log("Impossible de regler '%s' : %s / %s" % (nom, e1, e2))
                return False
        log("Impossible de regler '%s' : %s" % (nom, e1))
        return False


# ---------- 1. Courbes orangees ----------
def poids_nuit(t):
    """0 en plein jour, 1 en pleine nuit, transition douce a l'aube et au crepuscule."""
    def lisse(a, b, x):
        x = max(0.0, min(1.0, (x - a) / (b - a)))
        return x * x * (3 - 2 * x)
    if t <= 12.0:
        return 1.0 - lisse(4.5, 5.6, t)      # aube : nuit jusqu'a 4h30, jour a 5h36
    return lisse(18.7, 20.0, t)              # crepuscule : jour jusqu'a 18h42, nuit a 20h


def version_orange(r, g, b):
    lum = 0.2126 * r + 0.7152 * g + 0.0722 * b
    o = ORANGE_NUIT
    lum_o = 0.2126 * o[0] + 0.7152 * o[1] + 0.0722 * o[2]
    k = lum * LUMINOSITE_NUIT / max(lum_o, 1e-4)
    return (o[0] * k, o[1] * k, o[2] * k)


def type_csv_courbe_couleur():
    enum = getattr(unreal, "CSVImportType", None)
    if enum is None:
        return None
    for nom in ("ECSV_CURVE_LINEAR_COLOR", "ECSV_CurveLinearColor", "CURVE_LINEAR_COLOR"):
        if hasattr(enum, nom):
            return getattr(enum, nom)
    return None


def creer_courbe_orange(originale):
    nom = "Curve_Chelles_" + originale.get_name().replace("Curve_GoodSky_", "") + "_NuitOrange"
    lignes = []
    t = 0.0
    while t <= 24.0001:
        c = originale.get_linear_color_value(t)
        w = poids_nuit(t) * FORCE_ORANGE
        orr, org, orb = version_orange(c.r, c.g, c.b)
        r = c.r + (orr - c.r) * w
        g = c.g + (org - c.g) * w
        b = c.b + (orb - c.b) * w
        lignes.append("%.3f,%.5f,%.5f,%.5f,%.5f" % (t, r, g, b, c.a))
        t += 0.25
    dossier_csv = os.path.join(unreal.Paths.project_saved_dir(), "CycleJourNuit")
    if not os.path.isdir(dossier_csv):
        os.makedirs(dossier_csv)
    chemin_csv = os.path.join(dossier_csv, nom + ".csv")
    with open(chemin_csv, "w") as f:
        f.write("\n".join(lignes) + "\n")

    tache = unreal.AssetImportTask()
    tache.set_editor_property("filename", chemin_csv)
    tache.set_editor_property("destination_path", DOSSIER)
    tache.set_editor_property("destination_name", nom)
    tache.set_editor_property("replace_existing", True)
    tache.set_editor_property("automated", True)
    tache.set_editor_property("save", True)
    fabrique = unreal.CSVImportFactory()
    reglages = unreal.CSVImportSettings()
    t_csv = type_csv_courbe_couleur()
    reglages.set_editor_property("import_type", t_csv if t_csv is not None else 4)
    fabrique.set_editor_property("automated_import_settings", reglages)
    tache.set_editor_property("factory", fabrique)
    asset_tools.import_asset_tasks([tache])

    courbe = unreal.load_asset(DOSSIER + "/" + nom)
    if not isinstance(courbe, unreal.CurveLinearColor):
        raise RuntimeError("Import de la courbe %s rate (fichier %s)." % (nom, chemin_csv))
    test = courbe.get_linear_color_value(1.0)
    log("Courbe %s creee (a 1h du matin : R=%.2f V=%.2f B=%.2f)." % (nom, test.r, test.g, test.b))
    return courbe


# ---------- 2. Blueprint enfant ----------
def blueprint_enfant(cls_goodsky):
    chemin = DOSSIER + "/" + NOM_BP
    if eal.does_asset_exist(chemin):
        return classe_bp(chemin)
    fab = unreal.BlueprintFactory()
    fab.set_editor_property("parent_class", cls_goodsky)
    bp = asset_tools.create_asset(NOM_BP, DOSSIER, unreal.Blueprint, fab)
    if bp is None:
        raise RuntimeError("Creation de " + chemin + " impossible.")
    eal.save_asset(chemin, False)
    log(NOM_BP + " cree (enfant de BP_GoodSky, le pack reste intact).")
    return classe_bp(chemin)


def index_preset_custom():
    enum = unreal.load_asset("/Game/GoodSky/Source/Data/Enum/Enum_GoodSky_Preset")
    if enum is None:
        return None
    for i in range(0, 40):
        try:
            nom = str(unreal.KismetNodeHelperLibrary.get_enumerator_user_friendly_name(enum, i))
        except Exception:
            break
        if "Custom Mode" in nom:
            return i
    return None


def ajouter_tag(acteur, tag):
    tags = list(acteur.get_editor_property("tags"))
    tags.append(unreal.Name(tag))
    acteur.set_editor_property("tags", tags)


def rayon_utile(acteurs):
    """Distance max du decor au centre, pour que le ciel englobe tout le quartier."""
    d = 0.0
    for a in acteurs:
        if a.get_class().get_name() in ("BP_Sky_Sphere_C", "SkyAtmosphere"):
            continue
        try:
            origine, ext = a.get_actor_bounds(False)
        except Exception:
            continue
        if ext.x > 2.0e6 or ext.y > 2.0e6:   # volumes geants (post-process infini...)
            continue
        dist = (origine.x ** 2 + origine.y ** 2) ** 0.5 + max(ext.x, ext.y)
        d = max(d, dist)
    return d


try:
    cls_goodsky = classe_bp(GOODSKY_BP)
    if not cls_goodsky:
        raise RuntimeError("BP_GoodSky introuvable dans " + GOODSKY_BP)

    if not eal.does_directory_exist(DOSSIER):
        eal.make_directory(DOSSIER)

    cls_chelles = blueprint_enfant(cls_goodsky)
    cdo = unreal.get_default_object(cls_chelles)

    # Courbes : on lit celles du pack, on fabrique les versions orangees
    remplacees = 0
    for var in VARS_COURBES:
        orig = lire(cdo, var)
        if isinstance(orig, unreal.CurveLinearColor):
            nouvelle = creer_courbe_orange(orig) if "NuitOrange" not in orig.get_name() else orig
            try:
                cdo.set_editor_property(var, nouvelle)
                remplacees += 1
            except Exception as e:
                log("Courbe %s non remplacee : %s" % (var, e))
    log("%d courbe(s) de couleur passee(s) en version nuit orangee." % remplacees)
    eal.save_asset(DOSSIER + "/" + NOM_BP, False)

    acteurs = ell.get_all_level_actors()

    # Lumiere directionnelle (le soleil)
    soleil = None
    for a in acteurs:
        if isinstance(a, unreal.DirectionalLight):
            soleil = a
            break
    if soleil is None:
        soleil = ell.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(0, -40, 0))
        soleil.set_actor_label("Soleil_CycleJourNuit")
        ajouter_tag(soleil, TAG + "_cree")
        log("Pas de lumiere directionnelle : Soleil_CycleJourNuit ajoute.")
    comp = soleil.get_component_by_class(unreal.DirectionalLightComponent) or soleil.root_component
    mob = comp.get_editor_property("mobility")
    ancienne = str(mob).split(".")[-1].split(":")[0].strip("<> ")
    if mob != unreal.ComponentMobility.MOVABLE:
        ajouter_tag(soleil, TAG + "_mobilite_" + ancienne)
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        log("Soleil '%s' : mobilite %s -> Movable (indispensable pour qu'il bouge)." % (soleil.get_actor_label(), ancienne))

    # Ancien ciel : cache en jeu, pas supprime
    for a in acteurs:
        n = a.get_class().get_name()
        if n in ("BP_Sky_Sphere_C", "SkyAtmosphere") and TAG not in [str(t) for t in a.tags]:
            a.set_actor_hidden_in_game(True)
            ajouter_tag(a, TAG)
            log("Ancien ciel '%s' cache en jeu (pas supprime)." % a.get_actor_label())

    # Le ciel GoodSky
    ciel = None
    for a in acteurs:
        if a.get_actor_label() == LABEL:
            ciel = a
            break
    if ciel is None:
        ciel = ell.spawn_actor_from_class(cls_chelles, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        ciel.set_actor_label(LABEL)
        log("Ciel GoodSky pose : " + LABEL)
    try:
        ciel.set_folder_path("Chelles/CycleJourNuit")
    except Exception:
        pass

    # Taille : le ciel doit englober tout le quartier
    rayon = rayon_utile(acteurs)
    _o, ext_ciel = ciel.get_actor_bounds(False)
    r_ciel = max(ext_ciel.x, ext_ciel.y) / max(ciel.get_actor_scale3d().x, 1e-3)
    if r_ciel > 1.0 and rayon > 0.0:
        echelle = max(1.0, rayon * 1.3 / r_ciel)
        ciel.set_actor_scale3d(unreal.Vector(echelle, echelle, echelle))
        log("Taille du ciel x%.1f (quartier ~%.0f m de rayon)." % (echelle, rayon / 100.0))

    idx = index_preset_custom()
    if idx is not None:
        ecrire(ciel, VAR_PRESET, idx, cdo)
        log("Sky Preset = Custom Mode (From Time Of Day).")
    else:
        log("ATTENTION : preset 'Custom Mode' non trouve, a choisir a la main dans Details > Sky Preset.")
    ecrire(ciel, VAR_TIME_OF_DAY, True, cdo)
    ecrire(ciel, VAR_LUMIERE, soleil, None)
    ecrire(ciel, VAR_HEURE, float(HEURE_DE_DEPART), cdo)
    ecrire(ciel, VAR_DUREE, float(DUREE_JOURNEE_MINUTES), cdo)
    ecrire(ciel, VAR_CYCLE, True, cdo)
    eal.save_asset(DOSSIER + "/" + NOM_BP, False)

    log("Cycle : une journee = %.0f min, depart a %.0fh. Nuit orangee de 20h a 4h30." % (DUREE_JOURNEE_MINUTES, HEURE_DE_DEPART))
    log("TERMINE. Clique Play pour voir le cycle. Ctrl+S pour garder le niveau.")
except Exception as e:
    unreal.log_error("[CycleJourNuit] ERREUR : " + str(e))
    RAPPORT.append("ERREUR : " + str(e))

try:
    d = os.path.join(unreal.Paths.project_saved_dir(), "CycleJourNuit")
    if not os.path.isdir(d):
        os.makedirs(d)
    with open(os.path.join(d, "rapport.txt"), "w") as f:
        f.write("\n".join(RAPPORT) + "\n")
except Exception:
    pass
