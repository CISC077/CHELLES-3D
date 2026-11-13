# -*- coding: utf-8 -*-
# CHELLES 3D - Spider-Male SANS BLANC (2026-11-13, demande MERIDJA)
# Dans Unreal : File > Execute Python Script... > choisir ce fichier.
#
# Le costume de Spider-Male ne garde que le ROUGE et le BLEU :
#   tout ce qui est blanc / gris clair (araignee sur la poitrine, liseres...) devient rouge,
#   SAUF les yeux (lunettes) qui restent blancs.
#
# Comment : un materiau M_SpiderMale_SansBlanc lit la texture d'origine et repeint les zones blanches.
# Les yeux sont reperes par leur place sur le corps (devant la tete), mesuree sur le squelette.
#
# Ne touche QUE les corps de /Game/Personnages/SK_SpiderMan_UE4/ (SK_SpiderMan_UE4, _Vol, _Pouvoirs,
# _Cavalier, _Combat). La texture d'origine n'est PAS modifiee.
# Spider-Femelle, PANTERA et les autres persos ne changent pas.
#
# Pour revenir en arriere : Tools\SpiderMaleSansBlanc\retirer_spider_male_sans_blanc.py
# On peut relancer ce script sans risque.
#
# Retouche : ouvrir une instance MI_SpiderMale_SansBlanc_<corps> (meme dossier) et changer :
#   Couleur      = couleur qui remplace le blanc (rouge par defaut, mettre du bleu si besoin)
#   SeuilClair   = plus bas -> plus de zones claires repeintes
#   SeuilGris    = plus haut -> repeint aussi les zones blanc-bleute / blanc-rose
#   YeuxBas      = hauteur sous laquelle on ne protege plus le blanc (yeux)
#   GarderYeux   = 1 garde les yeux blancs, 0 repeint tout
import json
import os
import unreal

DOSSIER = "/Game/Personnages/SK_SpiderMan_UE4"
NOM_MAT = "M_SpiderMale_SansBlanc"
PREFIXE_MI = "MI_SpiderMale_SansBlanc_"

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
TOOLS = os.path.dirname(os.path.abspath(__file__))
SAUVEGARDE = os.path.join(TOOLS, "sauvegarde_materiaux.json")


def log(m):
    unreal.log_warning("[SpiderMaleSansBlanc] " + m)


# ---------------------------------------------------------------------------
# 0. Les corps de Spider-Male et leurs materiaux d'origine (sauvegardes une seule fois)
# ---------------------------------------------------------------------------
sauvegarde = {}
if os.path.exists(SAUVEGARDE):
    with open(SAUVEGARDE, "r") as f:
        sauvegarde = json.load(f)

corps = []
for chemin in EAL.list_assets(DOSSIER, recursive=False, include_folder=False):
    chemin = chemin.split(".")[0]
    a = unreal.load_asset(chemin)
    if isinstance(a, unreal.SkeletalMesh):
        corps.append((chemin, a))
if not corps:
    raise RuntimeError("Aucun corps Spider-Male trouve dans " + DOSSIER)

for chemin, mesh in corps:
    if chemin not in sauvegarde:
        liste = []
        for m in mesh.get_editor_property("materials"):
            mi = m.get_editor_property("material_interface")
            liste.append(mi.get_path_name() if mi else "")
        sauvegarde[chemin] = liste
with open(SAUVEGARDE, "w") as f:
    json.dump(sauvegarde, f, indent=2)
log("Materiaux d'origine sauvegardes dans " + SAUVEGARDE)


def textures_de(mi, vus=None):
    """Textures utilisees par un materiau (ou une instance), la plus grande d'abord."""
    vus = vus or set()
    if not mi or mi.get_path_name() in vus:
        return []
    vus.add(mi.get_path_name())
    tex = []
    if isinstance(mi, unreal.MaterialInstance):
        for v in mi.get_editor_property("texture_parameter_values") or []:
            t = v.get_editor_property("parameter_value")
            if t:
                tex.append(t)
        tex += textures_de(mi.get_editor_property("parent"), vus)
    elif isinstance(mi, unreal.Material):
        try:
            tex += list(MEL.get_used_textures(mi))
        except Exception:
            pass
    tex = [t for t in tex if isinstance(t, unreal.Texture2D)]
    tex.sort(key=lambda t: -(t.blueprint_get_size_x() * t.blueprint_get_size_y()))
    return tex


