# -*- coding: utf-8 -*-
"""Enleve de Rachid les JetPacks poses par installer_jetpacks.py (etiquette ChellesJetPackAjoute).
Les Blueprints BP_JetPack* du dossier Bonus restent (on peut toujours en poser a la main).
Ceux poses a la main ne sont pas touches.
"""
import os
import time
import traceback

import unreal

TAG_AJOUTE = "ChellesJetPackAjoute"
ell = unreal.EditorLevelLibrary
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
JOURNAL = os.path.join(PROJET, "Tools", "JetPack", "jetpack.log")


def log(msg):
    unreal.log("[JetPack] " + str(msg))
    try:
        with open(JOURNAL, "a", encoding="utf-8") as f:
            f.write(time.strftime("%H:%M:%S ") + str(msg) + "\n")
    except Exception:
        pass


try:
    open(JOURNAL, "w").close()
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        ell.load_level("/Game/Maps/Rachid")
    n = 0
    for a in ell.get_all_level_actors():
        try:
            if any(str(t) == TAG_AJOUTE for t in a.get_editor_property("tags")):
                ell.destroy_actor(a)
                n += 1
        except Exception:
            pass
    ell.save_current_level()
    log("%d JetPack(s) enleve(s) de Rachid. Rachid enregistree." % n)
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
