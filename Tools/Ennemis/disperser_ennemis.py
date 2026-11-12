# -*- coding: utf-8 -*-
"""ENNEMIS VARIES ET EPARPILLES dans la carte /Game/Maps/Rachid.

A lancer DANS L'EDITEUR (Unreal ouvert, carte Rachid) :
    File > Execute Python Script... > Tools/Ennemis/disperser_ennemis.py
ou double-clic sur DISPERSER_ENNEMIS.bat (Unreal FERME : il l'ouvre, travaille, enregistre et referme).

Ce que fait le script :
  1. enregistre Rachid puis copie Rachid.umap dans Tools/Ennemis/sauvegarde/ ;
  2. cree des tenues de couleur (dossier /Game/Bandit_Sport/Material/Chelles) :
       rouge, vert, noir = materiaux du pack Bandit_Sport ;
       orange, violet, jaune = nouvelle matiere "teinte" (M_Bandit_clothes_Teinte_v2) ;
  3. habille chaque sorte d'ennemi en bandit avec SA couleur (le premier bandit reste BLEU) :
       AI_Soldier      -> rouge        AI_Charger -> vert       AI_Patrol  -> violet
       AI_Soldier_Boss -> noir         AI_Genesis -> orange     AI_Patrol1 -> jaune
  4. met exactement NB_BOSS boss (3), TAILLE_BOSS fois plus grands (x3), habilles en deux
     couleurs (rouge+noir, bleu+noir, jaune+rouge), puis ajoute
     des ennemis pour arriver a TOTAL_ENNEMIS (melange des sortes ci-dessus) ;
  5. eparpille TOUS les ennemis (bandits compris) sur le NavMesh de la ville,
     a ESPACEMENT metres les uns des autres et loin du point de depart du joueur ;
  6. enregistre Rachid.

Aucun Blueprint n'est modifie : on change seulement les ennemis poses dans la carte.
Les zombies (invasion) ne sont pas touches.
Pour annuler : retirer_ennemis.py (ou RETIRER_ENNEMIS.bat).
Journal : Tools/Ennemis/ennemis.log
"""
import json
import math
import os
import random
import shutil
import time
import traceback

import unreal

# ------------------------------------------------------------------ reglages
CARTE = "/Game/Maps/Rachid"
TOTAL_ENNEMIS = 40          # nombre total d'ennemis dans la carte apres le script
ESPACEMENT = 35.0           # metres minimum entre deux ennemis (reduit tout seul si la place manque)
LOIN_DU_JOUEUR = 40.0       # metres minimum entre un ennemi et le depart du joueur
GRAINE = None               # mettre un nombre (ex. 7) pour retrouver toujours la meme disposition

DOSSIER_IA = "/Game/FPS_Game/Blueprints/Bonus/AI/"
MESH_BANDIT = "/Game/Bandit_Sport/Mesh/SK_Bandit_FPS"
DOSSIER_MAT = "/Game/Bandit_Sport/Material"
DOSSIER_NOUVEAUX = "/Game/Bandit_Sport/Material/Chelles"

# sorte d'ennemi -> couleur de tenue
COULEUR_PAR_SORTE = {
    "AI_Soldier": "Rouge",
    "AI_Soldier_Boss": "Noir",
    "AI_Charger": "Vert",
    "AI_Genesis": "Orange",
    "AI_Patrol": "Violet",
    "AI_Patrol1": "Jaune",
}
# melange des autres ennemis AJOUTES (poids) ; les boss sont geres par NB_BOSS
MELANGE_AJOUTS = [("AI_Soldier", 4), ("AI_Patrol", 3), ("AI_Patrol1", 2),
                  ("AI_Charger", 2), ("AI_Genesis", 2)]
NB_BOSS = 3                 # il y aura exactement ce nombre de boss (AI_Soldier_Boss)
TAILLE_BOSS = 3.0           # les boss sont 3 fois plus grands que les autres ennemis
# tenue en DEUX couleurs pour chaque boss : (haut, bas) en couleurs lineaires
BOSS_TENUES = [
    ("Boss_RougeNoir", (0.85, 0.02, 0.02), (0.02, 0.02, 0.02)),
    ("Boss_BleuNoir", (0.03, 0.12, 0.9), (0.02, 0.02, 0.02)),
    ("Boss_JauneRouge", (1.0, 0.8, 0.02), (0.85, 0.02, 0.02)),
]
# sortes deplacees (eparpillees) mais pas rhabillees
SORTES_GARDEES = ("AI_Soldier_Bandit",)

