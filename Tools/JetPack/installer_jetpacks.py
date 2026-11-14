# -*- coding: utf-8 -*-
"""JETPACKS A RAMASSER dans la carte /Game/Maps/Rachid.

A lancer DANS L'EDITEUR (Unreal ouvert) :
    File > Execute Python Script... > Tools/JetPack/installer_jetpacks.py
ou double-clic sur INSTALLER_JETPACKS.bat (Unreal FERME : il compile le plugin, ouvre Unreal,
travaille, enregistre et referme).

Il faut que le plugin ChellesJetPack soit compile (INSTALLER_JETPACKS.bat le fait).

Ce que fait le script :
  1. cree les Blueprints dans /Game/FPS_Game/Blueprints/Bonus :
       BP_JetPack           (le modele commun : 3D, flammes, bruit, vitesse...)
       BP_JetPack_Rouge, BP_JetPack_Bleu, BP_JetPack_Vert, BP_JetPack_Jaune, BP_JetPack_Violet
     (enfants de BP_JetPack, seule la couleur change) ;
  2. enregistre Rachid et en fait une copie dans Tools/JetPack/sauvegarde/ ;
  3. enleve les JetPacks poses par un passage precedent de ce script (etiquette ChellesJetPackAjoute) ;
  4. pose un JetPack de chaque couleur dans la ville, en plein air, loin les uns des autres
     (le ROUGE pres du depart du joueur pour l'essayer tout de suite) ;
  5. enregistre Rachid.

Les JetPacks deja poses a la main ne sont pas touches. Relancer = nouvelle disposition.
Pour en poser d'autres a la main : glisser un BP_JetPack_* du dossier Bonus dans la carte.
Pour annuler : retirer_jetpacks.py (ou RETIRER_JETPACKS.bat).
Journal : Tools/JetPack/jetpack.log
"""
import math
import os
import random
import shutil
import time
import traceback

import unreal

# ------------------------------------------------------------------ reglages
CARTE = "/Game/Maps/Rachid"
DOSSIER_BP = "/Game/FPS_Game/Blueprints/Bonus"
CLASSE_CPP = "/Script/ChellesJetPack.JetPackChelles"

# modele du pack Jet Packs Bundle (autres choix : SciFi_Jetpack_Standalone_SK, Acme_Rocket_Jetpack_SK,
# OldFashioned_Jetpack_Standalone_SK, Twin_Fan_Jetpack_Solid_Standalone_SK)
MODELE = "/Game/Jet_Packs/Meshes/Skeletal/DualJet_Jetpack_Standalone_SK"
FLAMMES = "/Game/Jet_Packs/Particles/Jet_Exhaust_PS"
SON = "/Game/Jet_Packs/Audio/Jet_Engine_Loop_Cue"
DEMO_PERSO = "/Game/Jet_Packs/Demo/Blueprints/Jetpack_ThirdPersonCharacter"

# nom, couleur (rouge, vert, bleu entre 0 et 1), nom affiche au joueur
COULEURS = [
    ("Rouge", (1.00, 0.02, 0.02), "ROUGE"),
    ("Bleu", (0.02, 0.18, 1.00), "BLEU"),
    ("Vert", (0.05, 0.85, 0.08), "VERT"),
    ("Jaune", (1.00, 0.80, 0.02), "JAUNE"),
    ("Violet", (0.55, 0.05, 1.00), "VIOLET"),
]

ESPACEMENT = 150.0     # metres minimum entre deux JetPacks (reduit tout seul si la place manque)
PRES_DU_DEPART = (8.0, 30.0)  # le ROUGE est pose entre 8 et 30 m du depart du joueur
CIEL_LIBRE = 12.0      # metres de ciel libre au-dessus d'un JetPack (pour pouvoir decoller)
GRAINE = None          # mettre un nombre (ex. 7) pour retrouver toujours la meme disposition

TAG_AJOUTE = "ChellesJetPackAjoute"
DOSSIER_OUTLINER = "JetPacks"

ell = unreal.EditorLevelLibrary
eal = unreal.EditorAssetLibrary
outils = unreal.AssetToolsHelpers.get_asset_tools()
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ICI = os.path.join(PROJET, "Tools", "JetPack")
JOURNAL = os.path.join(ICI, "jetpack.log")


def log(msg):
    unreal.log("[JetPack] " + str(msg))
    try:
        os.makedirs(ICI, exist_ok=True)
        with open(JOURNAL, "a", encoding="utf-8") as f:
            f.write(time.strftime("%H:%M:%S ") + str(msg) + "\n")
    except Exception:
        pass


