# -*- coding: utf-8 -*-
"""
Habiller la tour Gambetta (CHELLES 3D, Unreal 4.27).

A lancer DANS l'editeur : selectionner le cube de la tour dans la vue,
puis File > Execute Python Script... > ce fichier.

Ce que fait le script :
  1. Regle les 4 textures de /Game/IMAGES/TourG pour garder leur canal alpha
     (compression Default, sRGB, "Compression No Alpha" decoche).
  2. Cree le materiau M_TourG_Facade : la facade est melangee au blanc du
     cube grace a l'alpha (plus de fond noir). Couleur du fond reglable.
  3. Cree 4 instances MI_TourG_Avant / Arriere / Droite / Gauche.
  4. Pose 4 plans colles sur les 4 faces du cube selectionne, attaches a lui
     (si on deplace ou etire le cube, les facades suivent).

Relancer le script remplace les facades deja posees (rien n'est duplique).
Ctrl+Z annule la pose des facades.
"""
import unicodedata
import unreal

DOSSIER_TEXTURES = "/Game/IMAGES/TourG"
DOSSIER_MATERIAUX = "/Game/IMAGES/TourG"
NOM_MATERIAU = "M_TourG_Facade"
PREFIXE_ACTEUR = "TourG_Facade_"
DECALAGE_CM = 2.0          # ecart entre la facade et le mur (evite le scintillement)

# Face du cube (axes locaux du cube) pour chaque facade.
# Avant = +X ; quand on regarde l'avant, la droite est -Y et la gauche +Y.
FACES = {
    "Avant":   unreal.Vector(1, 0, 0),
    "Arriere": unreal.Vector(-1, 0, 0),
    "Droite":  unreal.Vector(0, -1, 0),
    "Gauche":  unreal.Vector(0, 1, 0),
}
MOTS_CLES = {"Avant": "avant", "Arriere": "arriere", "Droite": "droite", "Gauche": "gauche"}

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def sans_accents(texte):
    t = unicodedata.normalize("NFD", texte)
    return "".join(c for c in t if unicodedata.category(c) != "Mn").lower()


def log(msg):
    unreal.log("[TourG] " + msg)


# ---------------------------------------------------------------- textures
def trouver_textures():
    trouvees = {}
    for chemin in eal.list_assets(DOSSIER_TEXTURES, recursive=False, include_folder=False):
        asset = eal.load_asset(chemin)
        if not isinstance(asset, unreal.Texture2D):
            continue
        nom = sans_accents(asset.get_name())
        for cle, mot in MOTS_CLES.items():
            if mot in nom and cle not in trouvees:
                trouvees[cle] = asset
    return trouvees


def regler_texture(tex):
    changes = []
    if tex.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_DEFAULT:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        changes.append("compression Default")
    if not tex.get_editor_property("srgb"):
        tex.set_editor_property("srgb", True)
        changes.append("sRGB")
    if tex.get_editor_property("compression_no_alpha"):
        tex.set_editor_property("compression_no_alpha", False)
        changes.append("alpha garde")
    alpha = "?"
    try:
        alpha = "oui" if tex.has_alpha_channel() else "NON"
    except Exception:
        pass
    log("%s : %s, canal alpha = %s" % (tex.get_name(), ", ".join(changes) or "deja bon", alpha))
    return alpha