def texture_secours():
    for chemin in EAL.list_assets(DOSSIER, recursive=False, include_folder=False):
        a = unreal.load_asset(chemin.split(".")[0])
        if isinstance(a, unreal.Texture2D):
            return a
    return None


# ---------------------------------------------------------------------------
# 1. Le materiau parent (repeint le blanc, garde les yeux)
# ---------------------------------------------------------------------------
CODE = r"""
float3 c = C;
float mx = max(c.r, max(c.g, c.b));
float mn = min(c.r, min(c.g, c.b));
float sat = (mx - mn) / max(mx, 0.0001);

// Combien ce point est "blanc" (clair et peu colore) : 0 = pas blanc, 1 = blanc
float w = smoothstep(SeuilClair - 0.06, SeuilClair + 0.06, mx)
        * (1.0 - smoothstep(SeuilGris - 0.06, SeuilGris + 0.06, sat));

// Les yeux (devant la tete, en haut) restent blancs
float fwd = ((FwdIsY > 0.5) ? P.y : P.x) * FwdSign;
if (GarderYeux > 0.5 && P.z > YeuxBas && fwd > YeuxAvant)
{
    w = 0.0;
}

// On garde les plis / ombres du tissu
float3 rempl = Couleur.rgb * lerp(0.55, 1.0, saturate(mx));
return lerp(c, rempl, w);
"""

chemin_mat = DOSSIER + "/" + NOM_MAT
if EAL.does_asset_exist(chemin_mat):
    mat = unreal.load_asset(chemin_mat)
    MEL.delete_all_material_expressions(mat)
    log(NOM_MAT + " existe deja : refait.")
else:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NOM_MAT, DOSSIER, unreal.Material, unreal.MaterialFactoryNew())
    log(NOM_MAT + " cree.")
mat.set_editor_property("used_with_skeletal_mesh", True)

tex_defaut = None
for chemin, mesh in corps:
    for orig in sauvegarde[chemin]:
        t = textures_de(unreal.load_asset(orig) if orig else None)
        if t:
            tex_defaut = t[0]
            break
    if tex_defaut:
        break
tex_defaut = tex_defaut or texture_secours()
if not tex_defaut:
    raise RuntimeError("Texture du costume introuvable. Envoie le Output Log a Claude.")
log("Texture du costume : " + tex_defaut.get_path_name())

echant = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, -500)
echant.set_editor_property("parameter_name", "Texture")
echant.set_editor_property("texture", tex_defaut)

CLASSE_POS = getattr(unreal, "MaterialExpressionPreSkinnedPosition", None) or getattr(unreal, "MaterialExpressionPreSkinnedLocalPosition", None)
if CLASSE_POS is None:
    raise RuntimeError("Noeud 'Pre-Skinned Local Position' absent de cet Unreal : envoie le Output Log a Claude.")
pos = MEL.create_material_expression(mat, CLASSE_POS, -1200, -250)
if hasattr(unreal, "MaterialExpressionVertexInterpolator"):
    passe = MEL.create_material_expression(mat, unreal.MaterialExpressionVertexInterpolator, -900, -250)
    if not MEL.connect_material_expressions(pos, "", passe, "VS"):
        MEL.connect_material_expressions(pos, "", passe, "")
    pos = passe

PARAMS = {"SeuilClair": 0.30, "SeuilGris": 0.32, "GarderYeux": 1.0,
          "FwdIsY": 1.0, "FwdSign": 1.0, "YeuxBas": 157.0, "YeuxAvant": 3.0}

perso = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, -400, 0)
perso.set_editor_property("code", CODE)
perso.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
perso.set_editor_property("description", "Spider-Male sans blanc")
entrees = []
for n in ["C", "P"] + sorted(PARAMS.keys()) + ["Couleur"]:
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", n)
    entrees.append(ci)
perso.set_editor_property("inputs", entrees)

MEL.connect_material_expressions(echant, "RGB", perso, "C")
MEL.connect_material_expressions(pos, "", perso, "P")
y = -100
for k in sorted(PARAMS.keys()):
    s = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, y)
    s.set_editor_property("parameter_name", k)
    s.set_editor_property("default_value", float(PARAMS[k]))
    MEL.connect_material_expressions(s, "", perso, k)
    y += 70
coul = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, y)
coul.set_editor_property("parameter_name", "Couleur")
coul.set_editor_property("default_value", unreal.LinearColor(0.42, 0.012, 0.018, 1.0))  # rouge du costume
MEL.connect_material_expressions(coul, "", perso, "Couleur")