MATERIAUX_PACK = {
    "Rouge": DOSSIER_MAT + "/M_Bandit_Clothes_Red",
    "Vert": DOSSIER_MAT + "/M_Bandit_clothes_Green",
    "Noir": DOSSIER_MAT + "/M_Bandit_clothes",
}
TEINTES = {  # couleurs lineaires, multipliees par le tissu du pack
    "Orange": (1.0, 0.32, 0.02),
    "Violet": (0.42, 0.06, 0.85),
    "Jaune": (1.0, 0.82, 0.02),
}
INTENSITE_TEINTE = 2.4      # plus grand = tenue plus claire (modifiable ensuite dans M_Bandit_clothes_Teinte_v2)
SECOURS = {"Orange": "Vert", "Violet": "Rouge", "Jaune": "Noir"}  # si la teinte ne peut pas etre creee

TAG_AJOUTE = "ChellesEnnemiAjoute"
TAG_HABILLE = "ChellesEnnemiHabille"
DOSSIER_OUTLINER = "Ennemis"

ell = unreal.EditorLevelLibrary
eal = unreal.EditorAssetLibrary
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ICI = os.path.join(PROJET, "Tools", "Ennemis")
JOURNAL = os.path.join(ICI, "ennemis.log")
ETAT = os.path.join(ICI, "etat_avant.json")


def log(msg):
    unreal.log("[Ennemis] " + str(msg))
    try:
        os.makedirs(ICI, exist_ok=True)
        with open(JOURNAL, "a", encoding="utf-8") as f:
            f.write(time.strftime("%H:%M:%S ") + str(msg) + "\n")
    except Exception:
        pass


# ------------------------------------------------------------------ outils
def sorte(acteur):
    n = acteur.get_class().get_name()
    return n[:-2] if n.endswith("_C") else n


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


def corps(acteur):
    """Le Mesh du personnage (composant herite de Character)."""
    try:
        m = acteur.get_editor_property("mesh")
        if m:
            return m
    except Exception:
        pass
    comps = acteur.get_components_by_class(unreal.SkeletalMeshComponent)
    return comps[0] if comps else None


def chemin(obj):
    return obj.get_path_name() if obj else None


def charger(ch):
    if ch and eal.does_asset_exist(ch.split(".")[0]):
        return eal.load_asset(ch.split(".")[0])
    return None


def tuple_vecteur(r):
    """Les fonctions de navigation rendent un Vector, un tuple (bool, Vector) ou None selon la version."""
    if r is None:
        return None
    if isinstance(r, unreal.Vector):
        return r
    if isinstance(r, (tuple, list)):
        for x in r:
            if isinstance(x, unreal.Vector):
                ok = [y for y in r if isinstance(y, bool)]
                if ok and not ok[0]:
                    return None
                return x
    return None


# ------------------------------------------------------------------ tenues
def regler_sampler(expr, tex):
    st = unreal.MaterialSamplerType
    try:
        cs = tex.get_editor_property("compression_settings")
        srgb = tex.get_editor_property("srgb")
        if cs == unreal.TextureCompressionSettings.TC_NORMALMAP:
            v = st.SAMPLERTYPE_NORMAL
        elif cs == unreal.TextureCompressionSettings.TC_MASKS:
            v = st.SAMPLERTYPE_MASKS
        elif cs == unreal.TextureCompressionSettings.TC_GRAYSCALE:
            v = st.SAMPLERTYPE_GRAYSCALE if srgb else st.SAMPLERTYPE_LINEAR_GRAYSCALE
        else:
            v = st.SAMPLERTYPE_COLOR if srgb else st.SAMPLERTYPE_LINEAR_COLOR
        expr.set_editor_property("sampler_type", v)
    except Exception as e:
        log("   (type de texture non regle pour %s : %s)" % (tex.get_name(), e))


def relier(de, sortie, vers, entree):
    ok = unreal.MaterialEditingLibrary.connect_material_expressions(de, sortie, vers, entree)
    if ok is False:
        log("   ATTENTION : liaison %s -> %s.%s ratee" % (de.get_name(), vers.get_name(), entree))
    return ok