def charger(chemin):
    if not eal.does_asset_exist(chemin):
        log("   ATTENTION : %s introuvable" % chemin)
        return None
    return eal.load_asset(chemin)


def regler(objet, nom, valeur):
    try:
        objet.set_editor_property(nom, valeur)
        return True
    except Exception as e:
        log("   ATTENTION : reglage %s impossible (%s)" % (nom, e))
        return False


def a_tag(acteur, tag):
    try:
        return any(str(t) == tag for t in acteur.get_editor_property("tags"))
    except Exception:
        return False


def ajouter_tag(acteur, tag):
    tags = list(acteur.get_editor_property("tags"))
    if not any(str(t) == tag for t in tags):
        tags.append(unreal.Name(tag))
        acteur.set_editor_property("tags", tags)


# ------------------------------------------------------------------ 1. couleurs du materiau
def materiau_a_colorer(modele):
    """Version "Mask" du materiau du pack (faite pour changer les couleurs)."""
    noms = ["DualJet_Mask_Example_MI"]
    nom_modele = modele.get_name().lower() if modele else ""
    for cle, mi in (("scifi", "SciFi_Mask_Example_MI"), ("acme", "Acme_Mask_Example_MI"),
                    ("oldfashioned", "OldFashioned_Mask_Example_MI"), ("handheld", "HandHeld_Mask_Example_MI"),
                    ("twin_fan", "TwinFan_Jetpack_Mask_Example_MI"), ("wingsuit", "WingSuit_Mask_Example_MI"),
                    ("dualjet", "DualJet_Mask_Example_MI")):
        if cle in nom_modele:
            noms = [mi]
            break
    for n in noms:
        ch = "/Game/Jet_Packs/Materials/Mask_Examples/" + n
        if eal.does_asset_exist(ch):
            return eal.load_asset(ch)
    return None


def noms_parametres_vecteur(mi):
    noms = []
    try:
        noms = [str(n) for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(mi)]
    except Exception:
        pass
    if not noms:
        try:
            for v in mi.get_editor_property("vector_parameter_values"):
                noms.append(str(v.get_editor_property("parameter_info").get_editor_property("name")))
        except Exception:
            pass
    return noms


def choisir_parametres(mi):
    """Rend (parametres couleur principale, parametres couleur sombre)."""
    if mi is None:
        return [], []
    noms = noms_parametres_vecteur(mi)
    log("   parametres de couleur de %s : %s" % (mi.get_name(), ", ".join(noms) or "(aucun)"))
    couleurs = [n for n in noms if any(m in n.lower() for m in ("colour", "color", "tint", "couleur", "albedo"))]
    if not couleurs:
        couleurs = list(noms)
    if not couleurs:
        return [], []
    principale = [couleurs[0]]
    sombre = []
    for n in couleurs[1:]:
        # 2e couleur du masque : plus sombre (effet deux tons), les suivantes comme la principale
        if not sombre:
            sombre.append(n)
        else:
            principale.append(n)
    log("   couleur principale -> %s ; couleur sombre -> %s" % (", ".join(principale), ", ".join(sombre) or "-"))
    return principale, sombre


# ------------------------------------------------------------------ 1. place sur le dos (d'apres la demo du pack)
def mesurer_dos():
    """Pose un instant le personnage de demo du pack, mesure ou est le JetPack par rapport a spine_03."""
    if not eal.does_asset_exist(DEMO_PERSO):
        log("   personnage de demo introuvable : JetPack accroche directement sur spine_03")
        return None
    perso = None
    try:
        cls = eal.load_blueprint_class(DEMO_PERSO)
        perso = ell.spawn_actor_from_class(cls, unreal.Vector(0, 0, -200000), unreal.Rotator(0, 0, 0))
        if perso is None:
            return None
        corps = None
        for c in perso.get_components_by_class(unreal.SkeletalMeshComponent):
            if c.get_name() == "CharacterMesh0" or corps is None:
                corps = c
        enfant = None
        for c in perso.get_components_by_class(unreal.ChildActorComponent):
            enfant = c.get_editor_property("child_actor")
            if enfant:
                break
        if corps is None or enfant is None:
            log("   demo : JetPack de demo non trouve")
            return None
        jet = None
        for c in enfant.get_components_by_class(unreal.SkeletalMeshComponent):
            if c.get_editor_property("skeletal_mesh") is not None and c.get_editor_property("visible"):
                jet = c
                break
        if jet is None:
            return None
        dos = corps.get_socket_transform("spine_03", unreal.RelativeTransformSpace.RTS_WORLD)
        rel = unreal.MathLibrary.make_relative_transform(jet.get_world_transform(), dos)
        loc = rel.translation
        rot = rel.rotation.rotator()
        log("   demo du pack : %s sur spine_03, decalage (%.1f, %.1f, %.1f) rotation (%.1f, %.1f, %.1f)"
            % (jet.get_editor_property("skeletal_mesh").get_name(), loc.x, loc.y, loc.z, rot.roll, rot.pitch, rot.yaw))
        return unreal.Vector(loc.x, loc.y, loc.z), unreal.Rotator(roll=rot.roll, pitch=rot.pitch, yaw=rot.yaw)
    except Exception as e:
        log("   mesure du dos impossible (%s) : JetPack accroche directement sur spine_03" % e)
        return None
    finally:
        try:
            if perso is not None:
                for c in perso.get_components_by_class(unreal.ChildActorComponent):
                    a = c.get_editor_property("child_actor")
                    if a:
                        ell.destroy_actor(a)
                ell.destroy_actor(perso)
        except Exception:
            pass