MEL.connect_material_property(perso, "", unreal.MaterialProperty.MP_BASE_COLOR)
rug = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 250)
rug.set_editor_property("r", 0.5)
MEL.connect_material_property(rug, "", unreal.MaterialProperty.MP_ROUGHNESS)
MEL.recompile_material(mat)
EAL.save_loaded_asset(mat, False)


# ---------------------------------------------------------------------------
# 2. Une instance par corps, avec la place des yeux mesuree sur son squelette
# ---------------------------------------------------------------------------
def mesurer(mesh):
    os_lus = {}
    acteur = None
    try:
        acteur = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, -100000), unreal.Rotator(0, 0, 0))
        comp = acteur.get_editor_property("skeletal_mesh_component")
        comp.set_skeletal_mesh(mesh)
        base = acteur.get_actor_location()
        for nom in ("head", "neck_01", "foot_l", "ball_l"):
            if comp.does_socket_exist(nom):
                p = comp.get_socket_location(nom)
                os_lus[nom] = (p.x - base.x, p.y - base.y, p.z - base.z)
    except Exception as e:
        log("Mesure des os impossible (" + str(e) + ")")
    finally:
        if acteur:
            acteur.destroy_actor()
    if len(os_lus) < 4 or os_lus["head"][2] < 50:
        log("Os non lus pour " + mesh.get_name() + " : valeurs moyennes (a regler dans l'instance si les yeux deviennent rouges).")
        return {"FwdIsY": 1.0, "FwdSign": 1.0, "YeuxBas": 157.0, "YeuxAvant": 3.0}
    pied, bout = os_lus["foot_l"], os_lus["ball_l"]
    dx, dy = bout[0] - pied[0], bout[1] - pied[1]
    avant_y = abs(dy) >= abs(dx)
    sens = 1.0 if (dy if avant_y else dx) >= 0 else -1.0
    tete = os_lus["head"]
    tete_av = (tete[1] if avant_y else tete[0]) * sens
    return {"FwdIsY": 1.0 if avant_y else 0.0, "FwdSign": sens,
            "YeuxBas": tete[2] - 1.0,        # les yeux sont au-dessus de l'os de la tete
            "YeuxAvant": tete_av + 3.0}      # et devant lui (le visage)


usine = unreal.MaterialInstanceConstantFactoryNew()
problemes = []
for chemin, mesh in corps:
    reperes = mesurer(mesh)
    origines = sauvegarde[chemin]
    noms_mi = []
    mesh.modify()
    mats = mesh.get_editor_property("materials")
    for i in range(len(mats)):
        # une instance par partie du corps (chaque partie garde SA texture d'origine)
        nom_mi = PREFIXE_MI + mesh.get_name() + ("" if i == 0 else "_%d" % i)
        chemin_mi = DOSSIER + "/" + nom_mi
        if EAL.does_asset_exist(chemin_mi):
            mi = unreal.load_asset(chemin_mi)
        else:
            mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(nom_mi, DOSSIER, unreal.MaterialInstanceConstant, usine)
        MEL.set_material_instance_parent(mi, mat)
        orig = origines[i] if i < len(origines) else ""
        t = textures_de(unreal.load_asset(orig) if orig else None)
        tex = t[0] if t else tex_defaut
        MEL.set_material_instance_texture_parameter_value(mi, "Texture", tex)
        for k, v in reperes.items():
            MEL.set_material_instance_scalar_parameter_value(mi, k, float(v))
        MEL.update_material_instance(mi)
        EAL.save_loaded_asset(mi, False)
        log("%s partie %d : texture %s, yeux au-dessus de z=%.1f, devant=%.1f"
            % (mesh.get_name(), i, tex.get_name(), reperes["YeuxBas"], reperes["YeuxAvant"]))
        m = mats[i]
        m.set_editor_property("material_interface", mi)
        mats[i] = m
        noms_mi.append(nom_mi)
    mesh.set_editor_property("materials", mats)
    try:
        mesh.post_edit_change()
    except Exception:
        pass
    EAL.save_loaded_asset(mesh, False)

    for i, m in enumerate(mesh.get_editor_property("materials")):
        x = m.get_editor_property("material_interface")
        if not x or x.get_name() != noms_mi[i]:
            problemes.append(mesh.get_name())

if problemes:
    raise RuntimeError("Materiau pas pose sur : " + ", ".join(sorted(set(problemes))) + ". Envoie le Output Log a Claude.")
log("TERMINE : %d corps Spider-Male sans blanc. Lance le jeu, TAB > Spider-Male pour verifier." % len(corps))