def creer_matiere_teinte():
    """M_Bandit_clothes_Teinte : tissu du pack en gris x Couleur x Intensite."""
    ch = DOSSIER_NOUVEAUX + "/M_Bandit_clothes_Teinte_v2"
    if eal.does_asset_exist(ch):
        return eal.load_asset(ch)
    tex_couleur = charger("/Game/Bandit_Sport/Textures/T_Thief_clothes_BaseColor_blue")
    tex_normal = charger("/Game/Bandit_Sport/Textures/T_Thief_clothes_Normal")
    tex_orm = charger("/Game/Bandit_Sport/Textures/T_Thief_clothes_OcclusionRoughnessMetallic")
    if not tex_couleur:
        raise RuntimeError("texture T_Thief_clothes_BaseColor_blue introuvable")
    outils = unreal.AssetToolsHelpers.get_asset_tools()
    mat = outils.create_asset("M_Bandit_clothes_Teinte_v2", DOSSIER_NOUVEAUX, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_skeletal_mesh", True)
    mel = unreal.MaterialEditingLibrary
    MP = unreal.MaterialProperty

    t_c = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -1100, -200)
    t_c.set_editor_property("texture", tex_couleur)
    regler_sampler(t_c, tex_couleur)
    gris = mel.create_material_expression(mat, unreal.MaterialExpressionDesaturation, -800, -200)
    mel.connect_material_expressions(t_c, "RGB", gris, "")
    couleur = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -800, -20)
    couleur.set_editor_property("parameter_name", "Couleur")
    couleur.set_editor_property("default_value", unreal.LinearColor(1.0, 0.32, 0.02, 1.0))
    inten = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -560, 60)
    inten.set_editor_property("parameter_name", "Intensite")
    inten.set_editor_property("default_value", INTENSITE_TEINTE)
    m1 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -560, -150)
    mel.connect_material_expressions(gris, "", m1, "A")
    relier(couleur, "", m1, "B")
    m2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -330, -100)
    mel.connect_material_expressions(m1, "", m2, "A")
    mel.connect_material_expressions(inten, "", m2, "B")
    mel.connect_material_property(m2, "", MP.MP_BASE_COLOR)

    if tex_normal:
        t_n = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -800, 200)
        t_n.set_editor_property("texture", tex_normal)
        regler_sampler(t_n, tex_normal)
        mel.connect_material_property(t_n, "RGB", MP.MP_NORMAL)
    if tex_orm:
        t_o = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -800, 450)
        t_o.set_editor_property("texture", tex_orm)
        regler_sampler(t_o, tex_orm)
        mel.connect_material_property(t_o, "R", MP.MP_AMBIENT_OCCLUSION)
        mel.connect_material_property(t_o, "G", MP.MP_ROUGHNESS)
        mel.connect_material_property(t_o, "B", MP.MP_METALLIC)

    mel.recompile_material(mat)
    eal.save_asset(ch, only_if_is_dirty=False)
    log("   matiere creee : %s" % ch)
    return mat


