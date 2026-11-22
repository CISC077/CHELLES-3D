# -*- coding: utf-8 -*-
"""CHELLES 3D - "Blueprint Compilation Errors" au Play (22/11/2026)
BP_CharacterBase (x2), BP_GameState, BPC_Weapon_Sword, AnimBP_TPS_CharacterBase, W_Replicated_HUD.

Cause (Saved/Logs/FPS_Chelles.log) : le 21/11 vers 21h16, des objets du dossier
FPS_Game/Blueprints/PickUps ont ete poses dans Rachid, puis la carte a ete enregistree (21h22).
Ce dossier est une VIEILLE COPIE du pack (son BP_CharacterBase, son BP_PlayerController...) qui ne
compile jamais. Des qu'un de ses objets est dans la carte, Unreal charge toute la vieille copie
et affiche ces erreurs. Le jeu, lui, utilise les vrais Blueprints (FPS_Game/Blueprints/...).

A lancer DANS L'EDITEUR (carte Rachid ouverte) :
    File > Execute Python Script... > Tools/ErreurBlueprint/retirer_pickups.py

Ce que fait le script :
  1. copie Rachid.umap dans Tools/ErreurBlueprint/sauvegarde/ ;
  2. enleve de Rachid les acteurs venant de FPS_Game/Blueprints/PickUps, .../System,
     .../Bonus/GameModifiers et .../Bonus/Shop (liste dans erreur_blueprint.log) ;
     les bonus poses par Tools/Bonus (Bonus/Replication) et les armes du jeu ne sont PAS touches ;
  3. enregistre Rachid.
Ensuite : fermer Unreal (NE PAS enregistrer les blueprints FPS_Game s'il le propose) et le rouvrir.
Pour annuler : Unreal ferme, remettre la copie de sauvegarde a la place de Content/Maps/Rachid.umap.
"""
import os
import shutil
import time
import traceback

import unreal

CARTE = "/Game/Maps/Rachid"
DOSSIERS = (
    "/Game/FPS_Game/Blueprints/PickUps/",
    "/Game/FPS_Game/Blueprints/System/",
    "/Game/FPS_Game/Blueprints/Bonus/GameModifiers/",
    "/Game/FPS_Game/Blueprints/Bonus/Shop/",
)

ell = unreal.EditorLevelLibrary
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ICI = os.path.join(PROJET, "Tools", "ErreurBlueprint")
JOURNAL = os.path.join(ICI, "erreur_blueprint.log")


def log(msg):
    unreal.log("[ErreurBlueprint] " + str(msg))
    try:
        os.makedirs(ICI, exist_ok=True)
        with open(JOURNAL, "a", encoding="utf-8") as f:
            f.write(time.strftime("%Y-%m-%d %H:%M:%S ") + str(msg) + "\n")
    except Exception:
        pass


def main():
    log("---- RETRAIT des objets PickUps / System / GameModifiers / Shop ----")
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        ell.load_level(CARTE)

    src = os.path.join(PROJET, "Content", "Maps", "Rachid.umap")
    dossier = os.path.join(ICI, "sauvegarde")
    os.makedirs(dossier, exist_ok=True)
    copie = os.path.join(dossier, "Rachid_avant_pickups_" + time.strftime("%Y-%m-%d_%H%M") + ".umap")
    shutil.copy2(src, copie)
    log("1/3 copie de la carte : " + copie)

    enleves = []
    for a in ell.get_all_level_actors():
        try:
            chemin = a.get_class().get_path_name()
        except Exception:
            continue
        if any(chemin.startswith(d) for d in DOSSIERS):
            p = a.get_actor_location()
            enleves.append("%s (%s) a X=%.0f Y=%.0f Z=%.0f" % (a.get_actor_label(), chemin, p.x, p.y, p.z))
            ell.destroy_actor(a)
    log("2/3 %d acteur(s) enleve(s) :" % len(enleves))
    for e in enleves:
        log("     - " + e)

    if ell.save_current_level():
        log("3/3 Rachid enregistree.")
    else:
        log("3/3 ATTENTION : Rachid n'a pas pu etre enregistree (faire Ctrl+S a la main).")
    log("TERMINE. Fermez Unreal (sans enregistrer les blueprints FPS_Game) puis rouvrez-le.")
    unreal.EditorDialog.show_message(
        "Erreurs Blueprint",
        "%d objet(s) enleve(s) de Rachid (vieille copie PickUps / System du pack).\n"
        "Liste : Tools/ErreurBlueprint/erreur_blueprint.log\n"
        "Copie de la carte : Tools/ErreurBlueprint/sauvegarde\n\n"
        "Fermez Unreal (repondez NE PAS ENREGISTRER pour les blueprints FPS_Game) puis rouvrez-le."
        % len(enleves),
        unreal.AppMsgType.OK)


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())