# ------------------------------------------------------------------ 1. Blueprints
def creer_bp(nom, parent):
    chemin = DOSSIER_BP + "/" + nom
    if eal.does_asset_exist(chemin):
        log("   %s existe deja : mis a jour" % nom)
        return eal.load_blueprint_class(chemin)
    fab = unreal.BlueprintFactory()
    fab.set_editor_property("parent_class", parent)
    bp = outils.create_asset(nom, DOSSIER_BP, unreal.Blueprint, fab)
    if bp is None:
        raise RuntimeError("Creation de %s impossible." % chemin)
    eal.save_asset(chemin, False)
    log("   %s cree" % nom)
    return eal.load_blueprint_class(chemin)


def creer_blueprints(cls_cpp, dos):
    modele = charger(MODELE)
    flammes = charger(FLAMMES)
    son = charger(SON)
    mi = materiau_a_colorer(modele)
    principale, sombre = choisir_parametres(mi)

    cls_base = creer_bp("BP_JetPack", cls_cpp)
    cdo = unreal.get_default_object(cls_base)
    if modele:
        regler(cdo, "modele_jet_pack", modele)
    if mi and principale:
        regler(cdo, "materiau_couleur", mi)
        regler(cdo, "parametres_couleur", [unreal.Name(n) for n in principale])
        regler(cdo, "parametres_couleur_sombre", [unreal.Name(n) for n in sombre])
    else:
        log("   pas de parametre de couleur trouve : seule la lueur prendra la couleur")
    if flammes:
        regler(cdo, "flammes", flammes)
    if son:
        regler(cdo, "son_moteur", son)
    regler(cdo, "os_du_dos", unreal.Name("spine_03"))
    if dos is not None:
        regler(cdo, "decalage_dos", dos[0])
        regler(cdo, "rotation_dos", dos[1])
    regler(cdo, "couleur", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    regler(cdo, "nom_couleur", "BLANC")
    eal.save_asset(DOSSIER_BP + "/BP_JetPack", False)

    classes = []
    for nom, (r, v, b), affiche in COULEURS:
        cls = creer_bp("BP_JetPack_" + nom, cls_base)
        c = unreal.get_default_object(cls)
        regler(c, "couleur", unreal.LinearColor(r, v, b, 1.0))
        regler(c, "nom_couleur", affiche)
        eal.save_asset(DOSSIER_BP + "/BP_JetPack_" + nom, False)
        classes.append((nom, cls))
    log("1/5 Blueprints prets dans %s : BP_JetPack + %s"
        % (DOSSIER_BP, ", ".join("BP_JetPack_" + n for n, _c in classes)))
    return classes


# ------------------------------------------------------------------ 4. places dans la ville
def lire_impact(hit):
    try:
        return (hit.get_editor_property("impact_point"), hit.get_editor_property("impact_normal"),
                hit.get_editor_property("actor"))
    except Exception:
        pass
    t = hit.to_tuple()
    return t[5], t[7], t[9]


def tracer(monde, debut, fin, complexe=False):
    try:
        hit = unreal.SystemLibrary.line_trace_single(
            monde, debut, fin, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, complexe, [],
            unreal.DrawDebugTrace.NONE, True)
    except Exception:
        return None
    if hit is None or isinstance(hit, tuple):
        return None
    try:
        r = lire_impact(hit)
        return None if r[0] is None else r
    except Exception:
        return None


MOTS_PAS_SOL = ("sky", "ciel", "neige", "snow", "cloud", "water", "eau", "tree", "arbre", "foliage",
                "voiture", "vehic", "car_", "bus")


def texte_acteur(act):
    try:
        return (act.get_class().get_name() + " " + act.get_actor_label()).lower()
    except Exception:
        return ""


def zone_ville(acteurs):
    for a in acteurs:
        if isinstance(a, unreal.NavMeshBoundsVolume) and (a.get_actor_label() == "NavMesh_Chelles"
                                                          or a_tag(a, "ChellesNavMesh")):
            return a.get_actor_bounds(False)
    vols = [a for a in acteurs if isinstance(a, unreal.NavMeshBoundsVolume)]
    if vols:
        return vols[0].get_actor_bounds(False)
    pts = [a.get_actor_location() for a in acteurs if isinstance(a, unreal.StaticMeshActor)]
    if not pts:
        return None
    xs = sorted(p.x for p in pts)
    ys = sorted(p.y for p in pts)
    zs = sorted(p.z for p in pts)
    q = lambda l, f: l[int((len(l) - 1) * f)]
    mn = unreal.Vector(q(xs, 0.05), q(ys, 0.05), q(zs, 0.02) - 5000.0)
    mx = unreal.Vector(q(xs, 0.95), q(ys, 0.95), q(zs, 0.98) + 10000.0)
    return (unreal.Vector((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, (mn.z + mx.z) / 2),
            unreal.Vector((mx.x - mn.x) / 2, (mx.y - mn.y) / 2, (mx.z - mn.z) / 2))


def depart_joueur(acteurs):
    for a in acteurs:
        if isinstance(a, unreal.PlayerStart):
            return a.get_actor_location()
    return None


def place_libre(monde, p):
    """Sol plat, pas sous un toit / un arbre : rend le point du sol ou None."""
    if tracer(monde, unreal.Vector(p.x, p.y, p.z + 20.0), unreal.Vector(p.x, p.y, p.z + CIEL_LIBRE * 100.0), True) is not None:
        return None
    # pas colle a un mur (1,5 m de chaque cote)
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        if tracer(monde, unreal.Vector(p.x, p.y, p.z + 80.0),
                  unreal.Vector(p.x + dx * 150.0, p.y + dy * 150.0, p.z + 80.0), True) is not None:
            return None
    return p


def points_de_rue(monde, centre, etendue, voulu, autour=None):
    haut_z = centre.z + etendue.z
    bas_z = centre.z - etendue.z
    impacts = []
    debut = time.time()
    essais = 0
    while len(impacts) < voulu and essais < voulu * 6 and time.time() - debut < 120:
        essais += 1
        if autour is not None:
            ang = random.uniform(0, 2 * math.pi)
            d = random.uniform(PRES_DU_DEPART[0], PRES_DU_DEPART[1]) * 100.0
            x, y = autour.x + math.cos(ang) * d, autour.y + math.sin(ang) * d
        else:
            x = random.uniform(centre.x - etendue.x, centre.x + etendue.x)
            y = random.uniform(centre.y - etendue.y, centre.y + etendue.y)
        r = tracer(monde, unreal.Vector(x, y, haut_z), unreal.Vector(x, y, bas_z))
        if r is None:
            continue
        p, n, act = r
        if n is None or n.z < 0.85:
            continue
        if act is not None:
            if isinstance(act, unreal.Pawn):
                continue
            if any(m in texte_acteur(act) for m in MOTS_PAS_SOL):
                continue
        impacts.append(p)
    if not impacts:
        return []
    zs = sorted(p.z for p in impacts)
    rue = zs[int(len(zs) * 0.3)]
    return [p for p in impacts if p.z <= rue + 350.0]


def chercher_places(monde, zone, joueur, nombre):
    centre, etendue = zone
    places = []
    # le premier (ROUGE) pres du depart du joueur
    if joueur is not None:
        for p in points_de_rue(monde, centre, etendue, 60, autour=joueur):
            if abs(p.z - joueur.z) < 400.0 and place_libre(monde, p):
                places.append(p)
                log("   ROUGE pres du depart du joueur (%.0f m)" % (math.hypot(p.x - joueur.x, p.y - joueur.y) / 100.0))
                break
    candidats = [p for p in points_de_rue(monde, centre, etendue, 1500) if place_libre(monde, p)]
    log("   %d places en plein air trouvees dans la ville" % len(candidats))

    def dist(a, b):
        return math.hypot(a.x - b.x, a.y - b.y) / 100.0

    espacement = ESPACEMENT
    base = list(places)
    while True:
        random.shuffle(candidats)
        choisis = list(base)
        for p in candidats:
            if len(choisis) >= nombre:
                break
            if all(dist(p, c) >= espacement for c in choisis) and (joueur is None or dist(p, joueur) >= 5.0):
                choisis.append(p)
        if len(choisis) >= nombre or espacement < 10:
            log("   espacement utilise : %.0f m" % espacement)
            return choisis[:nombre]
        espacement *= 0.8


# ------------------------------------------------------------------ principal
def demander(texte):
    if os.environ.get("CHELLES_QUITTER") == "1":
        return True
    r = unreal.EditorDialog.show_message("Chelles 3D - JetPacks", texte, unreal.AppMsgType.YES_NO)
    return r == unreal.AppReturnType.YES


def main():
    open(JOURNAL, "w").close()
    if GRAINE is not None:
        random.seed(GRAINE)

    cls_cpp = unreal.load_class(None, CLASSE_CPP)
    if cls_cpp is None:
        log("ABANDON : le plugin ChellesJetPack n'est pas compile/active. "
            "Fermez Unreal et lancez Tools\\JetPack\\INSTALLER_JETPACKS.bat")
        return

    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        log("chargement de %s ..." % CARTE)
        ell.load_level(CARTE)
        monde = ell.get_editor_world()
    log("Carte : %s" % monde.get_path_name())

    acteurs = ell.get_all_level_actors()
    anciens = [a for a in acteurs if a_tag(a, TAG_AJOUTE)]
    if not demander("Le script va :\n"
                    "- creer BP_JetPack et 5 JetPacks de couleur (rouge, bleu, vert, jaune, violet)\n"
                    "  dans FPS_Game/Blueprints/Bonus ;\n"
                    "- poser un JetPack de chaque couleur dans Rachid (le rouge pres du depart).\n\n"
                    "%sUne copie de Rachid est faite avant. Continuer ?"
                    % (("Les %d JetPacks du passage precedent seront replaces.\n\n" % len(anciens)) if anciens else "")):
        log("Annule par l'utilisateur. Rien n'a ete modifie.")
        return

    # ---- 1. Blueprints
    log("1/5 Blueprints ...")
    dos = mesurer_dos()
    classes = creer_blueprints(cls_cpp, dos)

    # ---- 2. sauvegarde
    ell.save_current_level()
    fichier = os.path.join(PROJET, "Content", "Maps", "Rachid.umap")
    dossier = os.path.join(ICI, "sauvegarde")
    os.makedirs(dossier, exist_ok=True)
    copie = os.path.join(dossier, "Rachid_%s.umap" % time.strftime("%Y-%m-%d_%H%M"))
    shutil.copy2(fichier, copie)
    log("2/5 sauvegarde : %s" % copie)

    # ---- 3. anciens
    acteurs = ell.get_all_level_actors()
    anciens = [a for a in acteurs if a_tag(a, TAG_AJOUTE)]
    for a in anciens:
        ell.destroy_actor(a)
    log("3/5 %d JetPack(s) d'un passage precedent enleve(s)" % len(anciens))

    # ---- 4. pose
    acteurs = ell.get_all_level_actors()
    zone = zone_ville(acteurs)
    if zone is None:
        log("ABANDON de la pose : etendue de la ville introuvable (les Blueprints sont quand meme crees).")
        return
    joueur = depart_joueur(acteurs)
    log("4/5 recherche de places ...")
    places = chercher_places(monde, zone, joueur, len(classes))
    poses = []
    for (nom, cls), p in zip(classes, places):
        a = ell.spawn_actor_from_class(cls, unreal.Vector(p.x, p.y, p.z + 30.0),
                                       unreal.Rotator(roll=0.0, pitch=0.0, yaw=random.uniform(0, 360)))
        if a is None:
            log("   ERREUR : impossible de poser BP_JetPack_%s" % nom)
            continue
        a.set_actor_label("JetPack_%s" % nom)
        ajouter_tag(a, TAG_AJOUTE)
        a.set_folder_path(DOSSIER_OUTLINER)
        poses.append(nom)
        log("   JetPack %s pose en (%.0f, %.0f, %.0f)" % (nom, p.x, p.y, p.z))
    if len(poses) < len(classes):
        log("   ATTENTION : seulement %d place(s) trouvee(s). Envoyez Tools/JetPack/jetpack.log." % len(poses))
    log("4/5 poses : %s (dossier Outliner : %s)" % (", ".join(poses) or "aucun", DOSSIER_OUTLINER))

    # ---- 5. enregistrement
    if ell.save_current_level():
        log("5/5 Rachid enregistree.")
    else:
        log("5/5 ATTENTION : Rachid n'a pas pu etre enregistree. Faites File > Save Current.")
    log("TERMINE. Lancez Play, marchez sur un JetPack, puis ESPACE pour voler (CTRL/C descendre, MAJ turbo, N lacher).")


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
