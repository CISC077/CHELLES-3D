# -*- coding: utf-8 -*-
# CHELLES 3D - Tenue de Spider-Femelle (2026-11-11, d'apres les photos envoyees par MERIDJA)
# Dans Unreal : File > Execute Python Script... > choisir ce fichier.
#
#   combinaison noire, epaules / haut du torse / bras blancs, losange blanc sur la poitrine,
#   avant-bras roses avec une toile blanche, mains blanches, cagoule blanche,
#   masque blanc avec de grands yeux bordes de rose, baskets bleu canard a semelle blanche.
#
# Une seule matiere, M_SpiderFemelle_Tenue, qui "peint" les zones selon la place sur le corps
# (position du corps au repos : les zones suivent les bras et les jambes quand elle bouge).
# Les reperes (cou, epaules, coudes, poignets, chevilles, tete) sont mesures sur le squelette.
#
# Ne touche QUE /Game/Personnages/SK_SpiderFemelle/ : Spider-Male et les autres ne changent pas.
# On peut le relancer sans risque. Il faut avoir lance creer_spider_femelle.py avant.
#
# Retouche possible : ouvrir M_SpiderFemelle_Tenue, changer les valeurs des parametres
# (ex. YeuxHaut pour monter les yeux, CouleurRose...), puis Apply et Save.
import unreal

DEST = "/Game/Personnages/SK_SpiderFemelle"
MESH = DEST + "/SK_SpiderFemelle_Vol"
NOM_MAT = "M_SpiderFemelle_Tenue"

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def log(m):
    unreal.log_warning("[SpiderFemelleTenue] " + m)


mesh = unreal.load_asset(MESH) if EAL.does_asset_exist(MESH) else None
if not mesh:
    raise RuntimeError("SK_SpiderFemelle_Vol introuvable : lance d'abord Tools\\SpiderFemelle\\creer_spider_femelle.py")

# ---------------------------------------------------------------------------
# 1. Mesures sur le squelette (position de repos)
# ---------------------------------------------------------------------------
os_lus = {}
acteur = None
try:
    acteur = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, -100000), unreal.Rotator(0, 0, 0))
    comp = acteur.get_editor_property("skeletal_mesh_component")
    comp.set_skeletal_mesh(mesh)
    base = acteur.get_actor_location()
    for nom in ("head", "neck_01", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "foot_l", "ball_l", "spine_03", "pelvis"):
        p = comp.get_socket_location(nom)
        os_lus[nom] = (p.x - base.x, p.y - base.y, p.z - base.z)
except Exception as e:
    log("Mesure des os impossible (" + str(e) + ") : valeurs moyennes utilisees.")
finally:
    if acteur:
        acteur.destroy_actor()

for k, v in sorted(os_lus.items()):
    log("os %-11s x=%7.1f y=%7.1f z=%7.1f" % (k, v[0], v[1], v[2]))

ok = len(os_lus) >= 9 and os_lus["head"][2] > 50
if ok:
    pied, bout = os_lus["foot_l"], os_lus["ball_l"]
    # Sens du regard : le bout du pied est devant la cheville
    dx, dy = bout[0] - pied[0], bout[1] - pied[1]
    avant_y = abs(dy) >= abs(dx)
    sens = 1.0 if (dy if avant_y else dx) >= 0 else -1.0
    lat = lambda n: abs(os_lus[n][0] if avant_y else os_lus[n][1])
    av = lambda n: (os_lus[n][1] if avant_y else os_lus[n][0]) * sens
    P = {
        "FwdIsY": 1.0 if avant_y else 0.0,
        "FwdSign": sens,
        "CouZ": os_lus["neck_01"][2],
        "ClavZ": os_lus["clavicle_l"][2],
        "EpauleL": lat("upperarm_l"),
        "CoudeL": lat("lowerarm_l"),
        "PoignetL": lat("hand_l"),
        "ChevilleZ": os_lus["foot_l"][2],
        "TeteZ": os_lus["head"][2],
        "TeteAvant": av("head"),
    }
else:
    log("Os non lus : reperes d'un mannequin UE4 standard (a regler dans le materiau si besoin).")
    P = {"FwdIsY": 1.0, "FwdSign": 1.0, "CouZ": 150.0, "ClavZ": 145.0, "EpauleL": 15.0, "CoudeL": 32.0,
         "PoignetL": 52.0, "ChevilleZ": 13.0, "TeteZ": 160.0, "TeteAvant": 0.0}
# Reglages du dessin (modifiables dans le materiau)
P.update({"YeuxHaut": 7.0, "YeuxEcart": 3.4, "YeuxLarge": 2.7, "YeuxRond": 1.9, "Toile": 2.4})
for k in sorted(P):
    log("parametre %-10s = %.2f" % (k, P[k]))

