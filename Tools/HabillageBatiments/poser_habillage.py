# -*- coding: utf-8 -*-
"""ETAPE 2 - POSE : depose les modeles des packs par-dessus les batiments simples de Rachid,
d'apres Tools/HabillageBatiments/Rapport/correspondances.json (cree par l'etape 1).

Lancer : double-clic sur 2_POSER.bat (Unreal FERME)
     ou, Unreal ouvert sur Rachid : File > Execute Python Script... > Tools/HabillageBatiments/poser_habillage.py

Ce que fait le script :
  1. enregistre Rachid et en copie le fichier dans Tools/HabillageBatiments/sauvegarde/ ;
  2. enleve les modeles poses par un passage precedent de ce script (etiquette ChellesHabillage) ;
  3. pose chaque modele (dossier "Habillage" de l'Outliner), un peu plus grand que le volume simple
     pour bien le cacher, sans collision (on garde celle du batiment simple) ;
  4. enregistre Rachid.
Les batiments simples, les packs et tout le reste ne sont PAS modifies.
Pour annuler : 3_ANNULER.bat (ou retirer_habillage.py).
"""
import json
import os
import shutil
import time
import traceback

import unreal

CARTE = "/Game/Maps/Rachid"
TAG_AJOUTE = "ChellesHabillage"
DOSSIER_OUTLINER = "Habillage"
# Au-dela de cette distance (metres), le modele detaille n'est plus dessine et on voit le batiment simple
# a sa place : la ville reste fluide. 0 = toujours dessine.
DISTANCE_AFFICHAGE_M = 350

ell = unreal.EditorLevelLibrary
eal = unreal.EditorAssetLibrary
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ICI = os.path.join(PROJET, "Tools", "HabillageBatiments")
JOURNAL = os.path.join(ICI, "habillage.log")
CORRESP = os.path.join(ICI, "Rapport", "correspondances.json")


def log(msg):
    unreal.log("[Habillage] " + str(msg))
    try:
        with open(JOURNAL, "a", encoding="utf-8") as f:
            f.write(time.strftime("%H:%M:%S ") + str(msg) + "\n")
    except Exception:
        pass


def a_tag(a, tag):
    try:
        return any(str(t) == tag for t in a.get_editor_property("tags"))
    except Exception:
        return False


def demander(texte):
    if os.environ.get("CHELLES_QUITTER") == "1":
        return True
    r = unreal.EditorDialog.show_message("Chelles 3D - Habillage des batiments", texte, unreal.AppMsgType.YES_NO)
    return r == unreal.AppReturnType.YES


def main():
    log("---- POSE ----")
    if not os.path.exists(CORRESP):
        log("ABANDON : lance d'abord 1_ANALYSER.bat (correspondances.json introuvable).")
        return
    poses = json.load(open(CORRESP, encoding="utf-8"))["poses"]
    if not poses:
        log("ABANDON : aucune correspondance dans correspondances.json.")
        return

    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        log("chargement de %s ..." % CARTE)
        ell.load_level(CARTE)
    anciens = [a for a in ell.get_all_level_actors() if a_tag(a, TAG_AJOUTE)]
    if not demander("%d modeles de maisons / immeubles vont etre poses sur les batiments simples de Rachid.\n%s\n"
                    "Une copie de Rachid est faite avant. Continuer ?"
                    % (len(poses), ("Les %d modeles du passage precedent seront remplaces.\n" % len(anciens)) if anciens else "")):
        log("Annule. Rien n'a ete modifie.")
        return

    # 1. sauvegarde
    ell.save_current_level()
    fichier = os.path.join(PROJET, "Content", "Maps", "Rachid.umap")
    dossier = os.path.join(ICI, "sauvegarde")
    os.makedirs(dossier, exist_ok=True)
    copie = os.path.join(dossier, "Rachid_%s.umap" % time.strftime("%Y-%m-%d_%H%M"))
    shutil.copy2(fichier, copie)
    log("1/4 sauvegarde : %s" % copie)

    # 2. anciens
    for a in anciens:
        ell.destroy_actor(a)
    log("2/4 %d modeles d'un passage precedent enleves" % len(anciens))

    # 3. pose
    meshes, manquants, n = {}, set(), 0
    for i, p in enumerate(poses):
        ch = p["modele"]
        if ch not in meshes:
            meshes[ch] = eal.load_asset(ch) if eal.does_asset_exist(ch) else None
        m = meshes[ch]
        if not isinstance(m, unreal.StaticMesh):
            manquants.add(ch)
            continue
        x, y, z = p["position"]
        a = ell.spawn_actor_from_object(m, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=p["yaw"]))
        if a is None:
            continue
        a.set_actor_scale3d(unreal.Vector(*p["echelle"]))
        a.set_actor_label("Habillage_%04d_%s" % (i, m.get_name()))
        a.set_folder_path("%s/%s" % (DOSSIER_OUTLINER, p.get("pack", "Pack")))
        tags = list(a.get_editor_property("tags"))
        tags.append(unreal.Name(TAG_AJOUTE))
        a.set_editor_property("tags", tags)
        c = a.static_mesh_component
        try:
            c.set_mobility(unreal.ComponentMobility.MOVABLE)  # comme les tuiles : pas de lightmap a calculer
        except Exception:
            pass
        try:
            c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            c.set_editor_property("can_ever_affect_navigation", False)
        except Exception:
            pass
        if DISTANCE_AFFICHAGE_M > 0:
            try:
                c.set_editor_property("ld_max_draw_distance", DISTANCE_AFFICHAGE_M * 100.0)
                c.set_cull_distance(DISTANCE_AFFICHAGE_M * 100.0)
            except Exception:
                pass
        n += 1
    for ch in sorted(manquants):
        log("   modele introuvable (pack deplace ?) : %s" % ch)
    log("3/4 %d modeles poses" % n)

    # 4. enregistrement
    if ell.save_current_level():
        log("4/4 Rachid enregistree.")
    else:
        log("4/4 ATTENTION : Rachid n'a pas pu etre enregistree (deux Unreal ouverts ?).")
    log("TERMINE.")


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
