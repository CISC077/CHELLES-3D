# -*- coding: utf-8 -*-
"""ANNULE installer_bar.py dans la carte /Game/Maps/Rachid.

A lancer DANS L'EDITEUR : File > Execute Python Script... > Tools/Bar/retirer_bar.py
ou double-clic sur RETIRER_LE_BAR.bat (Unreal ferme).

Enleve seulement les objets poses par le script (etiquette ChellesBar, dossier Outliner "Bar")
puis enregistre Rachid. Rien d'autre n'est touche.
Autre solution : remettre une copie de Tools/Bar/sauvegarde/ a la place de Content/Maps/Rachid.umap (Unreal ferme).
"""
import os
import time
import traceback

import unreal

CARTE = "/Game/Maps/Rachid"
TAG = "ChellesBar"

ell = unreal.EditorLevelLibrary
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ICI = os.path.join(PROJET, "Tools", "Bar")
JOURNAL = os.path.join(ICI, "bar.log")


def log(msg):
    unreal.log("[Bar] " + str(msg))
    try:
        os.makedirs(ICI, exist_ok=True)
        with open(JOURNAL, "a", encoding="utf-8") as f:
            f.write(time.strftime("%H:%M:%S ") + str(msg) + "\n")
    except Exception:
        pass


def main():
    log("---- RETRAIT DU BAR ----")
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        ell.load_level(CARTE)
    enleves = 0
    for a in ell.get_all_level_actors():
        try:
            t = [str(x) for x in a.get_editor_property("tags")]
        except Exception:
            continue
        if TAG in t:
            ell.destroy_actor(a)
            enleves += 1
    log("%d objets du bar enleves." % enleves)
    if ell.save_current_level():
        log("Rachid enregistree.")
    log("TERMINE.")


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
