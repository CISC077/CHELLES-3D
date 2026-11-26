# -*- coding: utf-8 -*-
"""Apres la suppression du pack SportVehiclesPack (25/11/2026) : /Game/Meshes/StaticMesh utilisait 21 materiaux du pack.
- Si plus rien n'utilise ce maillage (c'etait une voiture de police posee en decor, retiree de Rachid) :
  copie du .uasset dans la sauvegarde, puis suppression du maillage.
- Sinon : les emplacements de materiau vides / du pack recoivent un materiau neutre, puis le maillage est enregistre.
Journal : Tools/SuppressionPolice/suppression.log (lignes [StaticMesh]).
"""
import os
import shutil
import time
import traceback

import unreal

CIBLE = "/Game/Meshes/StaticMesh"
PACK = "/game/sportvehiclespack/"
NEUTRE = "/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ICI = os.path.join(PROJET, "Tools", "SuppressionPolice")
JOURNAL = os.path.join(ICI, "suppression.log")
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log("[StaticMesh] " + str(msg))
    with open(JOURNAL, "a", encoding="utf-8") as f:
        f.write(time.strftime("%H:%M:%S ") + "[StaticMesh] " + str(msg) + "\n")


def options():
    o = unreal.AssetRegistryDependencyOptions()
    for nom, val in (("include_soft_package_references", True), ("include_hard_package_references", True),
                     ("include_searchable_names", False), ("include_soft_management_references", False),
                     ("include_hard_management_references", False)):
        try:
            o.set_editor_property(nom, val)
        except Exception:
            pass
    return o


def sauvegarder():
    src = os.path.join(PROJET, "Content", "Meshes", "StaticMesh.uasset")
    dossier = os.path.join(PROJET, "_Sauvegardes", "SuppressionPolice_StaticMesh_" + time.strftime("%Y-%m-%d_%Hh%M"), "Content", "Meshes")
    os.makedirs(dossier, exist_ok=True)
    shutil.copy2(src, dossier)
    log("Copie : " + os.path.join(dossier, "StaticMesh.uasset"))


def main():
    log("==== REPARATION DE /Game/Meshes/StaticMesh ====")
    if not eal.does_asset_exist(CIBLE):
        log("Le maillage n'existe plus : rien a faire.")
        return
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    try:
        ar.scan_paths_synchronous(["/Game/Maps", "/Game/Meshes"], True)
    except Exception:
        pass
    refs = [str(r) for r in (ar.get_referencers(CIBLE, options()) or []) if str(r) != CIBLE]
    for r in refs:
        log("  utilise par : " + r)
    sauvegarder()
    if not refs:
        if eal.delete_asset(CIBLE):
            log("Plus personne ne l'utilisait (voiture de police) : maillage SUPPRIME.")
        else:
            log("ATTENTION : suppression refusee par Unreal.")
        return
    sm = unreal.load_asset(CIBLE)
    neutre = unreal.load_asset(NEUTRE)
    nb = 0
    for i, m in enumerate(sm.static_materials):
        mi = m.material_interface
        p = mi.get_path_name().lower() if mi else ""
        if mi is None or p.startswith(PACK):
            sm.set_material(i, neutre)
            nb += 1
    log("%d materiau(x) remplace(s) par un materiau neutre." % nb)
    log("Enregistre." if eal.save_loaded_asset(sm) else "ATTENTION : enregistrement refuse.")


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
