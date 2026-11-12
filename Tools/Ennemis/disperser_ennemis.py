# -*- coding: utf-8 -*-
"""ENNEMIS VARIES ET EPARPILLES dans la carte /Game/Maps/Rachid.

A lancer DANS L'EDITEUR (Unreal ouvert, carte Rachid) :
    File > Execute Python Script... > Tools/Ennemis/disperser_ennemis.py
ou double-clic sur DISPERSER_ENNEMIS.bat (Unreal FERME : il l'ouvre, travaille, enregistre et referme).

Ce que fait le script :
  1. enregistre Rachid puis copie Rachid.umap dans Tools/Ennemis/sauvegarde/ ;
  2. cree des tenues de couleur (dossier /Game/Bandit_Sport/Material/Chelles) :
       rouge, vert, noir = materiaux du pack Bandit_Sport ;
       orange, violet, jaune = nouvelle matiere "teinte" (M_Bandit_clothes_Teinte) ;
  3. habille chaque sorte d'ennemi en bandit avec SA couleur (le premier bandit reste BLEU) :
       AI_Soldier      -> rouge        AI_Charger -> vert       AI_Patrol  -> violet
       AI_Soldier_Boss -> noir         AI_Genesis -> orange     AI_Patrol1 -> jaune
  4. met exactement NB_BOSS boss (3), TAILLE_BOSS fois plus grands (x3), puis ajoute
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
INTENSITE_TEINTE = 2.4      # plus grand = tenue plus claire (modifiable ensuite dans M_Bandit_clothes_Teinte)
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


def creer_matiere_teinte():
    """M_Bandit_clothes_Teinte : tissu du pack en gris x Couleur x Intensite."""
    ch = DOSSIER_NOUVEAUX + "/M_Bandit_clothes_Teinte"
    if eal.does_asset_exist(ch):
        return eal.load_asset(ch)
    tex_couleur = charger("/Game/Bandit_Sport/Textures/T_Thief_clothes_BaseColor_blue")
    tex_normal = charger("/Game/Bandit_Sport/Textures/T_Thief_clothes_Normal")
    tex_orm = charger("/Game/Bandit_Sport/Textures/T_Thief_clothes_OcclusionRoughnessMetallic")
    if not tex_couleur:
        raise RuntimeError("texture T_Thief_clothes_BaseColor_blue introuvable")
    outils = unreal.AssetToolsHelpers.get_asset_tools()
    mat = outils.create_asset("M_Bandit_clothes_Teinte", DOSSIER_NOUVEAUX, unreal.Material, unreal.MaterialFactoryNew())
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
    mel.connect_material_expressions(couleur, "RGB", m1, "B")
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


def chercher_positions(monde, nombre, centre, rayon, joueur):
    nav = unreal.NavigationSystemV1
    candidats = []
    essais = 0
    debut = time.time()
    while len(candidats) < nombre * 25 and essais < nombre * 60 and time.time() - debut < 60:
        essais += 1
        p = None
        try:
            p = tuple_vecteur(nav.get_random_reachable_point_in_radius(monde, centre, rayon))
        except Exception:
            p = None
        if p is None:
            try:
                p = tuple_vecteur(nav.get_random_point_in_navigable_radius(monde, centre, rayon))
            except Exception:
                p = None
        if p is not None:
            candidats.append(p)
    log("   %d points du NavMesh trouves (%d essais)" % (len(candidats), essais))
    if not candidats:
        return []

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
    nb_boss = 0
    for a in ennemis:
        if sorte(a) == "AI_Soldier_Boss":
            a.set_actor_scale3d(unreal.Vector(TAILLE_BOSS, TAILLE_BOSS, TAILLE_BOSS))
            nb_boss += 1
    log("   %d boss mis a la taille x%g" % (nb_boss, TAILLE_BOSS))
    log("4/6 habilles : %s (bandits bleus inchanges)" %
        ", ".join("%s x%d" % kv for kv in sorted(par_couleur.items())))

    # ---- 5. eparpillement
    joueur = depart_joueur(acteurs)
    rayon = math.hypot(etendue.x, etendue.y)
    origine = joueur if joueur is not None else centre
    if joueur is not None:
        rayon += math.hypot(joueur.x - centre.x, joueur.y - centre.y)
    log("5/6 recherche de places sur le NavMesh (rayon %.0f m autour de %s) ..." %
        (rayon / 100.0, "depart du joueur" if joueur is not None else "centre de la ville"))
    places = chercher_positions(monde, len(ennemis), origine, rayon, joueur)
    if len(places) < len(ennemis):
        log("   ATTENTION : seulement %d places pour %d ennemis : les autres ne bougent pas. "
            "Le NavMesh est-il construit (Build > Build Paths) ?" % (len(places), len(ennemis)))
    random.shuffle(ennemis)
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
