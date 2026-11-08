# -*- coding: utf-8 -*-
# CHELLES 3D - Costume Spider-Man pour les personnages SuperPowers (Super, Flame, Speedster).
# Dans Unreal : File > Execute Python Script... > choisir ce fichier.
# Ne modifie rien d'existant : cree seulement, dans /Game/Personnages/SK_SpiderMan_UE4/,
# SK_SpiderMan_Pouvoirs (squelette SuperPowers) et SK_SpiderMan_Cavalier (squelette du cavalier HorseStarterKit).
# Le meme Spider-Man, sur le squelette de chaque pack pour garder ses animations. On peut le relancer sans risque.
import os, unreal

ICI = os.path.dirname(os.path.abspath(__file__))
FBX = os.path.join(ICI, "SK_SpiderMan_UE4.fbx")
DEST = "/Game/Personnages/SK_SpiderMan_UE4"
# (nom du mesh a creer, squelette du pack)
A_CREER = [
    ("SK_SpiderMan_Pouvoirs", "/Game/SuperPowers/Demo/Character/Mesh/UE4_Mannequin_Skeleton"),        # Super, Flame, Speedster
    ("SK_SpiderMan_Cavalier", "/Game/HorseStarterKit/ThirdPersonBP/Character/Mesh/UE4_Mannequin_Skeleton"),  # cavalier du cheval
]
MODELE = DEST + "/SK_SpiderMan_UE4"  # on lui reprend ses materiaux

def log(m):
    unreal.log_warning("[SpiderManPouvoirs] " + m)

if not os.path.isfile(FBX):
    raise RuntimeError("Fichier introuvable : " + FBX + " (mets ce script dans Tools\\SpiderManJoueur)")
modele = unreal.load_asset(MODELE)

for NOM, SQUELETTE in A_CREER:
    if unreal.EditorAssetLibrary.does_asset_exist(DEST + "/" + NOM):
        deja = unreal.load_asset(DEST + "/" + NOM)
        if deja and modele:
            deja.set_editor_property("materials", modele.get_editor_property("materials"))
            deja.modify()
            unreal.EditorAssetLibrary.save_loaded_asset(deja, False)
        log(NOM + " existe deja : materiaux Spider-Man remis.")
        continue
    skel = unreal.load_asset(SQUELETTE)
    if not skel:
        log("Squelette introuvable, " + NOM + " non cree : " + SQUELETTE)
        continue

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
        log("Import rate : " + DEST + "/" + NOM)
        continue
    if modele:
        mesh.set_editor_property("materials", modele.get_editor_property("materials"))
    mesh.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    log(NOM + " cree.")

log("TERMINE")
