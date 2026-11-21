# -*- coding: utf-8 -*-
"""RETIRE les voitures de police posees dans la carte /Game/Maps/Rachid (CHELLES 3D).

A lancer Unreal FERME : double-clic sur RETIRER_VOITURES_POLICE.bat
ou DANS L'EDITEUR : File > Execute Python Script... > Tools/RetirerPolice/retirer_voitures_police.py

Ce qui est retire :
  - les Blueprints de police du pack "Sports and Police Cars" (SportVehiclesPack, nom avec "Police")
  - les modeles de voiture de police poses en decor (maillage PoliceCar..., ou pack SportVehiclesPack/Meshes/Police...)
Ce qui n'est PAS touche : le commissariat (Police_Department), les panneaux, les agents de police (personnages),
les voitures de sport normales, et le pack lui-meme (rien n'est efface du dossier Content).

Rien de definitif :
  - une copie de Rachid.umap est faite AVANT dans Tools/RetirerPolice/Sauvegarde/
  - la liste des voitures retirees (place, rotation, taille, modele) est ecrite dans voitures_police_retirees.json
  - REMETTRE_VOITURES_POLICE.bat les remet exactement a leur place.
Journal : Tools/RetirerPolice/police.log
"""
import json
import os
import shutil
import time
import traceback

import unreal

CARTE = "/Game/Maps/Rachid"
ell = unreal.EditorLevelLibrary
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ICI = os.path.join(PROJET, "Tools", "RetirerPolice")
JOURNAL = os.path.join(ICI, "police.log")
LISTE = os.path.join(ICI, "voitures_police_retirees.json")

# Morceaux de chemin qui designent une VOITURE de police
VOITURE = ["policecar", "/sportvehiclespack/meshes/police"]
# Morceaux de chemin a ne jamais toucher (batiments, panneaux, personnages)
JAMAIS = ["police_department", "signboard", "inscription", "officer", "/personnages/", "policier", "agent"]


def log(msg):
    unreal.log("[RetirerPolice] " + str(msg))
    try:
        os.makedirs(ICI, exist_ok=True)
        with open(JOURNAL, "a", encoding="utf-8") as f:
            f.write(time.strftime("%H:%M:%S ") + str(msg) + "\n")
    except Exception:
        pass


def chemin(obj):
    try:
        return obj.get_path_name() if obj else ""
    except Exception:
        return ""


def maillages(acteur):
    res = []
    for classe, prop in ((unreal.StaticMeshComponent, "static_mesh"), (unreal.SkeletalMeshComponent, "skeletal_mesh")):
        try:
            for c in acteur.get_components_by_class(classe):
                res.append(chemin(c.get_editor_property(prop)))
        except Exception:
            pass
    return [m for m in res if m]


def est_voiture_police(acteur):
    classe = chemin(acteur.get_class())
    cl = classe.lower()
    if any(j in cl for j in JAMAIS):
        return False, ""
    # 1) Blueprint de police du pack Sports and Police Cars
    if "/sportvehiclespack/" in cl and "police" in cl:
        return True, "Blueprint " + classe
    # 2) decor : modele de voiture de police
    for m in maillages(acteur):
        ml = m.lower()
        if any(j in ml for j in JAMAIS):
            continue
        if any(v in ml for v in VOITURE):
            return True, "modele " + m
    return False, ""


def decrire(acteur, raison):
    t = acteur.get_actor_transform()
    d = {
        "nom": acteur.get_actor_label(),
        "raison": raison,
        "classe": chemin(acteur.get_class()),
        "position": [t.translation.x, t.translation.y, t.translation.z],
        "rotation": [t.rotation.rotator().roll, t.rotation.rotator().pitch, t.rotation.rotator().yaw],
        "taille": [t.scale3d.x, t.scale3d.y, t.scale3d.z],
        "dossier": str(acteur.get_folder_path()),
    }
    if isinstance(acteur, unreal.StaticMeshActor):
        c = acteur.static_mesh_component
        d["maillage"] = chemin(c.get_editor_property("static_mesh"))
        d["materiaux"] = [chemin(m) for m in (c.get_editor_property("override_materials") or [])]
    return d


def sauvegarder_carte():
    src = os.path.join(PROJET, "Content", "Maps", "Rachid.umap")
    dossier = os.path.join(ICI, "Sauvegarde")
    os.makedirs(dossier, exist_ok=True)
    dst = os.path.join(dossier, "Rachid_avant_retrait_police_%s.umap" % time.strftime("%Y-%m-%d_%Hh%M"))
    shutil.copy2(src, dst)
    log("Copie de la carte : " + dst)


def main():
    log("==== RETRAIT DES VOITURES DE POLICE ====")
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        ell.load_level(CARTE)
    trouvees = []
    for a in ell.get_all_level_actors():
        try:
            ok, raison = est_voiture_police(a)
        except Exception:
            continue
        if ok:
            trouvees.append((a, raison))
    if not trouvees:
        log("Aucune voiture de police trouvee dans Rachid : rien a faire.")
        return
    sauvegarder_carte()
    anciennes = []
    if os.path.exists(LISTE):
        try:
            anciennes = json.load(open(LISTE, encoding="utf-8"))
        except Exception:
            anciennes = []
    nouvelles = []
    for a, raison in trouvees:
        d = decrire(a, raison)
        nouvelles.append(d)
        log("  retiree : %s (%s)" % (d["nom"], raison))
    with open(LISTE, "w", encoding="utf-8") as f:
        json.dump(anciennes + nouvelles, f, ensure_ascii=False, indent=1)
    for a, _ in trouvees:
        ell.destroy_actor(a)
    log("%d voiture(s) de police retiree(s). Liste : %s" % (len(trouvees), LISTE))
    if ell.save_current_level():
        log("Rachid enregistree.")
    else:
        log("ATTENTION : Rachid n'a pas pu etre enregistree (un autre Unreal est ouvert ?).")
    log("TERMINE.")


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
