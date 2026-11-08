# -*- coding: utf-8 -*-
# CHELLES 3D - Costume Spider-Man pour les personnages SuperPowers (Super, Flame, Speedster).
# Dans Unreal : File > Execute Python Script... > choisir ce fichier.
# Ne modifie rien d'existant : cree seulement /Game/Personnages/SK_SpiderMan_UE4/SK_SpiderMan_Pouvoirs
# (le meme Spider-Man, mais sur le squelette du pack SuperPowers pour garder ses animations).
import os, unreal

ICI = os.path.dirname(os.path.abspath(__file__))
FBX = os.path.join(ICI, "SK_SpiderMan_UE4.fbx")
DEST = "/Game/Personnages/SK_SpiderMan_UE4"
NOM = "SK_SpiderMan_Pouvoirs"
SQUELETTE = "/Game/SuperPowers/Demo/Character/Mesh/UE4_Mannequin_Skeleton"
MODELE = DEST + "/SK_SpiderMan_UE4"  # on lui reprend ses materiaux

def log(m):
    unreal.log_warning("[SpiderManPouvoirs] " + m)

if not os.path.isfile(FBX):
    raise RuntimeError("Fichier introuvable : " + FBX + " (mets ce script dans Tools\\SpiderManJoueur)")
skel = unreal.load_asset(SQUELETTE)
if not skel:
    raise RuntimeError("Squelette SuperPowers introuvable : " + SQUELETTE)

opt = unreal.FbxImportUI()
opt.import_mesh = True
opt.import_as_skeletal = True
opt.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
opt.skeleton = skel
opt.import_animations = False
opt.import_materials = False
opt.import_textures = False
opt.create_physics_asset = True
opt.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
opt.skeletal_mesh_import_data.set_editor_property("use_t0_as_ref_pose", False)

t = unreal.AssetImportTask()
t.filename = FBX
t.destination_path = DEST
t.destination_name = NOM
t.automated = True
t.replace_existing = True
t.save = True
t.options = opt
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])

mesh = unreal.load_asset(DEST + "/" + NOM)
if not mesh:
    raise RuntimeError("Import rate : " + DEST + "/" + NOM)

modele = unreal.load_asset(MODELE)
if modele:
    mesh.set_editor_property("materials", modele.get_editor_property("materials"))
    log("Materiaux repris de SK_SpiderMan_UE4.")
else:
    log("SK_SpiderMan_UE4 introuvable : Spider-Man restera sans materiau.")
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
log("TERMINE : " + DEST + "/" + NOM + " cree sur le squelette SuperPowers.")
