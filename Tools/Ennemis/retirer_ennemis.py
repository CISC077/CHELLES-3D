# -*- coding: utf-8 -*-
"""ANNULE disperser_ennemis.py dans la carte /Game/Maps/Rachid.

A lancer DANS L'EDITEUR : File > Execute Python Script... > Tools/Ennemis/retirer_ennemis.py
ou double-clic sur RETIRER_ENNEMIS.bat (Unreal ferme).

- enleve les ennemis ajoutes (etiquette ChellesEnnemiAjoute) ;
- remet les ennemis d'origine a leur place, avec leur apparence d'avant
  (d'apres Tools/Ennemis/etat_avant.json) ;
- enregistre Rachid.
Les tenues creees dans /Game/Bandit_Sport/Material/Chelles restent (elles ne genent pas).
Autre solution : remettre une copie de Tools/Ennemis/sauvegarde/ a la place de Content/Maps/Rachid.umap (Unreal ferme).
"""
import json
import os
import traceback

import unreal

CARTE = "/Game/Maps/Rachid"
TAG_AJOUTE = "ChellesEnnemiAjoute"
TAG_HABILLE = "ChellesEnnemiHabille"

ell = unreal.EditorLevelLibrary
eal = unreal.EditorAssetLibrary
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ICI = os.path.join(PROJET, "Tools", "Ennemis")
JOURNAL = os.path.join(ICI, "ennemis.log")
ETAT = os.path.join(ICI, "etat_avant.json")


def log(msg):
    unreal.log("[Ennemis] " + str(msg))
    with open(JOURNAL, "a", encoding="utf-8") as f:
        f.write(str(msg) + "\n")


def tags(a):
    try:
        return [str(t) for t in a.get_editor_property("tags")]
    except Exception:
        return []


def charger(ch):
    if ch and eal.does_asset_exist(ch.split(".")[0]):
        return eal.load_asset(ch.split(".")[0])
    return None


def corps(a):
    try:
        m = a.get_editor_property("mesh")
        if m:
            return m
    except Exception:
        pass
    c = a.get_components_by_class(unreal.SkeletalMeshComponent)
    return c[0] if c else None


def main():
    log("---- RETRAIT ----")
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        ell.load_level(CARTE)
    etat = {}
    if os.path.exists(ETAT):
        with open(ETAT, encoding="utf-8") as f:
            etat = json.load(f)
    enleves = remis = 0
    for a in ell.get_all_level_actors():
        t = tags(a)
        if TAG_AJOUTE in t:
            ell.destroy_actor(a)
            enleves += 1
            continue
        e = etat.get(a.get_name())
        if not e:
            continue
        x, y, z = e["position"]
        p, yaw, r = e["rotation"]
        a.set_actor_location_and_rotation(unreal.Vector(x, y, z), unreal.Rotator(r, p, yaw), False, True)
        if e.get("dossier") is not None:
            a.set_folder_path(e["dossier"] if e["dossier"] != "None" else "")
        c = corps(a)
        if c and TAG_HABILLE in t:
            m = charger(e.get("mesh"))
            if m:
                c.set_editor_property("skeletal_mesh", m)
            c.set_editor_property("override_materials", [charger(x) for x in e.get("materiaux", [])])
            a.set_editor_property("tags", [unreal.Name(x) for x in t if x != TAG_HABILLE])
        remis += 1
    log("%d ennemis ajoutes enleves, %d ennemis d'origine remis en place." % (enleves, remis))
    if ell.save_current_level():
        log("Rachid enregistree.")
    if os.path.exists(ETAT):
        os.rename(ETAT, ETAT + ".utilise")
    log("TERMINE.")


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
