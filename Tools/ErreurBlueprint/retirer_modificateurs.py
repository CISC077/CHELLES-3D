# -*- coding: utf-8 -*-
"""LOAD ERRORS « Failed to load BP_SimplePrototypingZombie / BP_EnemySpawner »
(référencés par BP_GameState et des K2Node_...) + erreurs de compilation de BP_GameState.

Cause (vérifiée le 20/11/2026 dans Saved/Logs/FPS_Chelles.log et dans Rachid.umap) :
des acteurs « GameModifiers » du pack FPS_Game sont posés dans Rachid :
    BP_StartWaves, BP_DestroyEnemies, BP_FriendlyFire
Chacun charge l'ancienne copie du pack : FPS_Game/Blueprints/System/BP_GameState
(+ UI/W_Announcer). Cette vieille copie pointe vers BP_SimplePrototypingZombie et
BP_EnemySpawner, mis en quarantaine le 16/11 (réparation du packaging) parce qu'ils
cassaient l'export. Résultat : 16 Load Errors + BP_GameState qui ne compile pas.
Ces 3 acteurs ne marchent donc pas en jeu de toute façon.

A lancer DANS L'EDITEUR (carte Rachid ouverte) :
    File > Execute Python Script... > Tools/ErreurBlueprint/retirer_modificateurs.py

Ce que fait le script :
  1. copie Rachid.umap dans Tools/ErreurBlueprint/sauvegarde/ ;
  2. enlève de Rachid les acteurs BP_StartWaves / BP_DestroyEnemies / BP_FriendlyFire
     (rien d'autre ; leur nom et leur position sont notés dans erreur_blueprint.log) ;
  3. enregistre Rachid.
Ensuite : fermer Unreal (NE PAS enregistrer les blueprints FPS_Game s'il le propose) et le rouvrir.
Pour annuler : Unreal fermé, remettre la copie de sauvegarde à la place de Content/Maps/Rachid.umap.
"""
import os
import shutil
import time
import traceback

import unreal

CARTE = "/Game/Maps/Rachid"
A_ENLEVER = ("BP_StartWaves", "BP_DestroyEnemies", "BP_FriendlyFire")

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
    log("---- RETRAIT GameModifiers (StartWaves / DestroyEnemies / FriendlyFire) ----")
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        ell.load_level(CARTE)

    src = os.path.join(PROJET, "Content", "Maps", "Rachid.umap")
    dossier = os.path.join(ICI, "sauvegarde")
    os.makedirs(dossier, exist_ok=True)
    copie = os.path.join(dossier, "Rachid_avant_modificateurs_" + time.strftime("%Y-%m-%d_%H%M") + ".umap")
    shutil.copy2(src, copie)
    log("1/3 copie de la carte : " + copie)

    enleves = []
    for a in ell.get_all_level_actors():
        try:
            nom_classe = a.get_class().get_name()
        except Exception:
            continue
        if any(nom_classe.startswith(n) for n in A_ENLEVER):
            p = a.get_actor_location()
            enleves.append("%s (%s) a X=%.0f Y=%.0f Z=%.0f" % (a.get_actor_label(), nom_classe, p.x, p.y, p.z))
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
        "Load Errors zombies",
        "%d acteur(s) enleve(s) de Rachid (StartWaves / DestroyEnemies / FriendlyFire).\n"
        "Copie de la carte : Tools/ErreurBlueprint/sauvegarde\n\n"
        "Fermez Unreal (repondez NE PAS ENREGISTRER pour les blueprints FPS_Game) puis rouvrez-le."
        % len(enleves),
        unreal.AppMsgType.OK)


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())
