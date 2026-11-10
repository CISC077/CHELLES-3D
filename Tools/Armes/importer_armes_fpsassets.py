# -*- coding: utf-8 -*-
# CHELLES 3D - Importe les 2 armes du pack FPSAssets (AK et M4A1) qui ne sont que des fichiers bruts
# (AK.obj, M4A1.fbx + leurs images). Le plugin ChellesArmes s'en sert pour les armes "UE4" et "Template".
#
# A lancer UNE fois dans Unreal : Fichier > Executer un script Python... > ce fichier.
# Ne modifie rien d'existant : cree seulement, dans /Game/FPSAssets/Weapon :
#   SM_AK, SM_M4A1 (modeles), T_AK_D, T_AK_N, T_M4A1_D (images), M_AK_Chelles, M_M4A1_Chelles (materiaux).
# Pour annuler : supprimer ces 7 elements dans le Content Browser.

import os
import unreal

DEST = "/Game/FPSAssets/Weapon"
PROJET = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCES = [
    os.path.join(PROJET, "Content", "FPSAssets", "Weapon"),
    os.path.join(PROJET, "_Quarantaine", "Content", "FPSAssets", "Weapon"),
]

outils = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def trouver(nom):
    for dossier in SOURCES:
        chemin = os.path.join(dossier, nom)
        if os.path.isfile(chemin):
            return chemin
    return None


def importer(fichier, nom, modele=False):
    chemin_ue = DEST + "/" + nom
    if lib.does_asset_exist(chemin_ue):
        unreal.log("[Armes] %s existe deja, garde tel quel." % chemin_ue)
        return lib.load_asset(chemin_ue)
    source = trouver(fichier)
    if not source:
        unreal.log_warning("[Armes] %s introuvable (ni dans Content ni dans _Quarantaine)." % fichier)
        return None
    tache = unreal.AssetImportTask()
    tache.set_editor_property("filename", source)
    tache.set_editor_property("destination_path", DEST)
    tache.set_editor_property("destination_name", nom)
    tache.set_editor_property("automated", True)
    tache.set_editor_property("replace_existing", False)
    tache.set_editor_property("save", True)
    if modele:
        options = unreal.FbxImportUI()
        options.set_editor_property("import_mesh", True)
        options.set_editor_property("import_as_skeletal", False)
        options.set_editor_property("import_animations", False)
        options.set_editor_property("import_materials", False)
        options.set_editor_property("import_textures", False)
        options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        donnees = options.get_editor_property("static_mesh_import_data")
        donnees.set_editor_property("combine_meshes", True)
        donnees.set_editor_property("auto_generate_collision", False)
        tache.set_editor_property("options", options)
    outils.import_asset_tasks([tache])
    objet = lib.load_asset(chemin_ue)
    if objet:
        unreal.log("[Armes] Importe : %s" % chemin_ue)
    else:
        unreal.log_warning("[Armes] Import rate : %s" % fichier)
    return objet


def materiau(nom, couleur, normale):
    chemin_ue = DEST + "/" + nom
    if lib.does_asset_exist(chemin_ue):
        return lib.load_asset(chemin_ue)
    mat = outils.create_asset(nom, DEST, unreal.Material, unreal.MaterialFactoryNew())
    if couleur:
        n = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, -100)
        n.set_editor_property("texture", couleur)
        mel.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    if normale:
        n = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, 200)
        n.set_editor_property("texture", normale)
        n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        mel.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    metal = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -250, 450)
    metal.set_editor_property("r", 0.6)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    rugueux = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -250, 550)
    rugueux.set_editor_property("r", 0.45)
    mel.connect_material_property(rugueux, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    lib.save_loaded_asset(mat)
    unreal.log("[Armes] Materiau cree : %s" % chemin_ue)
    return mat


def habiller(modele, mat):
    if not modele or not mat:
        return
    nb = len(modele.get_editor_property("static_materials"))
    for i in range(nb):
        modele.set_material(i, mat)
    lib.save_loaded_asset(modele)


# --- Images ---
t_ak = importer("AK_diffuse.jpg", "T_AK_D")
t_ak_n = importer("AK_normal.jpg", "T_AK_N")
if t_ak_n:
    t_ak_n.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    t_ak_n.set_editor_property("srgb", False)
    lib.save_loaded_asset(t_ak_n)
t_m4 = importer("M4A1_Diffuse.png", "T_M4A1_D")

# --- Modeles ---
sm_ak = importer("AK.obj", "SM_AK", modele=True)
sm_m4 = importer("M4A1.fbx", "SM_M4A1", modele=True)

# --- Materiaux ---
habiller(sm_ak, materiau("M_AK_Chelles", t_ak, t_ak_n))
habiller(sm_m4, materiau("M_M4A1_Chelles", t_m4, None))

unreal.log("[Armes] Termine. Lancez le jeu : les armes UE4 / Template du kit FPS_Game prennent ces modeles.")
