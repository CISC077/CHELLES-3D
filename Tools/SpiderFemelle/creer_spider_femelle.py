# -*- coding: utf-8 -*-
# CHELLES 3D - Spider-Femelle (2026-11-11)
# Dans Unreal : File > Execute Python Script... > choisir ce fichier.
#
# Cree le corps de Spider-Femelle pour la roue TAB (case a cote de Spider-Male) :
#   /Game/Personnages/SK_SpiderFemelle/SK_SpiderFemelle_Vol
# = la femme "Robot sans yeux" du pack Female_Mannequin (visage lisse, comme un masque),
#   en costume rouge et noir, posee sur le squelette du pack SuperheroFlight pour voler
#   avec les memes animations que Spider-Male.
#
# Ne modifie ni ne supprime rien d'existant, sauf une chose : Unreal ajoute au squelette
# SuperheroFlight les petits os de bout de doigts du pack feminin (os "_end", sans effet
# sur les animations). Une copie du squelette est faite avant, par securite.
# On peut le relancer sans risque.
import os, unreal

ICI = os.path.dirname(os.path.abspath(__file__))
FBX = os.path.join(ICI, "SK_SpiderFemelle.fbx")
DEST = "/Game/Personnages/SK_SpiderFemelle"
NOM = "SK_SpiderFemelle_Vol"
SQUELETTE = "/Game/SuperheroFlight/Characters/Mannequin/Character/Mesh/UE4_Mannequin_Skeleton"
COPIE_SQUELETTE = DEST + "/Sauvegarde/UE4_Mannequin_Skeleton_avant_SpiderFemelle"
# Corps feminin du pack (le premier trouve est pris)
SOURCES = [
    "/Game/Female_Mannequin/Meshes/Female_Mannequin/Tall_Standalone_Curvier/SK_F_MNQ_Robot_without_eyes_Curvier",
    "/Game/Female_Mannequin/Meshes/Female_Mannequin/Tall_Standalone/SK_F_MNQ_Robot_without_eyes",
]
COSTUME = "/Game/Female_Mannequin/Materials/Female_Mannequin/Body_and_Head/MI_FemaleMannequin_red_and_black_eyeless"


def log(m):
    unreal.log_warning("[SpiderFemelle] " + m)


def charger(chemin):
    if not unreal.EditorAssetLibrary.does_asset_exist(chemin):
        return None
    return unreal.load_asset(chemin)


def mettre_costume(mesh, source):
    """Reprend les materiaux du corps d'origine, et met le rouge et noir a la place du blanc."""
    rouge_noir = charger(COSTUME)
    mats = source.get_editor_property("materials") if source else mesh.get_editor_property("materials")
    for m in mats:
        mi = m.get_editor_property("material_interface")
        nom = mi.get_name().lower() if mi else ""
        if rouge_noir and "joint" not in nom:
            m.set_editor_property("material_interface", rouge_noir)
    mesh.set_editor_property("materials", mats)
    mesh.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)


source = None
for chemin in SOURCES:
    source = charger(chemin)
    if source:
        log("Corps de depart : " + chemin)
        break
if not source:
    raise RuntimeError("Pack Female_Mannequin introuvable dans Content : Spider-Femelle non creee.")

skel = charger(SQUELETTE)
if not skel:
    raise RuntimeError("Squelette SuperheroFlight introuvable : " + SQUELETTE)

deja = charger(DEST + "/" + NOM)
if deja:
    mettre_costume(deja, source)
    log(NOM + " existe deja : costume rouge et noir remis. TERMINE")
else:
    # 1. Copie de securite du squelette SuperheroFlight (une seule fois)
    if not unreal.EditorAssetLibrary.does_asset_exist(COPIE_SQUELETTE):
        if unreal.EditorAssetLibrary.duplicate_asset(SQUELETTE, COPIE_SQUELETTE):
            log("Copie du squelette faite : " + COPIE_SQUELETTE)

    # 2. Export du corps feminin en FBX (a cote de ce script)
    tache = unreal.AssetExportTask()
    tache.set_editor_property("object", source)
    tache.set_editor_property("filename", FBX)
    tache.set_editor_property("automated", True)
    tache.set_editor_property("prompt", False)
    tache.set_editor_property("replace_identical", True)
    try:
        tache.set_editor_property("exporter", unreal.SkeletalMeshExporterFBX())
    except Exception:
        pass  # Unreal choisit l'exporteur FBX tout seul
    opt_fbx = unreal.FbxExportOption()
    for cle, val in (("ascii", False), ("level_of_detail", False), ("export_morph_targets", False), ("export_preview_mesh", False)):
        try:
            opt_fbx.set_editor_property(cle, val)
        except Exception:
            pass
    tache.set_editor_property("options", opt_fbx)
    if not unreal.Exporter.run_asset_export_task(tache) or not os.path.isfile(FBX):
        raise RuntimeError("Export FBX rate : " + FBX)
    log("FBX exporte : " + FBX)

    # 3. Import sur le squelette SuperheroFlight (comme SK_SpiderMan_Vol)
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

    mesh = charger(DEST + "/" + NOM)
    if not mesh:
        raise RuntimeError("Import rate (voir Output Log) : " + DEST + "/" + NOM)
    mettre_costume(mesh, source)
    unreal.EditorAssetLibrary.save_loaded_asset(skel, False)
    log(NOM + " cree.")

log("TERMINE : maintiens TAB en jeu et choisis Spider-Femelle.")
