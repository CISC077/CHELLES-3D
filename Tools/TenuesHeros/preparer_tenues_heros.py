# -*- coding: utf-8 -*-
# CHELLES 3D - Tenues des heros de la roue (touche K) - UE 4.27
#
# A lancer UNE fois dans Unreal : Fichier > Executer un script Python... > ce fichier.
# Ajoute 3 reglages aux materiaux de Spider-Male, Spider-Femelle et PANTERA :
#   ChellesTeinte  (0 = couleurs d'origine, 0.33 = tour de la roue des couleurs...)
#   ChellesUnie    (couleur unie)
#   ChellesMelange (0 = costume d'origine, 1 = couleur unie)
# Par defaut ils ne changent RIEN a l'image. Le plugin ChellesWraith les change avec K.
# Une copie de chaque materiau est gardee dans /Game/Personnages/Sauvegarde/TenuesHeros.
# Le script peut etre relance sans risque (il saute les materiaux deja prets).
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
SAUVE = "/Game/Personnages/Sauvegarde/TenuesHeros/"

# (materiau, couleur de base connue si c'est une simple couleur, sinon None)
MATERIAUX = [
    ("/Game/Personnages/SK_SpiderMan_UE4/M_SpiderMale_SansBlanc", None),
    ("/Game/Personnages/SK_SpiderFemelle/M_SpiderFemelle_Tenue", None),
    ("/Game/Personnages/SK_Pantera/M_Pantera_Costume", (0.008, 0.008, 0.010)),
    ("/Game/Personnages/SK_Pantera/M_Pantera_Accent", (0.045, 0.045, 0.055)),
]

CODE = """float3 k = float3(0.57735027, 0.57735027, 0.57735027);
float a = T * 6.2831853;
float ca = cos(a);
float sa = sin(a);
float3 r = C * ca + cross(k, C) * sa + k * dot(k, C) * (1.0 - ca);
r = max(r, 0.0);
float l = dot(C, float3(0.3, 0.59, 0.11));
float3 u = U * saturate(0.6 + 2.0 * l);
return lerp(r, u, saturate(K));"""


def log(m):
    unreal.log("[TenuesHeros] " + m)


def source_couleur(mat, couleur_connue):
    """Le noeud branche sur Base Color."""
    if hasattr(MEL, "get_material_property_input_node"):
        n = MEL.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
        if n is not None:
            return n
    if couleur_connue is not None:
        c = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -700, -200)
        c.set_editor_property("constant", unreal.LinearColor(couleur_connue[0], couleur_connue[1], couleur_connue[2], 1.0))
        return c
    return None


ok = 0
for chemin, couleur_connue in MATERIAUX:
    if not EAL.does_asset_exist(chemin):
        log("absent, saute : " + chemin)
        continue
    mat = EAL.load_asset(chemin)
    source = source_couleur(mat, couleur_connue)
    if source is None:
        unreal.log_error("[TenuesHeros] impossible de trouver la couleur de " + chemin + " (Unreal trop ancien ?)")
        continue
    if isinstance(source, unreal.MaterialExpressionCustom) and str(source.get_editor_property("description")) == "ChellesTeinte":
        log("deja pret : " + chemin)
        ok += 1
        continue

    nom = chemin.split("/")[-1]
    if not EAL.does_asset_exist(SAUVE + nom):
        EAL.duplicate_asset(chemin, SAUVE + nom)
        log("copie de securite : " + SAUVE + nom)

    perso = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, -200, -200)
    perso.set_editor_property("code", CODE)
    perso.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    perso.set_editor_property("description", "ChellesTeinte")
    entrees = []
    for n in ["C", "T", "U", "K"]:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        entrees.append(ci)
    perso.set_editor_property("inputs", entrees)

    teinte = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, -60)
    teinte.set_editor_property("parameter_name", "ChellesTeinte")
    teinte.set_editor_property("default_value", 0.0)
    unie = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 20)
    unie.set_editor_property("parameter_name", "ChellesUnie")
    unie.set_editor_property("default_value", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
    melange = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 180)
    melange.set_editor_property("parameter_name", "ChellesMelange")
    melange.set_editor_property("default_value", 0.0)

    liens = [
        MEL.connect_material_expressions(source, "", perso, "C"),
        MEL.connect_material_expressions(teinte, "", perso, "T"),
        MEL.connect_material_expressions(unie, "", perso, "U"),
        MEL.connect_material_expressions(melange, "", perso, "K"),
    ]
    if not all(liens):
        unreal.log_error("[TenuesHeros] liaison impossible dans " + chemin + " : materiau non modifie")
        continue
    MEL.connect_material_property(perso, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat, False)
    log("pret : " + chemin)
    ok += 1

log("TERMINE : %d materiau(x) prets. Lancez le jeu, prenez Spider-Male / Spider-Femelle / PANTERA et appuyez sur K." % ok)
