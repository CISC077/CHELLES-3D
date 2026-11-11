# -*- coding: utf-8 -*-
# CHELLES 3D - Corps du soldat pour Spider-Femelle et PANTERA (2026-11-11)
# Dans Unreal : File > Execute Python Script... > choisir ce fichier.
#
# Dans la roue TAB, Spider-Male, Spider-Femelle et PANTERA jouent le SOLDAT au sol (armes, tir, vie)
# et volent en l'air. Pour le soldat il faut leur corps sur le squelette du soldat FPS_Game
# (celui de SK_SpiderMan_UE4). Ce script cree :
#   /Game/Personnages/SK_SpiderFemelle/SK_SpiderFemelle_Soldat  (copie de SK_SpiderFemelle_Vol)
#   /Game/Personnages/SK_Pantera/SK_Pantera_Soldat              (copie de SK_Pantera_Vol)
# en exportant le corps de vol en FBX puis en le reimportant sur le squelette du soldat,
# avec les memes materiaux.
#
# Ne modifie ni ne supprime rien d'existant, sauf une chose : Unreal peut ajouter au squelette
# du soldat les petits os de bout de doigts du corps feminin (os "_end", sans effet sur les
# animations). Une copie du squelette est faite avant, par securite.
# On peut le relancer sans risque (il remet seulement les materiaux).
import os, unreal

ICI = os.path.dirname(os.path.abspath(__file__))
REFERENCE_SOLDAT = "/Game/Personnages/SK_SpiderMan_UE4/SK_SpiderMan_UE4"  # Spider-Male du soldat
COPIE_SQUELETTE = "/Game/Personnages/Sauvegarde/UE4_Mannequin_Skeleton_soldat_avant_heros"
HEROS = [
    ("/Game/Personnages/SK_SpiderFemelle/SK_SpiderFemelle_Vol", "/Game/Personnages/SK_SpiderFemelle", "SK_SpiderFemelle_Soldat"),
    ("/Game/Personnages/SK_Pantera/SK_Pantera_Vol", "/Game/Personnages/SK_Pantera", "SK_Pantera_Soldat"),
]
EAL = unreal.EditorAssetLibrary


def log(m):
    unreal.log_warning("[HerosSoldat] " + m)


def charger(chemin):
    if not EAL.does_asset_exist(chemin):
        return None
    return unreal.load_asset(chemin)


def copier_materiaux(dest, source):
    """Met sur dest les materiaux de source, case par case (en Python chaque case est une copie)."""
    src = source.get_editor_property("materials")
    mats = dest.get_editor_property("materials")
    for i in range(len(mats)):
        if i >= len(src):
            break
        m = mats[i]
        m.set_editor_property("material_interface", src[i].get_editor_property("material_interface"))
        mats[i] = m
    dest.set_editor_property("materials", mats)
    dest.modify()
    EAL.save_loaded_asset(dest, False)


reference = charger(REFERENCE_SOLDAT)
if not reference:
    raise RuntimeError("Spider-Male du soldat introuvable : " + REFERENCE_SOLDAT)
skel = reference.get_editor_property("skeleton")
if not skel:
    raise RuntimeError("Squelette du soldat introuvable sur " + REFERENCE_SOLDAT)
log("Squelette du soldat : " + skel.get_path_name())

# Copie de securite du squelette du soldat (une seule fois)
chemin_skel = skel.get_path_name().split(".")[0]
if not EAL.does_asset_exist(COPIE_SQUELETTE):
    if EAL.duplicate_asset(chemin_skel, COPIE_SQUELETTE):
        log("Copie du squelette faite : " + COPIE_SQUELETTE)

faits = 0
for source_chemin, dossier, nom in HEROS:
    source = charger(source_chemin)
    if not source:
        log("ABSENT : " + source_chemin + " (lance d'abord son script de creation) -> saute.")
        continue
    deja = charger(dossier + "/" + nom)
    if deja:
        copier_materiaux(deja, source)
        log(nom + " existe deja : materiaux remis.")
        faits += 1
        continue

    # 1. Export du corps de vol en FBX (a cote de ce script)
    fbx = os.path.join(ICI, nom + ".fbx")
    tache = unreal.AssetExportTask()
    tache.set_editor_property("object", source)
    tache.set_editor_property("filename", fbx)
    tache.set_editor_property("automated", True)
    tache.set_editor_property("prompt", False)
    tache.set_editor_property("replace_identical", True)
    try:
        tache.set_editor_property("exporter", unreal.SkeletalMeshExporterFBX())
    except Exception:
        pass
    opt_fbx = unreal.FbxExportOption()
    for cle, val in (("ascii", False), ("level_of_detail", False), ("export_morph_targets", False), ("export_preview_mesh", False)):
        try:
            opt_fbx.set_editor_property(cle, val)
        except Exception:
            pass
    tache.set_editor_property("options", opt_fbx)
    if not unreal.Exporter.run_asset_export_task(tache) or not os.path.isfile(fbx):
        log("Export FBX rate pour " + nom + " -> saute.")
        continue

    # 2. Import sur le squelette du soldat
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
    t.filename = fbx
    t.destination_path = dossier
    t.destination_name = nom
    t.automated = True
    t.replace_existing = False
    t.save = True
    t.options = opt
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])

    mesh = charger(dossier + "/" + nom)
    if not mesh:
        log("Import rate pour " + nom + " (voir Output Log).")
        continue
    copier_materiaux(mesh, source)
    log(nom + " cree.")
    faits += 1

EAL.save_loaded_asset(skel, True)  # seulement si Unreal y a ajoute des os
log("TERMINE : %d corps prets. Recompile ChellesRoue et ChellesArmes, puis TAB en jeu." % faits)