def creer_matiere_deux_teintes():
    """M_Bandit_clothes_DeuxTeintes : haut du corps = CouleurHaut, bas = CouleurBas
    (separation a la taille, reglable avec Decalage en cm, positif = plus bas)."""
    ch = DOSSIER_NOUVEAUX + "/M_Bandit_clothes_DeuxTeintes_v2"
    if eal.does_asset_exist(ch):
        return eal.load_asset(ch)
    tex_couleur = charger("/Game/Bandit_Sport/Textures/T_Thief_clothes_BaseColor_blue")
    tex_normal = charger("/Game/Bandit_Sport/Textures/T_Thief_clothes_Normal")
    tex_orm = charger("/Game/Bandit_Sport/Textures/T_Thief_clothes_OcclusionRoughnessMetallic")
    outils = unreal.AssetToolsHelpers.get_asset_tools()
    mat = outils.create_asset("M_Bandit_clothes_DeuxTeintes_v2", DOSSIER_NOUVEAUX, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_skeletal_mesh", True)
    mel = unreal.MaterialEditingLibrary
    MP = unreal.MaterialProperty
    E = mel.create_material_expression

    t_c = E(mat, unreal.MaterialExpressionTextureSample, -1300, -300)
    t_c.set_editor_property("texture", tex_couleur)
    regler_sampler(t_c, tex_couleur)
    gris = E(mat, unreal.MaterialExpressionDesaturation, -1000, -300)
    mel.connect_material_expressions(t_c, "RGB", gris, "")

    # hauteur du pixel par rapport au centre du personnage
    wp = E(mat, unreal.MaterialExpressionWorldPosition, -1600, 100)
    op = E(mat, unreal.MaterialExpressionObjectPositionWS, -1600, 200)
    sub = E(mat, unreal.MaterialExpressionSubtract, -1400, 150)
    mel.connect_material_expressions(wp, "", sub, "A")
    mel.connect_material_expressions(op, "", sub, "B")
    masque = E(mat, unreal.MaterialExpressionComponentMask, -1250, 150)
    masque.set_editor_property("r", False)
    masque.set_editor_property("g", False)
    masque.set_editor_property("b", True)
    masque.set_editor_property("a", False)
    mel.connect_material_expressions(sub, "", masque, "")
    dec = E(mat, unreal.MaterialExpressionScalarParameter, -1250, 300)
    dec.set_editor_property("parameter_name", "Decalage")
    dec.set_editor_property("default_value", 0.0)
    plus = E(mat, unreal.MaterialExpressionAdd, -1100, 180)
    mel.connect_material_expressions(masque, "", plus, "A")
    mel.connect_material_expressions(dec, "", plus, "B")
    raide = E(mat, unreal.MaterialExpressionMultiply, -950, 180)
    raide.set_editor_property("const_b", 0.2)
    mel.connect_material_expressions(plus, "", raide, "A")
    demi = E(mat, unreal.MaterialExpressionAdd, -820, 180)
    demi.set_editor_property("const_b", 0.5)
    mel.connect_material_expressions(raide, "", demi, "A")
    sat = E(mat, unreal.MaterialExpressionSaturate, -700, 180)
    mel.connect_material_expressions(demi, "", sat, "")

    haut = E(mat, unreal.MaterialExpressionVectorParameter, -900, -120)
    haut.set_editor_property("parameter_name", "CouleurHaut")
    haut.set_editor_property("default_value", unreal.LinearColor(0.85, 0.02, 0.02, 1.0))
    bas = E(mat, unreal.MaterialExpressionVectorParameter, -900, 20)
    bas.set_editor_property("parameter_name", "CouleurBas")
    bas.set_editor_property("default_value", unreal.LinearColor(0.02, 0.02, 0.02, 1.0))
    lerp = E(mat, unreal.MaterialExpressionLinearInterpolate, -560, -40)
    relier(bas, "", lerp, "A")
    relier(haut, "", lerp, "B")
    mel.connect_material_expressions(sat, "", lerp, "Alpha")

    inten = E(mat, unreal.MaterialExpressionScalarParameter, -560, 120)
    inten.set_editor_property("parameter_name", "Intensite")
    inten.set_editor_property("default_value", INTENSITE_TEINTE)
    m1 = E(mat, unreal.MaterialExpressionMultiply, -400, -200)
    mel.connect_material_expressions(gris, "", m1, "A")
    mel.connect_material_expressions(lerp, "", m1, "B")
    m2 = E(mat, unreal.MaterialExpressionMultiply, -250, -120)
    mel.connect_material_expressions(m1, "", m2, "A")
    mel.connect_material_expressions(inten, "", m2, "B")
    mel.connect_material_property(m2, "", MP.MP_BASE_COLOR)
    if tex_normal:
        t_n = E(mat, unreal.MaterialExpressionTextureSample, -560, 300)
        t_n.set_editor_property("texture", tex_normal)
        regler_sampler(t_n, tex_normal)
        mel.connect_material_property(t_n, "RGB", MP.MP_NORMAL)
    if tex_orm:
        t_o = E(mat, unreal.MaterialExpressionTextureSample, -560, 550)
        t_o.set_editor_property("texture", tex_orm)
        regler_sampler(t_o, tex_orm)
        mel.connect_material_property(t_o, "R", MP.MP_AMBIENT_OCCLUSION)
        mel.connect_material_property(t_o, "G", MP.MP_ROUGHNESS)
        mel.connect_material_property(t_o, "B", MP.MP_METALLIC)
    mel.recompile_material(mat)
    eal.save_asset(ch, only_if_is_dirty=False)
    log("   matiere creee : %s" % ch)
    return mat


def preparer_tenues_boss():
    """Une tenue deux couleurs par boss ; secours = noir."""
    res = []
    try:
        parent = creer_matiere_deux_teintes()
        mel = unreal.MaterialEditingLibrary
        outils = unreal.AssetToolsHelpers.get_asset_tools()
        for nom, h, b in BOSS_TENUES:
            ch = "%s/MI_Bandit_clothes_%s" % (DOSSIER_NOUVEAUX, nom)
            if eal.does_asset_exist(ch):
                mi = eal.load_asset(ch)
            else:
                mi = outils.create_asset("MI_Bandit_clothes_%s" % nom, DOSSIER_NOUVEAUX,
                                         unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            mel.set_material_instance_parent(mi, parent)
            mel.set_material_instance_vector_parameter_value(mi, "CouleurHaut", unreal.LinearColor(h[0], h[1], h[2], 1.0))
            mel.set_material_instance_vector_parameter_value(mi, "CouleurBas", unreal.LinearColor(b[0], b[1], b[2], 1.0))
            mel.update_material_instance(mi)
            eal.save_asset(ch, only_if_is_dirty=False)
            res.append((nom, mi))
            log("   tenue %s prete" % nom)
    except Exception as e:
        log("   ATTENTION : tenues des boss impossibles (%s) -> boss en noir" % e)
    return res


def creer_instance(nom_couleur, parent, rgb):
    ch = "%s/MI_Bandit_clothes_%s" % (DOSSIER_NOUVEAUX, nom_couleur)
    mel = unreal.MaterialEditingLibrary
    if eal.does_asset_exist(ch):
        mi = eal.load_asset(ch)
    else:
        outils = unreal.AssetToolsHelpers.get_asset_tools()
        mi = outils.create_asset("MI_Bandit_clothes_%s" % nom_couleur, DOSSIER_NOUVEAUX,
                                 unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.set_material_instance_vector_parameter_value(mi, "Couleur", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    mel.update_material_instance(mi)
    eal.save_asset(ch, only_if_is_dirty=False)
    return mi


def preparer_tenues():
    tenues = {}
    for nom, ch in MATERIAUX_PACK.items():
        m = charger(ch)
        if m:
            tenues[nom] = m
        else:
            log("   ATTENTION : %s introuvable" % ch)
    try:
        parent = creer_matiere_teinte()
        for nom, rgb in TEINTES.items():
            tenues[nom] = creer_instance(nom, parent, rgb)
            log("   tenue %s prete" % nom)
    except Exception as e:
        log("   ATTENTION : tenues teintees impossibles (%s) -> couleurs de secours" % e)
    for nom, sec in SECOURS.items():
        if nom not in tenues and sec in tenues:
            tenues[nom] = tenues[sec]
    return tenues


def emplacements_habits(mesh):
    """Index des emplacements de materiau 'clothes' de SK_Bandit_FPS."""
    idx = []
    for i, sm in enumerate(mesh.get_editor_property("materials")):
        mat = sm.get_editor_property("material_interface")
        texte = (str(sm.get_editor_property("material_slot_name")) + " " + (chemin(mat) or "")).lower()
        if "cloth" in texte:
            idx.append(i)
    return idx


def habiller(acteur, mesh_bandit, habits, matiere):
    comp = corps(acteur)
    if comp is None:
        return False
    try:
        comp.set_editor_property("skeletal_mesh", mesh_bandit)
    except Exception:
        comp.set_skeletal_mesh(mesh_bandit, True)
    nb = len(mesh_bandit.get_editor_property("materials"))
    surcharges = [None] * nb
    for i in habits:
        surcharges[i] = matiere
    comp.set_editor_property("override_materials", surcharges)
    ajouter_tag(acteur, TAG_HABILLE)
    return True


# ------------------------------------------------------------------ positions
def depart_joueur(acteurs):
    for a in acteurs:
        if isinstance(a, unreal.PlayerStart):
            return a.get_actor_location()
    for a in acteurs:
        if isinstance(a, unreal.Pawn):
            try:
                if a.get_editor_property("auto_possess_player") != unreal.AutoReceiveInput.DISABLED:
                    return a.get_actor_location()
            except Exception:
                pass
    return None


def zone_navmesh(acteurs):
    for a in acteurs:
        if isinstance(a, unreal.NavMeshBoundsVolume):
            o, e = a.get_actor_bounds(False)
            if a.get_actor_label() == "NavMesh_Chelles" or a_tag(a, "ChellesNavMesh"):
                return o, e
    vols = [a for a in acteurs if isinstance(a, unreal.NavMeshBoundsVolume)]
    if vols:
        return vols[0].get_actor_bounds(False)
    return None


def lire_impact(hit):
    """(point, normale, acteur) d'un HitResult, selon la version de l'API."""
    try:
        return (hit.get_editor_property("impact_point"), hit.get_editor_property("impact_normal"),
                hit.get_editor_property("actor"))
    except Exception:
        pass
    t = hit.to_tuple()
    return t[5], t[7], t[9]


def tracer(monde, debut, fin):
    try:
        hit = unreal.SystemLibrary.line_trace_single(
            monde, debut, fin, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
            unreal.DrawDebugTrace.NONE, True)
    except Exception:
        return None
    if hit is None:
        return None
    try:
        r = lire_impact(hit)
        if isinstance(hit, tuple) or r[0] is None:
            return None
        return r
    except Exception:
        return None


def sur_navmesh(monde, p):
    try:
        r = tuple_vecteur(unreal.NavigationSystemV1.project_point_to_navigation(
            monde, p, None, None, unreal.Vector(150.0, 150.0, 250.0)))
    except Exception:
        return None
    return r


MOTS_PAS_SOL = ("sky", "ciel", "neige", "snow", "cloud", "water", "eau", "tree", "arbre", "foliage")


def chercher_positions(monde, nombre, centre, etendue, joueur):
    """Tire des points au hasard sur toute la ville, descend jusqu'au sol, garde ceux
    qui sont au niveau de la rue (pas sur les toits) et sur le NavMesh si possible."""
    haut_z = centre.z + etendue.z
    bas_z = centre.z - etendue.z
    impacts = []
    debut = time.time()
    essais = 0
    while len(impacts) < nombre * 40 and essais < nombre * 150 and time.time() - debut < 90:
        essais += 1
        x = random.uniform(centre.x - etendue.x, centre.x + etendue.x)
        y = random.uniform(centre.y - etendue.y, centre.y + etendue.y)
        r = tracer(monde, unreal.Vector(x, y, haut_z), unreal.Vector(x, y, bas_z))
        if r is None:
            continue
        p, n, act = r
        if n is None or n.z < 0.8:
            continue
        if act is not None:
            if isinstance(act, unreal.Pawn):
                continue
            nom = (act.get_class().get_name() + " " + act.get_actor_label()).lower()
            if any(m in nom for m in MOTS_PAS_SOL):
                continue
        impacts.append(p)
    log("   %d points de sol trouves (%d tirs)" % (len(impacts), essais))
    if not impacts:
        return []

    # niveau de la rue : les toits sont bien plus hauts que la majorite des impacts
    zs = sorted(p.z for p in impacts)
    rue = zs[int(len(zs) * 0.3)]
    candidats = [p for p in impacts if p.z <= rue + 350.0]
    log("   niveau de la rue ~ %.1f m, %d points gardes (toits exclus)" % (rue / 100.0, len(candidats)))

    # NavMesh : on garde les points ou les ennemis pourront marcher (si le NavMesh repond)
    nav = []
    for p in candidats:
        q = sur_navmesh(monde, p)
        if q is not None and math.hypot(q.x - p.x, q.y - p.y) < 150.0 and abs(q.z - p.z) < 250.0:
            nav.append(p)
    if len(nav) >= nombre:
        candidats = nav
        log("   %d points sur le NavMesh" % len(nav))
    else:
        log("   NavMesh peu ou pas joignable depuis le script (%d points) : on garde le niveau de la rue" % len(nav))

    def dist(a, b):
        return math.hypot(a.x - b.x, a.y - b.y) / 100.0

    if joueur is not None:
        loin = [p for p in candidats if dist(p, joueur) >= LOIN_DU_JOUEUR]
        if len(loin) >= nombre:
            candidats = loin

    espacement = ESPACEMENT
    while True:
        random.shuffle(candidats)
        choisis = []
        for p in candidats:
            if all(dist(p, q) >= espacement for q in choisis):
                choisis.append(p)
                if len(choisis) == nombre:
                    log("   espacement utilise : %.0f m" % espacement)
                    return choisis
        if espacement < 4:
            log("   espacement utilise : %.0f m (place limitee)" % espacement)
            return choisis
        espacement *= 0.8


# ------------------------------------------------------------------ principal
def demander(texte):
    if os.environ.get("CHELLES_QUITTER") == "1":
        return True
    r = unreal.EditorDialog.show_message("Chelles 3D - Ennemis", texte, unreal.AppMsgType.YES_NO)
    return r == unreal.AppReturnType.YES


def main():
    open(JOURNAL, "w").close()
    if GRAINE is not None:
        random.seed(GRAINE)
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        log("chargement de %s ..." % CARTE)
        ell.load_level(CARTE)
        monde = ell.get_editor_world()
    log("Carte : %s" % monde.get_path_name())

    acteurs = ell.get_all_level_actors()
    sortes_connues = set(COULEUR_PAR_SORTE) | set(SORTES_GARDEES)
    ennemis = [a for a in acteurs if sorte(a) in sortes_connues]
    compte = {}
    for a in ennemis:
        compte[sorte(a)] = compte.get(sorte(a), 0) + 1
    boss_manquants = max(0, NB_BOSS - compte.get("AI_Soldier_Boss", 0))
    a_ajouter = max(boss_manquants, TOTAL_ENNEMIS - len(ennemis))
    resume = ", ".join("%s x%d" % (k, v) for k, v in sorted(compte.items())) or "aucun"
    log("Ennemis deja dans la carte : %d (%s)" % (len(ennemis), resume))

    zone = zone_navmesh(acteurs)
    if zone is None:
        log("ABANDON : pas de NavMesh dans Rachid. Lancez d'abord Tools/NavMesh/ajouter_navmesh.py puis Build > Build Paths.")
        return

    if not demander("Ennemis deja dans Rachid : %d\n(%s)\n\n"
                    "Le script va :\n"
                    "- habiller les ennemis non-bandits en bandits de couleurs differentes\n"
                    "  (le premier bandit reste bleu) ;\n"
                    "- en ajouter %d pour arriver a %d (dont %d boss geants x%g) ;\n"
                    "- les eparpiller tous sur la ville (%d m entre eux).\n\n"
                    "Une copie de Rachid est faite avant. Continuer ?"
                    % (len(ennemis), resume, a_ajouter, TOTAL_ENNEMIS, NB_BOSS, TAILLE_BOSS, int(ESPACEMENT))):
        log("Annule par l'utilisateur. Rien n'a ete modifie.")
        return

    # ---- 1. sauvegarde
    ell.save_current_level()
    fichier = os.path.join(PROJET, "Content", "Maps", "Rachid.umap")
    dossier = os.path.join(ICI, "sauvegarde")
    os.makedirs(dossier, exist_ok=True)
    copie = os.path.join(dossier, "Rachid_%s.umap" % time.strftime("%Y-%m-%d_%H%M"))
    shutil.copy2(fichier, copie)
    log("1/6 sauvegarde : %s" % copie)

    # etat d'origine (garde seulement au 1er passage, pour pouvoir tout remettre)
    if not os.path.exists(ETAT):
        etat = {}
        for a in ennemis:
            if a_tag(a, TAG_AJOUTE):
                continue
            c = corps(a)
            etat[a.get_name()] = {
                "label": a.get_actor_label(),
                "position": [a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z],
                "rotation": [a.get_actor_rotation().pitch, a.get_actor_rotation().yaw, a.get_actor_rotation().roll],
                "dossier": str(a.get_folder_path()),
                "echelle": [a.get_actor_scale3d().x, a.get_actor_scale3d().y, a.get_actor_scale3d().z],
                "mesh": chemin(c.get_editor_property("skeletal_mesh")) if c else None,
                "materiaux": [chemin(m) for m in c.get_editor_property("override_materials")] if c else [],
            }
        with open(ETAT, "w", encoding="utf-8") as f:
            json.dump(etat, f, indent=2, ensure_ascii=False)
        log("   etat d'origine de %d ennemis note dans %s" % (len(etat), ETAT))

    # ---- 2. tenues
    log("2/6 preparation des tenues ...")
    tenues = preparer_tenues()
    mesh_bandit = charger(MESH_BANDIT)
    if mesh_bandit is None:
        log("ABANDON : %s introuvable." % MESH_BANDIT)
        return
    habits = emplacements_habits(mesh_bandit)
    if not habits:
        log("ABANDON : aucun emplacement 'clothes' dans SK_Bandit_FPS.")
        return
    log("   emplacements de vetements dans SK_Bandit_FPS : %s" % habits)

    # ---- 3. ajouts
    classes = {}
    for s in COULEUR_PAR_SORTE:
        c = unreal.EditorAssetLibrary.load_blueprint_class(DOSSIER_IA + s)
        if c:
            classes[s] = c
        else:
            log("   ATTENTION : Blueprint %s introuvable" % s)
    tirage = [s for s, poids in MELANGE_AJOUTS if s in classes for _ in range(poids)]
    centre, etendue = zone
    pos_tmp = unreal.Vector(centre.x, centre.y, centre.z)
    ajoutes = 0
    for i in range(a_ajouter):
        s = "AI_Soldier_Boss" if i < boss_manquants else random.choice(tirage)
        if s not in classes:
            continue
        a = ell.spawn_actor_from_class(classes[s], pos_tmp, unreal.Rotator(0, 0, 0))
        if a is None:
            log("   ERREUR : impossible de poser %s" % s)
            continue
        a.set_actor_label("%s_Chelles_%02d" % (s, i + 1))
        ajouter_tag(a, TAG_AJOUTE)
        ennemis.append(a)
        ajoutes += 1
    log("3/6 %d ennemis ajoutes" % ajoutes)

    # ---- 4. habillage
    par_couleur = {}
    for a in ennemis:
        s = sorte(a)
        if s not in COULEUR_PAR_SORTE:
            continue
        nom = COULEUR_PAR_SORTE[s]
        mat = tenues.get(nom)
        if mat is None:
            continue
        if habiller(a, mesh_bandit, habits, mat):
            par_couleur[nom] = par_couleur.get(nom, 0) + 1
    boss = sorted([a for a in ennemis if sorte(a) == "AI_Soldier_Boss"],
                  key=lambda a: (a_tag(a, TAG_AJOUTE), a.get_actor_label()))
    for a in boss[NB_BOSS:]:  # boss en trop ajoutes par un passage precedent
        if a_tag(a, TAG_AJOUTE):
            ennemis.remove(a)
            ell.destroy_actor(a)
    boss = [a for a in boss if a in ennemis]
    tenues_boss = preparer_tenues_boss()
    for i, a in enumerate(boss):
        a.set_actor_scale3d(unreal.Vector(TAILLE_BOSS, TAILLE_BOSS, TAILLE_BOSS))
        if tenues_boss:
            nom, mi = tenues_boss[i % len(tenues_boss)]
            habiller(a, mesh_bandit, habits, mi)
            par_couleur["Noir"] = par_couleur.get("Noir", 0) - 1
            par_couleur[nom] = par_couleur.get(nom, 0) + 1
    par_couleur = {k: v for k, v in par_couleur.items() if v > 0}
    log("   %d boss, taille x%g" % (len(boss), TAILLE_BOSS))
    log("4/6 habilles : %s (bandits bleus inchanges)" %
        ", ".join("%s x%d" % kv for kv in sorted(par_couleur.items())))

    # ---- 5. eparpillement
    joueur = depart_joueur(acteurs)
    log("5/6 recherche de places dans la ville (%.0f x %.0f m) ..." % (etendue.x / 50.0, etendue.y / 50.0))
    places = chercher_positions(monde, len(ennemis), centre, etendue, joueur)
    if len(places) < len(ennemis):
        log("   ATTENTION : seulement %d places pour %d ennemis : les autres ne bougent pas. "
            "Envoyez Tools/Ennemis/ennemis.log." % (len(places), len(ennemis)))
    random.shuffle(ennemis)
    ennemis.sort(key=lambda a: sorte(a) != "AI_Soldier_Boss")
    for a, p in zip(ennemis, places):
        rot = unreal.Rotator(0.0, 0.0, random.uniform(-180.0, 180.0))
        a.set_actor_location_and_rotation(unreal.Vector(p.x, p.y, p.z + 110.0 * a.get_actor_scale3d().z), rot, False, True)
        a.set_folder_path(DOSSIER_OUTLINER)
    for a in ennemis[len(places):]:
        if a_tag(a, TAG_AJOUTE):  # un ennemi ajoute sans place : on l'enleve
            ell.destroy_actor(a)
    log("   %d ennemis eparpilles (dossier Outliner : %s)" % (min(len(places), len(ennemis)), DOSSIER_OUTLINER))

    # ---- 6. enregistrement
    if ell.save_current_level():
        log("6/6 Rachid enregistree.")
    else:
        log("6/6 ATTENTION : Rachid n'a pas pu etre enregistree. Faites File > Save Current.")
    log("TERMINE. Lancez Play pour voir les ennemis. Annuler : Tools/Ennemis/retirer_ennemis.py")


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
