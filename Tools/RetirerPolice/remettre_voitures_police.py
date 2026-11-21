# -*- coding: utf-8 -*-
"""ANNULE retirer_voitures_police.py : remet les voitures de police dans Rachid, a leur place exacte.

Double-clic sur REMETTRE_VOITURES_POLICE.bat (Unreal FERME),
ou dans l'editeur : File > Execute Python Script... > Tools/RetirerPolice/remettre_voitures_police.py
Lit Tools/RetirerPolice/voitures_police_retirees.json (le fichier est renomme ensuite en .remis).
"""
import json
import os
import time
import traceback

import unreal

CARTE = "/Game/Maps/Rachid"
ell = unreal.EditorLevelLibrary
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ICI = os.path.join(PROJET, "Tools", "RetirerPolice")
JOURNAL = os.path.join(ICI, "police.log")
LISTE = os.path.join(ICI, "voitures_police_retirees.json")


def log(msg):
    unreal.log("[RemettrePolice] " + str(msg))
    try:
        with open(JOURNAL, "a", encoding="utf-8") as f:
            f.write(time.strftime("%H:%M:%S ") + str(msg) + "\n")
    except Exception:
        pass


def main():
    log("==== REMISE DES VOITURES DE POLICE ====")
    if not os.path.exists(LISTE):
        log("Pas de liste voitures_police_retirees.json : rien a remettre.")
        return
    voitures = json.load(open(LISTE, encoding="utf-8"))
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        ell.load_level(CARTE)
    remises = 0
    for d in voitures:
        try:
            pos = unreal.Vector(*d["position"])
            r = d["rotation"]
            rot = unreal.Rotator(r[0], r[1], r[2])
            classe = unreal.load_object(None, d["classe"])
            if classe is None:
                log("  classe introuvable : " + d["classe"])
                continue
            a = ell.spawn_actor_from_class(classe, pos, rot)
            if a is None:
                continue
            a.set_actor_scale3d(unreal.Vector(*d["taille"]))
            a.set_actor_label(d["nom"])
            if d.get("dossier") and d["dossier"] != "None":
                a.set_folder_path(d["dossier"])
            if "maillage" in d and isinstance(a, unreal.StaticMeshActor):
                c = a.static_mesh_component
                c.set_static_mesh(unreal.load_asset(d["maillage"]))
                for i, m in enumerate(d.get("materiaux", [])):
                    if m:
                        c.set_material(i, unreal.load_asset(m))
            remises += 1
            log("  remise : " + d["nom"])
        except Exception:
            log("  echec pour %s :\n%s" % (d.get("nom"), traceback.format_exc()))
    log("%d voiture(s) remise(s)." % remises)
    if ell.save_current_level():
        log("Rachid enregistree.")
        os.replace(LISTE, LISTE + ".remis_" + time.strftime("%Y-%m-%d_%Hh%M"))
    log("TERMINE.")


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
