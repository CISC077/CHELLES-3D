"""SCRIPT A LANCER DANS UNREAL 4.27 : File > Execute Python Script... > ce fichier.
(n'importe quelle carte ouverte : le script ouvre lui-meme les bonnes cartes)

SOL "STYLIZED LANDSCAPE" + DECOR COMME SUR LA PHOTO DU PACK, DANS LA CARTE Rachid.

Ce que fait le script :
  1. SOL : il cree /Game/Chelles/StylizedLandscape/M_SolStylized (+ MI_SolStylized) qui reprend le
     materiau "auto" du pack Stylized Landscape (herbe vert vif sur le plat, roche dans les pentes,
     memes reglages que MI_landscape_material_Inst du pack). Il le pose sur le Landscape de Rachid
     (a la place du sol cartoon) et sur les dalles Chelles_Tile_Ground.
     Pas besoin de peindre de couche : le materiau se pose tout seul selon la pente.
  2. DECOR : il ouvre la carte de demo du pack (Stylized_Landscape_Demo), repere la rotonde a colonnes
     couverte de lierre, et COPIE tout ce qui l'entoure (rotonde, escaliers, ruines, rochers, lierre,
     arbres, buissons, fleurs) dans Rachid.
  3. ENDROIT : dans Rachid, il cherche tout seul un terrain libre et assez plat (pas de route, pas de
     batiment, pas de trottoir, pas de lampadaire) le plus pres possible du point de depart du joueur.
     Le decor est pose sur le sol a cet endroit, + un petit lac entoure de pierres si la copie n'en a pas.
     L'herbe haute et les fleurs du pack ne poussent QUE dans cette zone (pas sur les routes).
  4. Il deplace la camera de l'editeur sur le decor pour que tu le voies tout de suite.

Rien n'est efface : les anciens materiaux sont notes dans Tools/StylizedLandscape/journal_stylized.json,
une copie de Rachid.umap est faite dans Tools/StylizedLandscape/sauvegarde/ AVANT toute modification,
et annuler_stylized_landscape.py remet tout comme avant (et retire seulement ce que ce script a ajoute).
Journal texte : Tools/out/stylized_landscape.log
"""
import datetime, json, math, os, random, shutil, traceback
import unreal

# ----------------------------------------------------------------------------------------------- reglages
CARTE = "/Game/Maps/Rachid"
CARTE_DEMO = "/Game/Stylized_Landscape/maps/Stylized_Landscape_Demo"
PACK = "/Game/Stylized_Landscape"
DOSSIER_ASSETS = "/Game/Chelles/StylizedLandscape"
MF_AUTO = PACK + "/materials/landscape_materials/layers/MF_auto"
MI_PACK = PACK + "/materials/landscape_materials/MI_landscape_material_Inst"
MASQUE_HERBE = PACK + "/textures/landscape_textures/T_landscape_Mask_B"
MI_EAU = PACK + "/materials/water_materials/MI_water_Inst"
TYPES_HERBE = ["Grass_A", "Grass_B", "Grass_C", "Grass_D", "Grass_E"]

FAIRE_SOL = True                # materiau du pack sur le Landscape
FAIRE_DALLES_SOL = True         # ... et sur les dalles Chelles_Tile_Ground
FAIRE_DECOR = True              # copie du decor de la demo
FAIRE_LAC = True                # petit lac + pierres si la demo n'en donne pas
FAIRE_HERBE_ZONE = True         # herbe haute / fleurs du pack, seulement dans la zone du decor

RAYON_COPIE_M = 40.0            # on copie ce qui est a moins de 40 m de la rotonde
MAX_PLANTES = 900               # nombre max d'arbres/buissons/fleurs copies (fluidite)
COPIER_HERBE_FOLIAGE = False    # l'herbe peinte de la demo n'est pas copiee (l'herbe auto la remplace)
PENTE_MAX_M = 4.0               # ecart de hauteur max accepte dans la zone
CASE_M = 10.0                   # pas de la recherche d'un terrain libre
PART_OBSTACLES_MAX = 0.02       # 2 % de points "pas sol" toleres (un petit buisson...)

# Pour imposer l'endroit au lieu de la recherche automatique : ZONE_CENTRE = (x_cm, y_cm)
# (coordonnees Unreal, en bas a droite de la vue quand on clique au sol, ou "Location" d'un acteur)
ZONE_CENTRE = None
# Pour imposer la rotonde de la demo : CENTRE_DEMO = (x_cm, y_cm)
CENTRE_DEMO = None

TAG = "ChellesDecorStylized"
DOSSIER_OUTLINER = "Chelles/DecorStylized"

# ----------------------------------------------------------------------------------------------- fichiers
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DOSSIER_OUTIL = os.path.join(PROJET, "Tools", "StylizedLandscape")
JOURNAL_JSON = os.path.join(DOSSIER_OUTIL, "journal_stylized.json")
FICHIER_LOG = os.path.join(PROJET, "Tools", "out", "stylized_landscape.log")
_lignes = []
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
ELL = unreal.EditorLevelLibrary
SL = unreal.SystemLibrary
TITRE = "Stylized Landscape"


def log(msg):
    unreal.log("[Stylized] " + msg)
    _lignes.append(msg)