# ---------------------------------------------------------------------------
# 2. Le materiau (une seule matiere, zones calculees)
# ---------------------------------------------------------------------------
CODE = r"""
float lat = (FwdIsY > 0.5) ? P.x : P.y;
float fwd = ((FwdIsY > 0.5) ? P.y : P.x) * FwdSign;
float al = abs(lat);
float z = P.z;

float3 noir   = float3(0.010, 0.010, 0.012);
float3 blanc  = float3(0.82, 0.82, 0.84);
float3 rose   = CouleurRose.rgb;
float3 canard = float3(0.0, 0.16, 0.17);
float3 verre  = float3(0.45, 0.47, 0.50);

// Baskets : bleu canard, semelle blanche
if (z < ChevilleZ + 4.0)
{
    return (z < 2.5) ? blanc : canard;
}

// Avant-bras : rose avec une toile blanche ; mains blanches
if (al > CoudeL - 1.0)
{
    if (al > PoignetL + 1.5) return blanc;
    float t = Toile;
    float l1 = abs(frac(al / t) - 0.5);
    float l2 = abs(frac(fwd / t) - 0.5);
    float l3 = abs(frac(z / t) - 0.5);
    float ligne = max(max(step(0.44, l1), step(0.44, l2)), step(0.44, l3));
    return lerp(rose, blanc, ligne);
}

// Haut des bras : blanc
if (al > EpauleL && z > ClavZ - 40.0)
{
    return blanc;
}

// Tete : cagoule et masque blancs, grands yeux bordes de rose
if (z > CouZ + 1.0)
{
    float ex = (al - YeuxEcart) / YeuxLarge;
    float ez = (z - (TeteZ + YeuxHaut) - 0.35 * (al - YeuxEcart)) / YeuxRond;
    float d = ex * ex + ez * ez;
    if (fwd > TeteAvant + 3.0)
    {
        if (d < 1.0) return verre;
        if (d < 1.55) return rose;
    }
    return blanc;
}

// Epaules et haut du torse blancs
if (z > ClavZ - 5.0)
{
    return blanc;
}

// Losange blanc sur la poitrine (devant)
if (fwd > 0.0 && al / 6.0 + abs(z - (ClavZ - 13.0)) / 8.0 < 1.0)
{
    return blanc;
}

// Pointes blanches sur les cotes, sous les bras
if (al > EpauleL - 4.0 && z > ClavZ - 26.0 && z < ClavZ - 14.0 && abs(fwd) < 6.0)
{
    return blanc;
}

return noir;
"""


def scalaire(mat, nom, valeur, y):
    s = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, y)
    s.set_editor_property("parameter_name", nom)
    s.set_editor_property("default_value", float(valeur))
    return s


chemin_mat = DEST + "/" + NOM_MAT
if EAL.does_asset_exist(chemin_mat):
    mat = unreal.load_asset(chemin_mat)
    MEL.delete_all_material_expressions(mat)  # on refait le dessin (seulement dans ce materiau)
    log(NOM_MAT + " existe deja : dessin refait.")
else:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NOM_MAT, DEST, unreal.Material, unreal.MaterialFactoryNew())
    log(NOM_MAT + " cree.")
mat.set_editor_property("used_with_skeletal_mesh", True)

if not hasattr(unreal, "MaterialExpressionPreSkinnedLocalPosition"):
    raise RuntimeError("Noeud 'Pre-Skinned Local Position' absent de cet Unreal : envoie le Output Log a Claude.")
pos = MEL.create_material_expression(mat, unreal.MaterialExpressionPreSkinnedLocalPosition, -1200, -400)
# Ce noeud ne se lit que dans le vertex shader : on le fait passer par un "Vertex Interpolator"
if hasattr(unreal, "MaterialExpressionVertexInterpolator"):
    passe = MEL.create_material_expression(mat, unreal.MaterialExpressionVertexInterpolator, -900, -400)
    if not MEL.connect_material_expressions(pos, "", passe, "VS"):
        MEL.connect_material_expressions(pos, "", passe, "")
    pos = passe

perso = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, -400, 0)
perso.set_editor_property("code", CODE)
perso.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
perso.set_editor_property("description", "Tenue Spider-Femelle")
noms = ["P"] + sorted(P.keys()) + ["CouleurRose"]
entrees = []
for n in noms:
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", n)
    entrees.append(ci)
perso.set_editor_property("inputs", entrees)

MEL.connect_material_expressions(pos, "", perso, "P")
y = -250
for k in sorted(P.keys()):
    MEL.connect_material_expressions(scalaire(mat, k, P[k], y), "", perso, k)
    y += 70
rose = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, y)
rose.set_editor_property("parameter_name", "CouleurRose")
rose.set_editor_property("default_value", unreal.LinearColor(0.60, 0.02, 0.16, 1.0))
MEL.connect_material_expressions(rose, "", perso, "CouleurRose")

MEL.connect_material_property(perso, "", unreal.MaterialProperty.MP_BASE_COLOR)
rug = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 250)
rug.set_editor_property("r", 0.38)  # tissu un peu brillant, comme sur les photos
MEL.connect_material_property(rug, "", unreal.MaterialProperty.MP_ROUGHNESS)
MEL.recompile_material(mat)
EAL.save_loaded_asset(mat, False)

# ---------------------------------------------------------------------------
# 3. On met la tenue sur TOUTES les parties du corps de Spider-Femelle
#    (chaque case de la liste est une copie : il faut la remettre dans la liste)
# ---------------------------------------------------------------------------
mesh.modify()
mats = mesh.get_editor_property("materials")
for i in range(len(mats)):
    m = mats[i]
    m.set_editor_property("material_interface", mat)
    mats[i] = m
mesh.set_editor_property("materials", mats)
try:
    mesh.post_edit_change()
except Exception:
    pass
EAL.save_loaded_asset(mesh, False)

# Verification : on relit ce qui est enregistre
mauvais = []
for m in mesh.get_editor_property("materials"):
    mi = m.get_editor_property("material_interface")
    log("Materiau du corps : " + (mi.get_path_name() if mi else "aucun"))
    if not mi or mi.get_name() != NOM_MAT:
        mauvais.append(mi.get_name() if mi else "aucun")
if mauvais:
    raise RuntimeError("La tenue n'est pas posee partout (" + ", ".join(mauvais) + "). Envoie le Output Log a Claude.")

log("TERMINE : maintiens TAB en jeu et choisis Spider-Femelle.")
