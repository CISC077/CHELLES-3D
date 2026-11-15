# -*- coding: utf-8 -*-
"""ANNULE l'habillage : enleve de Rachid tous les modeles poses par poser_habillage.py
(etiquette ChellesHabillage, dossier "Habillage" de l'Outliner) puis enregistre la carte.
Les batiments simples et tout le reste ne sont pas touches.

Lancer : double-clic sur 3_ANNULER.bat (Unreal FERME)
     ou File > Execute Python Script... > Tools/HabillageBatiments/retirer_habillage.py
Autre solution : remettre une copie de Tools/HabillageBatiments/sauvegarde/ a la place de
Content/Maps/Rachid.umap (Unreal ferme).
"""
import os
import time
import traceback

import unreal

CARTE = "/Game/Maps/Rachid"
TAG_AJOUTE = "ChellesHabillage"

ell = unreal.EditorLevelLibrary
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
JOURNAL = os.path.join(PROJET, "Tools", "HabillageBatiments", "habillage.log")


def log(msg):
    unreal.log("[Habillage] " + str(msg))
    try:
        with open(JOURNAL, "a", encoding="utf-8") as f:
            f.write(time.strftime("%H:%M:%S ") + str(msg) + "\n")
    except Exception:
        pass


def main():
    log("---- ANNULATION ----")
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        ell.load_level(CARTE)
    n = 0
    for a in ell.get_all_level_actors():
        try:
            t = [str(x) for x in a.get_editor_property("tags")]
        except Exception:
            continue
        if TAG_AJOUTE in t:
            ell.destroy_actor(a)
            n += 1
    log("%d modeles enleves." % n)
    if ell.save_current_level():
        log("Rachid enregistree.")
    log("TERMINE.")


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
