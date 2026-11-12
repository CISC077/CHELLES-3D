# -*- coding: utf-8 -*-
"""
Cree des materiaux de REMPLACEMENT simples pour les packs qui n'existent
plus du tout dans le projet (GKnight, Mosaic_Windows, GothicChapel,
CityPark, Classic_Shooter). Ils sont ranges dans /Game/Remplacements/...
et Config\\DefaultEngine.ini redirige les anciens chemins vers eux.

Rien n'est supprime. Si un jour tu reinstalles le vrai pack, il suffit
d'enlever la ligne de redirection correspondante dans DefaultEngine.ini.

Dans Unreal : File > Execute Python Script > choisir ce fichier.
Puis fermer et rouvrir Unreal. (Ou Creer_les_remplacements.bat, Unreal ferme.)
"""
import unreal

RACINE = "/Game/Remplacements"
BASE = RACINE + "/M_Remplacement_Base"

# couleurs (R, V, B), metal, rugosite
GRIS_METAL = ((0.35, 0.36, 0.38), 0.9, 0.35)
VITRAIL = ((0.12, 0.22, 0.45), 0.0, 0.1)
CADRE = ((0.08, 0.08, 0.08), 0.6, 0.5)
BOIS = ((0.25, 0.14, 0.07), 0.0, 0.7)
PIERRE = ((0.45, 0.43, 0.40), 0.0, 0.85)

MATERIAUX = []   # (chemin d'origine, classe, reglage)
for n in ["M_All_Frame"]:
    MATERIAUX.append(("/Game/Mosaic_Windows/Material/" + n, "Material", CADRE))
for i in [1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
          21, 22, 23, 29, 32, 37, 38, 40]:
    MATERIAUX.append(("/Game/Mosaic_Windows/Material/M_Window_%d" % i, "Material", VITRAIL))
MATERIAUX.append(("/Game/Classic_Shooter/Level/Architecture/Materials/MacroSpaceHull",
                  "Material", GRIS_METAL))
GK = {
    "MI_BearFur": ((0.22, 0.15, 0.10), 0.0, 0.9),
    "MI_EyeMaterial": ((0.85, 0.85, 0.85), 0.0, 0.3),
    "MI_GKnight_Body": GRIS_METAL,
    "MI_GKnight_Glove": GRIS_METAL,
    "MI_GKnight_Helmet": GRIS_METAL,
    "MI_GKnight_Pants": ((0.20, 0.17, 0.14), 0.0, 0.8),
    "MI_GKnightCape": ((0.35, 0.04, 0.04), 0.0, 0.8),
    "MI_GKnightSkirt": ((0.20, 0.17, 0.14), 0.0, 0.8),
    "MI_headA": ((0.70, 0.52, 0.42), 0.0, 0.6),
    "MI_headD": ((0.70, 0.52, 0.42), 0.0, 0.6),
}
for n, r in GK.items():
    MATERIAUX.append(("/Game/GKnight/Materials/" + n, "MaterialInstanceConstant", r))
MATERIAUX.append(("/Game/GothicChapel/Materials/Tiled/Window_Glass_A", "MaterialInstanceConstant", VITRAIL))
MATERIAUX.append(("/Game/GothicChapel/Materials/Unique/MI_Door_ABC", "MaterialInstanceConstant", BOIS))
MATERIAUX.append(("/Game/CityPark/Materials/ParkSquare/MI_ParkSquare13", "MaterialInstanceConstant", PIERRE))

import os
import shutil

# 0) deux grosses textures de ModularFences : copie depuis la quarantaine
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
for _n in ("Tex_Decorative_Brick_D", "Tex_Decorative_Brick_N"):
    _src = os.path.join(PROJET, "_Quarantaine", "Content", "ModularFences", "Textures", _n + ".uasset")
    _dst = os.path.join(PROJET, "Content", "ModularFences", "Textures", _n + ".uasset")
    if not os.path.exists(_dst) and os.path.exists(_src):
        shutil.copy2(_src, _dst)
        unreal.log("[Remplacements] texture recopiee : " + _n)

outils = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def log(m):
    unreal.log("[Remplacements] " + m)


def remplir_materiau(mat, reglage, parametres):
    (r, v, b), metal, rug = reglage
    if parametres:
        c = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, -100)
        c.set_editor_property("parameter_name", "Couleur")
        m = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 100)
        m.set_editor_property("parameter_name", "Metal")
        g = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 200)
        g.set_editor_property("parameter_name", "Rugosite")
    else:
        c = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, -100)
        m = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 100)
        g = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 200)
    if parametres:
        c.set_editor_property("default_value", unreal.LinearColor(r, v, b, 1.0))
        m.set_editor_property("default_value", metal)
        g.set_editor_property("default_value", rug)
    else:
        c.set_editor_property("constant", unreal.LinearColor(r, v, b, 1.0))
        m.set_editor_property("r", metal)
        g.set_editor_property("r", rug)
    MEL.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(m, "", unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(g, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(mat)


def creer(chemin_complet, classe, factory):
    dossier, nom = chemin_complet.rsplit("/", 1)
    if EAL.does_asset_exist(chemin_complet):
        return None
    return outils.create_asset(nom, dossier, classe, factory)


# 1) materiau de base (pour les instances)
base = unreal.load_asset(BASE) if EAL.does_asset_exist(BASE) else None
if base is None:
    base = creer(BASE, unreal.Material, unreal.MaterialFactoryNew())
    remplir_materiau(base, GRIS_METAL, True)
    EAL.save_asset(BASE, only_if_is_dirty=False)
    log("cree " + BASE)

faits, deja = 0, 0
for origine, classe, reglage in MATERIAUX:
    cible = RACINE + origine[len("/Game"):]
    if EAL.does_asset_exist(cible):
        deja += 1
        continue
    if classe == "Material":
        a = creer(cible, unreal.Material, unreal.MaterialFactoryNew())
        remplir_materiau(a, reglage, False)
    else:
        a = creer(cible, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(a, base)
        (r, v, b), metal, rug = reglage
        MEL.set_material_instance_vector_parameter_value(a, "Couleur", unreal.LinearColor(r, v, b, 1.0))
        MEL.set_material_instance_scalar_parameter_value(a, "Metal", metal)
        MEL.set_material_instance_scalar_parameter_value(a, "Rugosite", rug)
        MEL.update_material_instance(a)
    EAL.save_asset(cible, only_if_is_dirty=False)
    faits += 1
    log("cree " + cible)

log("TERMINE : %d materiaux crees, %d existaient deja. Rien n'a ete supprime." % (faits, deja))
