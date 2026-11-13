# -*- coding: utf-8 -*-
# CHELLES 3D - ANNULER "Spider-Male sans blanc" (2026-11-13)
# Dans Unreal : File > Execute Python Script... > choisir ce fichier.
# Remet sur chaque corps Spider-Male les materiaux d'origine (lus dans sauvegarde_materiaux.json).
# Le materiau M_SpiderMale_SansBlanc et ses instances restent dans le dossier (inutilises, sans danger).
import json
import os
import unreal

EAL = unreal.EditorAssetLibrary
SAUVEGARDE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sauvegarde_materiaux.json")


def log(m):
    unreal.log_warning("[SpiderMaleSansBlanc] " + m)


if not os.path.exists(SAUVEGARDE):
    raise RuntimeError("Pas de sauvegarde_materiaux.json : rien a annuler.")
with open(SAUVEGARDE, "r") as f:
    sauvegarde = json.load(f)

for chemin, origines in sauvegarde.items():
    if not EAL.does_asset_exist(chemin):
        log("Introuvable (ignore) : " + chemin)
        continue
    mesh = unreal.load_asset(chemin)
    mesh.modify()
    mats = mesh.get_editor_property("materials")
    for i in range(min(len(mats), len(origines))):
        m = mats[i]
        m.set_editor_property("material_interface", unreal.load_asset(origines[i]) if origines[i] else None)
        mats[i] = m
    mesh.set_editor_property("materials", mats)
    try:
        mesh.post_edit_change()
    except Exception:
        pass
    EAL.save_loaded_asset(mesh, False)
    log("Remis d'origine : " + mesh.get_name())

log("TERMINE : Spider-Male a de nouveau son costume d'origine.")
