# -*- coding: utf-8 -*-
"""RETIRE L'EVENEMENT DE NOEL de la carte Rachid - lance par RETIRER_NOEL.bat (Unreal ferme).

Lance Tools/Neige/retirer_neige.py : enleve les flocons et les empreintes, rend au Landscape son
materiau d'ORIGINE (celui d'avant la neige), puis enregistre Rachid.
Les assets de /Game/Chelles/Noel et /Game/Chelles/Neige restent (INSTALLER_NOEL.bat les reutilise).
Journal : Tools/Noel/retirer_noel.log
"""
import os
import runpy
import traceback

import unreal

CARTE = "/Game/Maps/Rachid"
TAG_SAISON = "ChellesNoelSaison"
OUTILS = os.path.join(unreal.SystemLibrary.get_project_directory(), "Tools")
JOURNAL = os.path.join(OUTILS, "Noel", "retirer_noel.log")
open(JOURNAL, "w").write("")


def log(msg):
    unreal.log("[Noel] " + str(msg))
    with open(JOURNAL, "a", encoding="utf-8") as f:
        f.write(str(msg) + "\n")


try:
    ell = unreal.EditorLevelLibrary
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        ell.load_level(CARTE)
    for p in ell.get_all_level_actors():
        if isinstance(p, unreal.LandscapeProxy):
            tags = [str(t) for t in p.get_editor_property("tags")]
            if TAG_SAISON in tags:
                p.set_editor_property("tags", [unreal.Name(t) for t in tags if t != TAG_SAISON])
    runpy.run_path(os.path.join(OUTILS, "Neige", "retirer_neige.py"), run_name="retirer_neige_pour_noel")
    ok = ell.save_current_level()
    log("TERMINE : neige et sol de Noel retires, materiau d'origine remis (Rachid enregistree : %s)." % ok)
    log("Le plugin ChellesNoel met encore un bonnet aux passants en decembre : "
        "mettez Mode=Non dans Plugins/ChellesNoel/Noel.txt pour l'arreter aussi.")
except Exception:
    log("ERREUR :\n" + traceback.format_exc())
