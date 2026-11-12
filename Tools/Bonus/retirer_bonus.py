# -*- coding: utf-8 -*-
"""ANNULE disperser_bonus.py dans la carte /Game/Maps/Rachid.

A lancer DANS L'EDITEUR : File > Execute Python Script... > Tools/Bonus/retirer_bonus.py
ou double-clic sur RETIRER_BONUS.bat (Unreal ferme).

Enleve seulement les bonus poses par le script (etiquette ChellesBonusAjoute, dossier Outliner "Bonus")
puis enregistre Rachid. Les bonus poses a la main ne sont pas touches.
Autre solution : remettre une copie de Tools/Bonus/sauvegarde/ a la place de Content/Maps/Rachid.umap (Unreal ferme).
"""
import os
import time
import traceback

import unreal

CARTE = "/Game/Maps/Rachid"
TAG_AJOUTE = "ChellesBonusAjoute"

ell = unreal.EditorLevelLibrary
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ICI = os.path.join(PROJET, "Tools", "Bonus")
JOURNAL = os.path.join(ICI, "bonus.log")


def log(msg):
    unreal.log("[Bonus] " + str(msg))
    try:
        os.makedirs(ICI, exist_ok=True)
        with open(JOURNAL, "a", encoding="utf-8") as f:
            f.write(time.strftime("%H:%M:%S ") + str(msg) + "\n")
    except Exception:
        pass


def main():
    log("---- RETRAIT ----")
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        ell.load_level(CARTE)
    enleves = 0
    for a in ell.get_all_level_actors():
        try:
            t = [str(x) for x in a.get_editor_property("tags")]
        except Exception:
            continue
        if TAG_AJOUTE in t:
            ell.destroy_actor(a)
            enleves += 1
    log("%d bonus enleves." % enleves)
    if ell.save_current_level():
        log("Rachid enregistree.")
    log("TERMINE.")


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