def ecrire_log():
    try:
        os.makedirs(os.path.dirname(FICHIER_LOG), exist_ok=True)
        with open(FICHIER_LOG, "a", encoding="utf-8") as f:
            f.write("\n===== %s =====\n%s\n" % (datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"), "\n".join(_lignes)))
    except Exception:
        pass


def message(texte, oui_non=False):
    t = unreal.AppMsgType.YES_NO if oui_non else unreal.AppMsgType.OK
    r = unreal.EditorDialog.show_message(TITRE, texte, t)
    return r == unreal.AppReturnType.YES


def charger_journal():
    try:
        with open(JOURNAL_JSON, "r", encoding="utf-8") as f:
            j = json.load(f)
    except Exception:
        j = {}
    j.setdefault("landscapes", {})
    j.setdefault("dalles", {})
    return j


def ecrire_journal(j):
    os.makedirs(DOSSIER_OUTIL, exist_ok=True)
    with open(JOURNAL_JSON, "w", encoding="utf-8") as f:
        json.dump(j, f, indent=1, ensure_ascii=False)


def chemin(obj):
    return obj.get_path_name() if obj else None


def label(a):
    try:
        return a.get_actor_label()
    except Exception:
        return a.get_name()


def monde():
    return ELL.get_editor_world()


def nom_carte():
    w = monde()
    return w.get_name() if w else "?"


# ----------------------------------------------------------------------------------------------- traces au sol
def _lire_hit(hit):
    try:
        t = unreal.GameplayStatics.break_hit_result(hit)
    except Exception:
        t = hit.to_tuple()
    if not isinstance(t, (list, tuple)):
        t = [t]
    loc = act = None
    for v in t:
        if loc is None and isinstance(v, unreal.Vector):
            loc = v
        if act is None and isinstance(v, unreal.Actor):
            act = v
    return loc, act


_IGNORES = []


def preparer_ignores(acteurs):
    """Ciel, brouillard, grands volumes... : enormes et sans interet pour trouver le sol."""
    global _IGNORES
    _IGNORES = []
    for a in acteurs:
        try:
            if est_landscape(a) or label(a).startswith(("Chelles_", "CH_")):
                continue
            nom_cls = a.get_class().get_name().lower()
            o, e = a.get_actor_bounds(False)
            if "sky" in nom_cls or max(e.x, e.y) > 150000:
                _IGNORES.append(a)
        except Exception:
            pass


def premier_impact(x, y, z_haut, z_bas):
    hit = SL.line_trace_single(monde(), unreal.Vector(x, y, z_haut), unreal.Vector(x, y, z_bas),
                               unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, _IGNORES,
                               unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None, None
    return _lire_hit(hit)


def est_landscape(a):
    return isinstance(a, unreal.LandscapeProxy)


def est_sol_rachid(a):
    if a is None:
        return False
    if est_landscape(a):
        return True
    return label(a).startswith("Chelles_Tile_Ground")


def hauteur_sol(x, y, z_haut, z_bas, test_sol):
    """Altitude du sol (en passant a travers ce qui n'est pas du sol)."""
    z = z_haut
    for _ in range(10):
        loc, act = premier_impact(x, y, z, z_bas)
        if loc is None:
            return None
        if test_sol(act):
            return loc.z
        z = loc.z - 5.0
    return None


def bornes_z(landscapes):
    if not landscapes:
        return 100000.0, -100000.0
    zmin, zmax = 1e12, -1e12
    for l in landscapes:
        o, e = l.get_actor_bounds(False)
        zmin = min(zmin, o.z - e.z - 30000)
        zmax = max(zmax, o.z + e.z + 30000)
    return zmax, zmin


# ----------------------------------------------------------------------------------------------- materiau
def _expr(mat, classe, x, y):
    return MEL.create_material_expression(mat, classe, x, y)


def _lier(a, sortie, b, entree):
    if not MEL.connect_material_expressions(a, sortie, b, entree):
        raise RuntimeError("liaison impossible %s.%s -> %s.%s" % (a.get_name(), sortie, b.get_name(), entree))


def _ajouter_herbe(mat):
    """Herbe haute + fleurs du pack (Landscape Grass), seulement dans un disque autour du decor."""
    types = []
    for n in TYPES_HERBE:
        gt = EAL.load_asset(PACK + "/materials/landscape_materials/%s_LandscapeGrassType" % n)
        if gt:
            types.append((n, gt))
    if not types:
        log("  Herbe du pack introuvable : pas d'herbe haute.")
        return
    sortie = _expr(mat, unreal.MaterialExpressionLandscapeGrassOutput, 400, 600)
    entrees = []
    for n, gt in types:
        gi = unreal.GrassInput()
        gi.set_editor_property("name", n)
        gi.set_editor_property("grass_type", gt)
        entrees.append(gi)
    sortie.set_editor_property("grass_types", entrees)

    # disque : SphereMask(position monde, CentreDecor, RayonDecor)
    wp = _expr(mat, unreal.MaterialExpressionWorldPosition, -900, 600)
    centre = _expr(mat, unreal.MaterialExpressionVectorParameter, -900, 700)
    centre.set_editor_property("parameter_name", "CentreDecor")
    centre.set_editor_property("default_value", unreal.LinearColor(0, 0, -1000000, 0))
    rayon = _expr(mat, unreal.MaterialExpressionScalarParameter, -900, 820)
    rayon.set_editor_property("parameter_name", "RayonDecor")
    rayon.set_editor_property("default_value", 0.0)
    durete = _expr(mat, unreal.MaterialExpressionScalarParameter, -900, 900)
    durete.set_editor_property("parameter_name", "BordDecor")
    durete.set_editor_property("default_value", 0.3)
    sm = _expr(mat, unreal.MaterialExpressionSphereMask, -650, 650)
    _lier(wp, "", sm, "A")
    _lier(centre, "", sm, "B")
    _lier(rayon, "", sm, "Radius")
    _lier(durete, "", sm, "Hardness")

    # variations : masque du pack echantillonne en coordonnees monde
    tex = EAL.load_asset(MASQUE_HERBE)
    canaux = [None, "R", "G", "B", "A"]
    if tex:
        wp2 = _expr(mat, unreal.MaterialExpressionWorldPosition, -900, 1000)
        xy = _expr(mat, unreal.MaterialExpressionComponentMask, -750, 1000)
        xy.set_editor_property("r", True); xy.set_editor_property("g", True)
        xy.set_editor_property("b", False); xy.set_editor_property("a", False)
        _lier(wp2, "", xy, "")
        ech = _expr(mat, unreal.MaterialExpressionScalarParameter, -750, 1100)
        ech.set_editor_property("parameter_name", "EchelleMasqueHerbe")
        ech.set_editor_property("default_value", 1.0 / 3000.0)
        uv = _expr(mat, unreal.MaterialExpressionMultiply, -600, 1020)
        _lier(xy, "", uv, "A")
        _lier(ech, "", uv, "B")
        ts = _expr(mat, unreal.MaterialExpressionTextureSample, -450, 1000)
        ts.set_editor_property("texture", tex)
        try:
            ts.set_editor_property("sampler_type", type_echantillon(tex))
        except Exception:
            pass
        _lier(uv, "", ts, "UVs")
    for i, (n, gt) in enumerate(types):
        c = canaux[i] if i < len(canaux) else None
        if c is None or not tex:
            _lier(sm, "", sortie, n)
        else:
            m = _expr(mat, unreal.MaterialExpressionMultiply, -150, 650 + 80 * i)
            _lier(sm, "", m, "A")
            _lier(ts, c, m, "B")
            _lier(m, "", sortie, n)
    log("  Herbe du pack ajoutee (%d types), seulement autour du decor." % len(types))


def type_echantillon(tex):
    """Le bon 'Sampler Type' selon la compression de la texture (sinon le materiau ne compile pas)."""
    ST = unreal.MaterialSamplerType
    try:
        cs = tex.get_editor_property("compression_settings")
        srgb = bool(tex.get_editor_property("srgb"))
    except Exception:
        return ST.SAMPLERTYPE_COLOR
    TC = unreal.TextureCompressionSettings
    if cs == TC.TC_MASKS:
        return ST.SAMPLERTYPE_MASKS
    if cs == TC.TC_NORMALMAP:
        return ST.SAMPLERTYPE_NORMAL
    if cs == TC.TC_GRAYSCALE:
        return ST.SAMPLERTYPE_GRAYSCALE if srgb else ST.SAMPLERTYPE_LINEAR_GRAYSCALE
    if cs == TC.TC_ALPHA:
        return ST.SAMPLERTYPE_ALPHA
    return ST.SAMPLERTYPE_COLOR if srgb else ST.SAMPLERTYPE_LINEAR_COLOR


def construire_materiau():
    nom_mat, nom_mi = "M_SolStylized", "MI_SolStylized"
    ch_mat, ch_mi = DOSSIER_ASSETS + "/" + nom_mat, DOSSIER_ASSETS + "/" + nom_mi
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mf = EAL.load_asset(MF_AUTO)
    if mf is None:
        raise RuntimeError("MF_auto du pack introuvable : %s" % MF_AUTO)

    # le materiau (le notre) est toujours reconstruit : un essai rate ne reste pas casse
    if EAL.does_asset_exist(ch_mat):
        mat = EAL.load_asset(ch_mat)
        MEL.delete_all_material_expressions(mat)
        log("Materiau %s reconstruit" % ch_mat)
    else:
        mat = tools.create_asset(nom_mat, DOSSIER_ASSETS, unreal.Material, unreal.MaterialFactoryNew())
    if True:
        # MF_auto sort un "Material Attributes" : on le casse en BaseColor / Normal / Roughness...
        mat.set_editor_property("use_material_attributes", False)
        appel = _expr(mat, unreal.MaterialExpressionMaterialFunctionCall, -700, 0)
        appel.set_editor_property("material_function", mf)
        casse = _expr(mat, unreal.MaterialExpressionBreakMaterialAttributes, -400, 0)
        _lier(appel, "", casse, "")
        MP = unreal.MaterialProperty
        branches = 0
        for sortie, nom_prop in (("BaseColor", "MP_BASE_COLOR"), ("Metallic", "MP_METALLIC"),
                                 ("Specular", "MP_SPECULAR"), ("Roughness", "MP_ROUGHNESS"),
                                 ("Normal", "MP_NORMAL"), ("AmbientOcclusion", "MP_AMBIENT_OCCLUSION")):
            prop = getattr(MP, nom_prop, None)
            if prop is None:
                continue
            try:
                if MEL.connect_material_property(casse, sortie, prop):
                    branches += 1
            except Exception:
                pass
        if branches == 0:
            raise RuntimeError("MF_auto n'a pas pu etre branche au materiau")
        log("  MF_auto branche (%d sortie(s))" % branches)
        if FAIRE_HERBE_ZONE:
            try:
                _ajouter_herbe(mat)
            except Exception as e:
                log("  Herbe haute non ajoutee (%s) : le sol reste correct." % e)
        MEL.recompile_material(mat)
        EAL.save_loaded_asset(mat, False)
        log("Materiau cree : %s" % ch_mat)

    if EAL.does_asset_exist(ch_mi):
        mi = EAL.load_asset(ch_mi)
    else:
        mi = tools.create_asset(nom_mi, DOSSIER_ASSETS, unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, mat)
        copier_reglages_pack(mi)
        EAL.save_loaded_asset(mi, False)
        log("Instance creee : %s (pour regler les couleurs)" % ch_mi)
    return mi


def copier_reglages_pack(mi):
    """Reprend les reglages de MI_landscape_material_Inst du pack (couleurs, textures...)."""
    src = EAL.load_asset(MI_PACK)
    if src is None:
        return
    n = 0
    for lire_noms, lire, ecrire in (
            ("get_scalar_parameter_names", "get_material_instance_scalar_parameter_value",
             "set_material_instance_scalar_parameter_value"),
            ("get_vector_parameter_names", "get_material_instance_vector_parameter_value",
             "set_material_instance_vector_parameter_value"),
            ("get_texture_parameter_names", "get_material_instance_texture_parameter_value",
             "set_material_instance_texture_parameter_value")):
        try:
            noms = getattr(MEL, lire_noms)(src) or []
            noms_cible = set(str(x) for x in (getattr(MEL, lire_noms)(mi) or []))
        except Exception:
            continue
        for nom in noms:
            if str(nom) not in noms_cible:
                continue
            try:
                getattr(MEL, ecrire)(mi, nom, getattr(MEL, lire)(src, nom))
                n += 1
            except Exception:
                pass
    MEL.update_material_instance(mi)
    log("  %d reglage(s) repris du materiau du pack." % n)


def regler_zone_herbe(mi, centre, rayon_cm):
    try:
        MEL.set_material_instance_vector_parameter_value(
            mi, "CentreDecor", unreal.LinearColor(centre.x, centre.y, centre.z, 0))
        MEL.set_material_instance_scalar_parameter_value(mi, "RayonDecor", float(rayon_cm))
        MEL.update_material_instance(mi)
        EAL.save_loaded_asset(mi, False)
        SL.execute_console_command(monde(), "grass.FlushCache")
    except Exception as e:
        log("  Zone d'herbe non reglee : %s" % e)


# ----------------------------------------------------------------------------------------------- demo
CLASSES_COPIEES = ("StaticMeshActor", "DecalActor")


def _mesh_de(a):
    try:
        c = a.static_mesh_component
        return c.static_mesh if c else None
    except Exception:
        return None


def _est_eau(a):
    try:
        c = a.static_mesh_component
        for m in c.get_materials() or []:
            if m and "water" in m.get_name().lower():
                return True
    except Exception:
        pass
    return False


def _a_copier(a, rayon_xy):
    cls = a.get_class()
    nom_cls = cls.get_name()
    du_pack = cls.get_path_name().startswith(PACK + "/")
    if nom_cls not in CLASSES_COPIEES and not du_pack:
        return False
    if nom_cls == "StaticMeshActor":
        m = _mesh_de(a)
        if m is None:
            return False
    o, e = a.get_actor_bounds(False)
    if max(e.x, e.y) > rayon_xy:     # trop grand : terrain, plan d'eau geant, falaise lointaine
        return False
    return True


def trouver_rotonde(acteurs):
    if CENTRE_DEMO:
        return unreal.Vector(CENTRE_DEMO[0], CENTRE_DEMO[1], 0), "reglage CENTRE_DEMO"
    lierres = []
    ruines = []
    objets_pack = []
    for a in acteurs:
        m = _mesh_de(a) if a.get_class().get_name() == "StaticMeshActor" else None
        p = chemin(m) or ""
        cl = a.get_class().get_path_name()
        if "vines" in p.lower() or "vines" in cl.lower():
            lierres.append(a.get_actor_location())
        if "/meshes/ruins/" in p:
            ruines.append((a, p))
        if p.startswith(PACK + "/") or cl.startswith(PACK + "/"):
            objets_pack.append(a.get_actor_location())
    meilleur, score_max = None, -1.0
    for a, p in ruines:
        o, e = a.get_actor_bounds(False)
        if max(e.x, e.y) < 250:       # trop petit pour etre la rotonde
            continue
        loc = a.get_actor_location()
        n = sum(1 for l in lierres if (l - loc).length() < max(e.x, e.y) + 800)
        # rotonde = la ruine la plus entouree de lierre et d'autres objets du pack (comme sur la photo)
        voisins = sum(1 for l in objets_pack if (l - loc).length() < 2500)
        score = 2 * n + voisins / 10.0 + min(e.z, 2000) / 1000.0
        log("  candidat rotonde : %s (%s) taille=%.0fx%.0fx%.0f m lierre=%d voisins=%d score=%.1f" % (
            label(a), p.split("/")[-1].split(".")[0], e.x / 50, e.y / 50, e.z / 50, n, voisins, score))
        if score > score_max:
            meilleur, score_max = a, score
    if meilleur:
        return meilleur.get_actor_location(), label(meilleur)
    return None, None


def lire_demo():
    """Ouvre la demo, choisit le coin de la rotonde, note tout, et copie les acteurs (Ctrl+C)."""
    if not ELL.load_level(CARTE_DEMO):
        raise RuntimeError("Impossible d'ouvrir la carte de demo %s" % CARTE_DEMO)
    acteurs = ELL.get_all_level_actors()
    preparer_ignores(acteurs)
    landscapes = [a for a in acteurs if est_landscape(a)]
    z_haut, z_bas = bornes_z(landscapes)
    centre, nom = trouver_rotonde(acteurs)
    if centre is None:
        raise RuntimeError("Rotonde introuvable dans la demo (regle CENTRE_DEMO dans le script)")
    sol_c = hauteur_sol(centre.x, centre.y, z_haut, z_bas, est_landscape)
    centre = unreal.Vector(centre.x, centre.y, sol_c if sol_c is not None else centre.z)
    log("Demo : rotonde = %s a (%.0f, %.0f, %.0f)" % (nom, centre.x, centre.y, centre.z))
    r = RAYON_COPIE_M * 100.0

    def h_sol(loc):
        z = hauteur_sol(loc.x, loc.y, z_haut, z_bas, est_landscape)
        return loc.z - (z if z is not None else centre.z)

    choisis, infos, eau = [], {}, 0
    rayon_reel = 0.0
    for a in acteurs:
        loc = a.get_actor_location()
        d = math.hypot(loc.x - centre.x, loc.y - centre.y)
        if d > r or not _a_copier(a, r):
            continue
        if _est_eau(a):
            eau += 1
            continue
        choisis.append(a)
        rayon_reel = max(rayon_reel, d)
        sm = None
        mats = []
        if a.get_class().get_name() == "StaticMeshActor":
            sm = chemin(_mesh_de(a))
            try:
                mats = [chemin(m) for m in a.static_mesh_component.get_materials()]
            except Exception:
                mats = []
        rot = a.get_actor_rotation()
        sc = a.get_actor_scale3d()
        infos[label(a)] = {"dx": loc.x - centre.x, "dy": loc.y - centre.y, "h": h_sol(loc), "z": loc.z,
                           "mesh": sm, "mats": mats, "rot": [rot.pitch, rot.yaw, rot.roll],
                           "scale": [sc.x, sc.y, sc.z]}
    log("Demo : %d objet(s) a copier dans un rayon de %.0f m (%d plan(s) d'eau ignores)" % (len(choisis), RAYON_COPIE_M, eau))

    # foliage peint (arbres, buissons, fleurs) -> liste de transforms
    plantes = []
    n_total_fol = 0
    ifas = [a for a in acteurs if isinstance(a, unreal.InstancedFoliageActor)]
    for ifa in ifas:
        for c in ifa.get_components_by_class(unreal.InstancedStaticMeshComponent) or []:
            n_total_fol += c.get_instance_count()
    log("Demo : %d acteur(s) de foliage, %d plante(s) au total" % (len(ifas), n_total_fol))
    for ifa in ifas:
        for c in ifa.get_components_by_class(unreal.InstancedStaticMeshComponent) or []:
            m = c.static_mesh
            if m is None:
                continue
            p = chemin(m)
            if not COPIER_HERBE_FOLIAGE and "grass" in p.lower():
                continue
            for i in range(c.get_instance_count()):
                t = c.get_instance_transform(i, True)
                if isinstance(t, (tuple, list)):
                    t = t[-1]
                if t is None:
                    continue
                loc = t.translation
                d = math.hypot(loc.x - centre.x, loc.y - centre.y)
                if d > r:
                    continue
                rot = t.rotation.rotator()
                sc = t.scale3d
                plantes.append({"mesh": p, "dx": loc.x - centre.x, "dy": loc.y - centre.y, "h": h_sol(loc),
                                "rot": [rot.pitch, rot.yaw, rot.roll], "scale": [sc.x, sc.y, sc.z]})
    random.seed(77)
    if len(plantes) > MAX_PLANTES:
        # on garde en priorite les arbres (moins nombreux), puis un tirage regulier du reste
        arbres = [p for p in plantes if "/trees/" in p["mesh"]]
        reste = [p for p in plantes if "/trees/" not in p["mesh"]]
        random.shuffle(reste)
        plantes = arbres[:MAX_PLANTES] + reste[:max(0, MAX_PLANTES - len(arbres))]
    log("Demo : %d plante(s) du foliage a recopier" % len(plantes))

    copie_ok = False
    if choisis:
        ELL.select_nothing()
        ELL.set_selected_level_actors(choisis)
        try:
            SL.execute_console_command(monde(), "EDIT COPY")
            copie_ok = True
        except Exception as e:
            log("Copier/coller indisponible (%s) : on recree les objets un par un." % e)
        ELL.select_nothing()
    return {"centre": centre, "infos": infos, "plantes": plantes, "copie_ok": copie_ok,
            "rayon": max(rayon_reel, 1500.0) + 500.0, "eau": eau}


# ----------------------------------------------------------------------------------------------- zone libre
def chercher_zone(rayon_cm, landscapes, acteurs):
    zh, zb = bornes_z(landscapes)
    if ZONE_CENTRE:
        z = hauteur_sol(ZONE_CENTRE[0], ZONE_CENTRE[1], zh, zb, est_sol_rachid)
        return unreal.Vector(ZONE_CENTRE[0], ZONE_CENTRE[1], z if z is not None else 0.0), "reglage ZONE_CENTRE"

    # point prefere : depart du joueur, sinon milieu de la ville, sinon milieu du Landscape
    pref = None
    for a in acteurs:
        if isinstance(a, unreal.PlayerStart):
            pref = a.get_actor_location()
            break
    if pref is None:
        tuiles = [a for a in acteurs if label(a).startswith("Chelles_Tile_")]
        if tuiles:
            pts = [a.get_actor_location() for a in tuiles]
            pref = unreal.Vector(sum(p.x for p in pts) / len(pts), sum(p.y for p in pts) / len(pts), 0)
    xmin = ymin = 1e12
    xmax = ymax = -1e12
    for l in landscapes:
        o, e = l.get_actor_bounds(False)
        xmin, xmax = min(xmin, o.x - e.x), max(xmax, o.x + e.x)
        ymin, ymax = min(ymin, o.y - e.y), max(ymax, o.y + e.y)
    if pref is None:
        pref = unreal.Vector((xmin + xmax) / 2, (ymin + ymax) / 2, 0)

    case = CASE_M * 100.0
    nx, ny = int((xmax - xmin) / case) + 1, int((ymax - ymin) / case) + 1
    while nx * ny > 60000:            # tres grand terrain : cases plus grandes
        case *= 1.5
        nx, ny = int((xmax - xmin) / case) + 1, int((ymax - ymin) / case) + 1
    log("Recherche d'un terrain libre : %d x %d points (pas %.0f m)..." % (nx, ny, case / 100.0))
    ok = [[False] * ny for _ in range(nx)]
    hz = [[0.0] * ny for _ in range(nx)]
    for i in range(nx):
        for j in range(ny):
            x, y = xmin + i * case, ymin + j * case
            loc, act = premier_impact(x, y, zh, zb)
            if loc is not None and est_sol_rachid(act):
                ok[i][j] = True
                hz[i][j] = loc.z
    rc = int(math.ceil(rayon_cm / case))
    offs = [(di, dj) for di in range(-rc, rc + 1) for dj in range(-rc, rc + 1) if di * di + dj * dj <= rc * rc]
    tol = int(len(offs) * PART_OBSTACLES_MAX)
    cand = []
    for i in range(rc, nx - rc):
        for j in range(rc, ny - rc):
            if ok[i][j]:
                x, y = xmin + i * case, ymin + j * case
                cand.append(((x - pref.x) ** 2 + (y - pref.y) ** 2, i, j))
    cand.sort()
    for d2, i, j in cand:
        mauvais, zs = 0, []
        for di, dj in offs:
            if ok[i + di][j + dj]:
                zs.append(hz[i + di][j + dj])
            else:
                mauvais += 1
                if mauvais > tol:
                    break
        if mauvais > tol or not zs:
            continue
        if max(zs) - min(zs) > PENTE_MAX_M * 100.0:
            continue
        x, y = xmin + i * case, ymin + j * case
        return unreal.Vector(x, y, hz[i][j]), "a %.0f m du point de depart" % (math.sqrt(d2) / 100.0)
    return None, None


# ----------------------------------------------------------------------------------------------- pose
def marquer(a):
    try:
        tags = list(a.get_editor_property("tags") or [])
        if unreal.Name(TAG) not in tags:
            tags.append(unreal.Name(TAG))
        a.set_editor_property("tags", tags)
    except Exception:
        pass
    try:
        a.set_folder_path(DOSSIER_OUTLINER)
    except Exception:
        pass


def _statique(a, ombre=True, distance_max=0.0):
    try:
        c = a.static_mesh_component
        c.set_mobility(unreal.ComponentMobility.STATIC)
        if not ombre:
            c.set_cast_shadow(False)
        if distance_max > 0:
            c.set_editor_property("ld_max_draw_distance", float(distance_max))
    except Exception:
        pass


def poser(zone, demo, zh, zb):
    centre_demo = demo["centre"]
    infos = demo["infos"]

    def z_sol(x, y):
        z = hauteur_sol(x, y, zh, zb, est_sol_rachid)
        return z if z is not None else zone.z

    poses = []
    if demo["copie_ok"] and infos:
        ELL.select_nothing()
        SL.execute_console_command(monde(), "EDIT PASTE")
        colles = list(ELL.get_selected_level_actors() or [])
        ELL.select_nothing()
        log("Colles depuis la demo : %d objet(s)" % len(colles))
        vus = set()
        for a in colles:
            inf = infos.get(label(a))
            if inf is not None:
                vus.add(label(a))
                dx, dy, h = inf["dx"], inf["dy"], inf["h"]
            else:
                loc = a.get_actor_location()
                dx, dy, h = loc.x - centre_demo.x, loc.y - centre_demo.y, loc.z - centre_demo.z
            x, y = zone.x + dx, zone.y + dy
            a.set_actor_location(unreal.Vector(x, y, z_sol(x, y) + h), False, False)
            marquer(a)
            _statique(a)
            poses.append(a)
        if not colles:
            manquants = list(infos.keys())
        elif len(colles) >= len(infos):
            manquants = []          # tout est arrive (meme si des noms ont change)
        else:
            manquants = [k for k in infos if k not in vus]
    else:
        manquants = list(infos.keys())

    # objets non colles (ou copier/coller indisponible) : recrees un par un
    n_recrees = 0
    for k in manquants:
        inf = infos[k]
        if not inf.get("mesh"):
            continue
        m = EAL.load_asset(inf["mesh"])
        if m is None:
            continue
        x, y = zone.x + inf["dx"], zone.y + inf["dy"]
        rot = unreal.Rotator()
        rot.pitch, rot.yaw, rot.roll = inf["rot"][0], inf["rot"][1], inf["rot"][2]
        a = ELL.spawn_actor_from_object(m, unreal.Vector(x, y, z_sol(x, y) + inf["h"]), rot)
        if a is None:
            continue
        a.set_actor_scale3d(unreal.Vector(*inf["scale"]))
        for i, mp in enumerate(inf.get("mats") or []):
            mat = EAL.load_asset(mp) if mp else None
            if mat:
                a.static_mesh_component.set_material(i, mat)
        a.set_actor_label(k)
        marquer(a)
        _statique(a)
        poses.append(a)
        n_recrees += 1
    if n_recrees:
        log("Recrees un par un : %d objet(s)" % n_recrees)

    # plantes du foliage -> objets simples
    cache = {}
    n_pl = 0
    for p in demo["plantes"]:
        m = cache.get(p["mesh"])
        if m is None:
            m = EAL.load_asset(p["mesh"])
            cache[p["mesh"]] = m
        if m is None:
            continue
        x, y = zone.x + p["dx"], zone.y + p["dy"]
        rot = unreal.Rotator()
        rot.pitch, rot.yaw, rot.roll = p["rot"][0], p["rot"][1], p["rot"][2]
        a = ELL.spawn_actor_from_object(m, unreal.Vector(x, y, z_sol(x, y) + p["h"]), rot)
        if a is None:
            continue
        a.set_actor_scale3d(unreal.Vector(*p["scale"]))
        b = m.get_bounding_box()
        petit = (b.max.z - b.min.z) * p["scale"][2] < 300
        _statique(a, ombre=not petit, distance_max=8000.0 if petit else 0.0)
        a.set_actor_label("Stylized_" + p["mesh"].split("/")[-1].split(".")[0])
        marquer(a)
        n_pl += 1
    log("Plantes posees : %d" % n_pl)
    return poses


def poser_lac(zone, rayon_cm, poses, zh, zb):
    eau = EAL.load_asset(MI_EAU)
    disque = EAL.load_asset("/Engine/BasicShapes/Cylinder")
    if eau is None or disque is None:
        log("Lac : materiau d'eau ou forme introuvable, pas de lac.")
        return
    # direction la plus degagee, a environ 60 % du rayon
    dist = rayon_cm * 0.6
    meilleur, n_min = 0.0, 10 ** 9
    for k in range(8):
        ang = k * math.pi / 4
        px, py = zone.x + dist * math.cos(ang), zone.y + dist * math.sin(ang)
        n = 0
        for a in poses:
            l = a.get_actor_location()
            if math.hypot(l.x - px, l.y - py) < 900:
                n += 1
        if n < n_min:
            meilleur, n_min = ang, n
    cx, cy = zone.x + dist * math.cos(meilleur), zone.y + dist * math.sin(meilleur)
    z = hauteur_sol(cx, cy, zh, zb, est_sol_rachid)
    z = zone.z if z is None else z
    diam_m = 14.0
    a = ELL.spawn_actor_from_object(disque, unreal.Vector(cx, cy, z + 4.0), unreal.Rotator())
    a.set_actor_scale3d(unreal.Vector(diam_m, diam_m, 0.04))   # cylindre de 1 m -> disque de 14 m
    a.static_mesh_component.set_material(0, eau)
    a.static_mesh_component.set_collision_profile_name("NoCollision")
    a.set_actor_label("Stylized_Lac")
    marquer(a)
    _statique(a, ombre=False)
    # pierres autour
    pierres = [EAL.load_asset(PACK + "/meshes/cliff/SM_cliff_stone_0%d" % i) for i in range(1, 5)]
    pierres = [p for p in pierres if p]
    random.seed(7)
    n = 14
    for k in range(n):
        if not pierres:
            break
        ang = 2 * math.pi * k / n + random.uniform(-0.1, 0.1)
        r = diam_m * 50.0 + random.uniform(20, 90)
        px, py = cx + r * math.cos(ang), cy + r * math.sin(ang)
        m = pierres[k % len(pierres)]
        b = m.get_bounding_box()
        taille = max(b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z, 1.0)
        s = random.uniform(120, 220) / taille
        pz = hauteur_sol(px, py, zh, zb, est_sol_rachid)
        rot = unreal.Rotator()
        rot.yaw = random.uniform(0, 360)
        p = ELL.spawn_actor_from_object(m, unreal.Vector(px, py, (z if pz is None else pz) - 30.0), rot)
        if p:
            p.set_actor_scale3d(unreal.Vector(s, s, s))
            p.set_actor_label("Stylized_PierreLac")
            marquer(p)
            _statique(p)
    log("Lac pose a (%.0f, %.0f), diametre %.0f m, avec %d pierres." % (cx, cy, diam_m, n))


# ----------------------------------------------------------------------------------------------- sol
def poser_sol(journal, mi, landscapes, dalles):
    for l in landscapes:
        k = chemin(l)
        journal["landscapes"].setdefault(k, chemin(l.get_editor_property("landscape_material")))
        l.set_editor_property("landscape_material", mi)
        log("  Landscape %s : materiau -> MI_SolStylized" % label(l))
    for a in dalles:
        comp = a.static_mesh_component
        k = chemin(a)
        if k not in journal["dalles"]:
            journal["dalles"][k] = [chemin(m) for m in (comp.get_editor_property("override_materials") or [])]
        n = max(1, comp.get_num_materials())
        for i in range(n):
            comp.set_material(i, mi)
        log("  %s : materiau -> MI_SolStylized" % label(a))


def sauvegarder_umap():
    src = os.path.join(PROJET, "Content", *CARTE.replace("/Game/", "").split("/")) + ".umap"
    dst_dir = os.path.join(DOSSIER_OUTIL, "sauvegarde")
    dst = os.path.join(dst_dir, os.path.basename(src))
    if os.path.exists(src) and not os.path.exists(dst):
        os.makedirs(dst_dir, exist_ok=True)
        shutil.copy2(src, dst)
        log("Copie de securite : %s" % dst)


# ----------------------------------------------------------------------------------------------- main
def decor_present(acteurs):
    res = []
    for a in acteurs:
        try:
            if unreal.Name(TAG) in list(a.get_editor_property("tags") or []):
                res.append(a)
        except Exception:
            pass
    return res


def main():
    if not EAL.does_asset_exist(MF_AUTO) or not EAL.does_asset_exist(CARTE_DEMO):
        message("Le pack Stylized Landscape n'est pas trouve dans Content/Stylized_Landscape.")
        return
    q = ("Ce script va :\n"
         "- enregistrer la carte ouverte, puis copier Rachid.umap dans Tools/StylizedLandscape/sauvegarde\n"
         + ("- mettre le sol du pack Stylized Landscape sur le Landscape de Rachid\n" if FAIRE_SOL else "")
         + ("- ouvrir la demo du pack, copier la rotonde et son decor (escaliers, ruines, lierre, arbres, fleurs)\n"
            "- les poser dans Rachid sur un terrain libre et plat (pas sur les routes ni les batiments)\n"
            if FAIRE_DECOR else "")
         + "\nRien n'est efface. annuler_stylized_landscape.py remet tout comme avant.\n"
         "Cela peut prendre quelques minutes. Continuer ?")
    if not message(q, True):
        log("Annule par l'utilisateur : rien n'a ete modifie.")
        return
    if FAIRE_DECOR and nom_carte() == CARTE.split("/")[-1] and decor_present(ELL.get_all_level_actors()):
        message("Le decor Stylized est deja dans Rachid.\n"
                "Lance d'abord annuler_stylized_landscape.py si tu veux le refaire.")
        return

    ELL.save_all_dirty_levels()
    sauvegarder_umap()

    mi = construire_materiau() if FAIRE_SOL else None   # d'abord : si ca rate, rien n'est touche

    demo = None
    if FAIRE_DECOR:
        demo = lire_demo()

    if nom_carte() != CARTE.split("/")[-1]:
        if not ELL.load_level(CARTE):
            raise RuntimeError("Impossible de rouvrir %s" % CARTE)
    acteurs = ELL.get_all_level_actors()
    preparer_ignores(acteurs)
    deja = decor_present(acteurs)
    if deja and FAIRE_DECOR:
        message("Le decor Stylized est deja dans Rachid (%d objets).\n"
                "Lance d'abord annuler_stylized_landscape.py si tu veux le refaire." % len(deja))
        FAIRE = False
    else:
        FAIRE = FAIRE_DECOR
    landscapes = [a for a in acteurs if est_landscape(a)]
    dalles = [a for a in acteurs if label(a).startswith("Chelles_Tile_Ground") and isinstance(a, unreal.StaticMeshActor)]
    zh, zb = bornes_z(landscapes)
    log("Rachid : %d Landscape, %d dalle(s) de sol" % (len(landscapes), len(dalles)))

    journal = charger_journal()
    journal["date"] = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    zone = None
    try:
        if FAIRE_SOL:
            poser_sol(journal, mi, landscapes, dalles if FAIRE_DALLES_SOL else [])
        if FAIRE:
            rayon = demo["rayon"]
            zone, ou = chercher_zone(rayon + 500.0, landscapes, acteurs)
            if zone is None:
                log("Aucun terrain libre et plat assez grand trouve.")
                message("Le sol est change, mais je n'ai pas trouve de terrain libre assez grand pour le decor "
                        "(%.0f m de rayon).\nIndique un endroit dans ZONE_CENTRE en haut du script et relance."
                        % (rayon / 100.0))
            else:
                log("Zone du decor : (%.0f, %.0f, %.0f) %s" % (zone.x, zone.y, zone.z, ou))
                journal["zone"] = [zone.x, zone.y, zone.z, rayon]
                poses = poser(zone, demo, zh, zb)
                if FAIRE_LAC:
                    poser_lac(zone, rayon, poses, zh, zb)
                if mi is not None:
                    regler_zone_herbe(mi, zone, rayon + 300.0)
    finally:
        ecrire_journal(journal)
    ELL.save_current_level()
    log("Termine et enregistre.")
    if zone is not None:
        try:
            ELL.set_level_viewport_camera_info(unreal.Vector(zone.x - 4500, zone.y - 4500, zone.z + 2200),
                                               unreal.Rotator(0, -20, 45))
        except Exception:
            pass
        message("C'est fait !\n\nLe decor est a X=%.0f  Y=%.0f (la camera est dessus).\n"
                "Dans l'Outliner : dossier Chelles > DecorStylized.\n\n"
                "Lance Play pour y aller. Si l'eclairage parait bizarre : Build > Build Lighting Only.\n"
                "Pour revenir en arriere : annuler_stylized_landscape.py" % (zone.x, zone.y))
    else:
        message("Sol change et enregistre.\nPour revenir en arriere : annuler_stylized_landscape.py")


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())
    unreal.log_error(traceback.format_exc())
    try:
        message("Erreur : voir Tools/out/stylized_landscape.log")
    except Exception:
        pass
finally:
    ecrire_log()