# ---------------------------------------------------------------- materiau
def creer_materiau(texture_defaut):
    chemin = DOSSIER_MATERIAUX + "/" + NOM_MATERIAU
    if eal.does_asset_exist(chemin):
        log("materiau deja present : " + chemin)
        return eal.load_asset(chemin)

    outils = unreal.AssetToolsHelpers.get_asset_tools()
    mat = outils.create_asset(NOM_MATERIAU, DOSSIER_MATERIAUX, unreal.Material, unreal.MaterialFactoryNew())

    def noeud(classe, x, y):
        return mel.create_material_expression(mat, classe, x, y)

    # Coordonnees UV avec miroir horizontal / vertical (pour retourner une image si besoin)
    uv = noeud(unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    inv = noeud(unreal.MaterialExpressionOneMinus, -1200, 80)
    mel.connect_material_expressions(uv, "", inv, "")

    miroir_h = noeud(unreal.MaterialExpressionScalarParameter, -1400, 200)
    miroir_h.set_editor_property("parameter_name", "Miroir_Horizontal")
    miroir_v = noeud(unreal.MaterialExpressionScalarParameter, -1400, 300)
    miroir_v.set_editor_property("parameter_name", "Miroir_Vertical")
    masque = noeud(unreal.MaterialExpressionAppendVector, -1200, 240)
    mel.connect_material_expressions(miroir_h, "", masque, "A")
    mel.connect_material_expressions(miroir_v, "", masque, "B")

    uv_miroir = noeud(unreal.MaterialExpressionLinearInterpolate, -1000, 60)
    mel.connect_material_expressions(uv, "", uv_miroir, "A")
    mel.connect_material_expressions(inv, "", uv_miroir, "B")
    mel.connect_material_expressions(masque, "", uv_miroir, "Alpha")

    # Texture de facade
    tex = noeud(unreal.MaterialExpressionTextureSampleParameter2D, -450, 0)
    tex.set_editor_property("parameter_name", "Facade")
    tex.set_editor_property("texture", texture_defaut)
    mel.connect_material_expressions(uv_miroir, "", tex, "UVs")

    # Fond = couleur du cube (blanc), remplace la ou la facade est transparente
    fond = noeud(unreal.MaterialExpressionVectorParameter, -450, -250)
    fond.set_editor_property("parameter_name", "Couleur_du_fond")
    fond.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))

    melange = noeud(unreal.MaterialExpressionLinearInterpolate, -150, 0)
    mel.connect_material_expressions(fond, "", melange, "A")
    mel.connect_material_expressions(tex, "RGB", melange, "B")
    mel.connect_material_expressions(tex, "A", melange, "Alpha")
    mel.connect_material_property(melange, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rugosite = noeud(unreal.MaterialExpressionScalarParameter, -150, 200)
    rugosite.set_editor_property("parameter_name", "Rugosite")
    rugosite.set_editor_property("default_value", 0.8)
    mel.connect_material_property(rugosite, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("materiau cree : " + chemin)
    return mat


def creer_instance(mat, cle, texture):
    nom = "MI_TourG_" + cle
    chemin = DOSSIER_MATERIAUX + "/" + nom
    if eal.does_asset_exist(chemin):
        mi = eal.load_asset(chemin)
    else:
        outils = unreal.AssetToolsHelpers.get_asset_tools()
        mi = outils.create_asset(nom, DOSSIER_MATERIAUX, unreal.MaterialInstanceConstant,
                                 unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, mat)
    mel.set_material_instance_texture_parameter_value(mi, "Facade", texture)
    return mi


# ---------------------------------------------------------------- pose
def tourner(rotateur, v):
    try:
        return rotateur.rotate_vector(v)
    except Exception:
        return unreal.MathLibrary.greater_greater_vector_rotator(v, rotateur)


def axe_dominant(v):
    a = [abs(v.x), abs(v.y), abs(v.z)]
    return a.index(max(a))


def sens_uv_du_plan(plan):
    """Cherche dans quel sens vont U et V sur le plan moteur.
    Retourne (dirU, dirV) en axes locaux du plan ; valeur par defaut si echec."""
    defaut = (unreal.Vector(1, 0, 0), unreal.Vector(0, 1, 0))
    try:
        md = plan.get_static_mesh_description(0)
        pts = []
        for i in range(md.get_vertex_instance_count()):
            vi = unreal.VertexInstanceID()
            vi.set_editor_property("id_value", i)
            if not md.is_vertex_instance_valid(vi):
                continue
            p = md.get_vertex_position(md.get_vertex_instance_vertex(vi))
            uv = md.get_vertex_instance_uv(vi, 0)
            pts.append((p.x, p.y, uv.x, uv.y))
        if len(pts) < 3:
            return defaut
        n = float(len(pts))
        mx = sum(q[0] for q in pts) / n
        my = sum(q[1] for q in pts) / n
        mu = sum(q[2] for q in pts) / n
        mv = sum(q[3] for q in pts) / n
        sxx = sum((q[0] - mx) ** 2 for q in pts)
        syy = sum((q[1] - my) ** 2 for q in pts)
        sxy = sum((q[0] - mx) * (q[1] - my) for q in pts)
        det = sxx * syy - sxy * sxy
        if abs(det) < 1e-9:
            return defaut

        def gradient(k, m):
            sxk = sum((q[0] - mx) * (q[k] - m) for q in pts)
            syk = sum((q[1] - my) * (q[k] - m) for q in pts)
            return ((syy * sxk - sxy * syk) / det, (sxx * syk - sxy * sxk) / det)

        def axe(g):
            if abs(g[0]) >= abs(g[1]):
                return unreal.Vector(1 if g[0] > 0 else -1, 0, 0)
            return unreal.Vector(0, 1 if g[1] > 0 else -1, 0)

        du = axe(gradient(2, mu))
        dv = axe(gradient(3, mv))
        log("UV du plan : U vers %s, V vers %s" % (du, dv))
        return du, dv
    except Exception as e:
        log("sens des UV du plan non lu (%s), valeur par defaut" % e)
        return defaut


def poser_facades(cube, instances):
    comp = cube.static_mesh_component
    mesh = comp.static_mesh
    bornes = mesh.get_bounds()
    centre, ext = bornes.origin, bornes.box_extent
    ext_l = [ext.x, ext.y, ext.z]
    echelle = cube.get_actor_scale3d()
    ech_l = [abs(echelle.x) or 1, abs(echelle.y) or 1, abs(echelle.z) or 1]

    plan = unreal.load_asset("/Engine/BasicShapes/Plane.Plane")
    pb = plan.get_bounds().box_extent
    demi_plan = [pb.x or 50.0, pb.y or 50.0]
    dir_u, dir_v = sens_uv_du_plan(plan)
    bas = unreal.Vector(0, 0, -1)

    # Retire les anciennes facades posees par ce script
    for enfant in cube.get_attached_actors():
        if enfant.get_actor_label().startswith(PREFIXE_ACTEUR):
            unreal.EditorLevelLibrary.destroy_actor(enfant)

    for cle, n in FACES.items():
        if cle not in instances:
            continue
        mi = instances[cle]
        droite = unreal.Vector(n.y, -n.x, 0)   # droite de quelqu'un qui regarde la facade

        # Rotation : normale du plan (Z) vers l'exterieur, U du plan vers la droite
        if abs(dir_u.x) > 0.5:
            rot = unreal.MathLibrary.make_rot_from_zx(n, unreal.Vector(droite.x * dir_u.x, droite.y * dir_u.x, 0))
        else:
            rot = unreal.MathLibrary.make_rot_from_zy(n, unreal.Vector(droite.x * dir_u.y, droite.y * dir_u.y, 0))
        v_monde = tourner(rot, dir_v)
        miroir_v = 1.0 if (v_monde.x * bas.x + v_monde.y * bas.y + v_monde.z * bas.z) < 0 else 0.0
        mel.set_material_instance_scalar_parameter_value(mi, "Miroir_Vertical", miroir_v)
        mel.set_material_instance_scalar_parameter_value(mi, "Miroir_Horizontal", 0.0)
        eal.save_loaded_asset(mi)

        # Taille : le plan couvre toute la face
        ax_x = axe_dominant(tourner(rot, unreal.Vector(1, 0, 0)))
        ax_y = axe_dominant(tourner(rot, unreal.Vector(0, 1, 0)))
        taille = unreal.Vector(ext_l[ax_x] / demi_plan[0], ext_l[ax_y] / demi_plan[1], 1)

        ax_n = axe_dominant(n)
        recul = ext_l[ax_n] + DECALAGE_CM / ech_l[ax_n]
        pos = unreal.Vector(centre.x + n.x * recul, centre.y + n.y * recul, centre.z + n.z * recul)

        acteur = unreal.EditorLevelLibrary.spawn_actor_from_object(plan, cube.get_actor_location(), cube.get_actor_rotation())
        acteur.set_actor_label(PREFIXE_ACTEUR + cle)
        acteur.attach_to_actor(cube, "", unreal.AttachmentRule.KEEP_RELATIVE,
                               unreal.AttachmentRule.KEEP_RELATIVE, unreal.AttachmentRule.KEEP_RELATIVE, False)
        acteur.set_actor_relative_location(pos, False, True)
        acteur.set_actor_relative_rotation(rot, False, True)
        acteur.set_actor_relative_scale3d(taille)
        smc = acteur.static_mesh_component
        try:
            smc.set_mobility(comp.get_editor_property("mobility"))
        except Exception:
            pass
        smc.set_material(0, mi)
        smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        smc.set_editor_property("cast_shadow", False)
        try:
            acteur.set_folder_path("TourGambetta")
        except Exception:
            pass
        log("facade %s posee" % cle)


# ---------------------------------------------------------------- principal
def main():
    textures = trouver_textures()
    manquantes = [c for c in FACES if c not in textures]
    if not textures:
        unreal.EditorDialog.show_message("Tour Gambetta", "Aucune texture trouvee dans %s." % DOSSIER_TEXTURES,
                                         unreal.AppMsgType.OK)
        return

    sans_alpha = []
    for cle, tex in textures.items():
        if regler_texture(tex) == "NON":
            sans_alpha.append(tex.get_name())
        eal.save_loaded_asset(tex)

    mat = creer_materiau(list(textures.values())[0])
    instances = {cle: creer_instance(mat, cle, tex) for cle, tex in textures.items()}

    selection = [a for a in unreal.EditorLevelLibrary.get_selected_level_actors()
                 if isinstance(a, unreal.StaticMeshActor) and not a.get_actor_label().startswith(PREFIXE_ACTEUR)]
    msg = []
    if len(selection) == 1:
        with unreal.ScopedEditorTransaction("Facades tour Gambetta"):
            poser_facades(selection[0], instances)
        msg.append("Les facades sont posees sur « %s »." % selection[0].get_actor_label())
    else:
        msg.append("Materiaux crees, mais aucune facade posee : selectionne UNIQUEMENT le cube de la tour, "
                   "puis relance le script.")
    if manquantes:
        msg.append("Textures non trouvees : " + ", ".join(manquantes))
    if sans_alpha:
        msg.append("Attention, ces images n'ont PAS de transparence : " + ", ".join(sans_alpha)
                   + ". Il faut les reexporter en PNG avec fond transparent.")
    unreal.EditorDialog.show_message("Tour Gambetta", "\n\n".join(msg), unreal.AppMsgType.OK)


main()
