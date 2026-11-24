# -*- coding: utf-8 -*-
"""CHELLES 3D - "Blueprint Compilation Errors" au Play (24/11/2026)
BP_CharacterBase, BP_GameState, BPC_Weapon_Sword, AnimBP_TPS_CharacterBase, W_Replicated_HUD.

Cause (Saved/Logs/FPS_Chelles.log + lecture des .uasset) : une ANCIENNE epee
FPS_Game/Blueprints/Weapons/BPC_Weapon_Sword (avec un "_", fichier d'oct. 2023 venu du vieux
pack TPS_Game) est posee dans la carte Rachid. C'est la SEULE arme du dossier Weapons qui pointe
vers la vieille copie System/ du pack (System/BP_CharacterBase, System/BP_PlayerController).
Des que Rachid s'ouvre, Unreal charge donc toute cette vieille copie (BP_GameState, AnimBP,
W_Replicated_HUD...) qui ne compile jamais -> la fenetre d'erreurs au Play.
Le jeu, lui, utilise la bonne epee BPC_WeaponSword (sans "_") : roue TAB, ARMEMENTS, Refuge.

A lancer DANS L'EDITEUR (carte Rachid ouverte) :
    File > Execute Python Script... > Tools/ErreurBlueprint/retirer_ancienne_epee.py

Ce que fait le script :
  1. copie Rachid.umap dans Tools/ErreurBlueprint/sauvegarde/ ;
  2. remplace chaque ancienne epee BPC_Weapon_Sword posee dans Rachid par la bonne
     BPC_WeaponSword, au meme endroit (on garde une epee a ramasser) ;
  3. enregistre Rachid et note tout dans erreur_blueprint.log.
Ensuite : fermer Unreal (NE PAS enregistrer les blueprints FPS_Game s'il le propose) et le rouvrir.
Pour annuler : Unreal ferme, remettre la copie de sauvegarde a la place de Content/Maps/Rachid.umap.
"""
import os
import shutil
import time
import traceback

import unreal

CARTE = "/Game/Maps/Rachid"
ANCIENNE = "/Game/FPS_Game/Blueprints/Weapons/BPC_Weapon_Sword.BPC_Weapon_Sword_C"
BONNE = "/Game/FPS_Game/Blueprints/Weapons/BPC_WeaponSword.BPC_WeaponSword_C"
VIEILLE_COPIE = (
    "/Game/FPS_Game/Blueprints/System/BP_CharacterBase",
    "/Game/FPS_Game/Blueprints/System/BP_GameState",
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


def qui_charge_la_vieille_copie():
    """Note dans le journal qui reference encore la vieille copie (pour un diagnostic plus tard)."""
    try:
        reg = unreal.AssetRegistryHelpers.get_asset_registry()
        opts = unreal.AssetRegistryDependencyOptions(True, True, True, True, True)
        for paquet in VIEILLE_COPIE:
            refs = reg.get_referencers(paquet, opts) or []
            noms = sorted(str(r) for r in refs if not str(r).startswith("/Game/FPS_Game/Blueprints/System/"))
            log("   %s est utilise par : %s" % (paquet, ", ".join(noms) if noms else "(rien hors System)"))
    except Exception as e:
        log("   (diagnostic impossible : %s)" % e)


def main():
    log("---- REMPLACEMENT de l'ancienne epee BPC_Weapon_Sword ----")
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        ell.load_level(CARTE)

    src = os.path.join(PROJET, "Content", "Maps", "Rachid.umap")
    dossier = os.path.join(ICI, "sauvegarde")
    os.makedirs(dossier, exist_ok=True)
    copie = os.path.join(dossier, "Rachid_avant_epee_" + time.strftime("%Y-%m-%d_%H%M") + ".umap")
    shutil.copy2(src, copie)
    log("1/3 copie de la carte : " + copie)

    bonne = unreal.load_class(None, BONNE)
    if bonne is None:
        log("   ATTENTION : bonne epee introuvable (%s), les anciennes seront juste enlevees." % BONNE)

    faits = []
    for a in ell.get_all_level_actors():
        try:
            chemin = a.get_class().get_path_name()
        except Exception:
            continue
        if chemin != ANCIENNE:
            continue
        nom = a.get_actor_label()
        pos = a.get_actor_location()
        rot = a.get_actor_rotation()
        ell.destroy_actor(a)
        texte = "%s enlevee a X=%.0f Y=%.0f Z=%.0f" % (nom, pos.x, pos.y, pos.z)
        if bonne is not None:
            n = ell.spawn_actor_from_class(bonne, pos, rot)
            if n is not None:
                n.set_actor_label(nom + "_Chelles")
                texte += " -> remplacee par BPC_WeaponSword"
        faits.append(texte)
    log("2/3 %d ancienne(s) epee(s) traitee(s) :" % len(faits))
    for t in faits:
        log("     - " + t)

    if faits:
        if ell.save_current_level():
            log("3/3 Rachid enregistree.")
        else:
            log("3/3 ATTENTION : Rachid n'a pas pu etre enregistree (faire Ctrl+S a la main).")
    else:
        log("3/3 rien a changer dans Rachid (aucune ancienne epee posee).")
    qui_charge_la_vieille_copie()
    log("TERMINE. Fermez Unreal (sans enregistrer les blueprints FPS_Game) puis rouvrez-le.")
    unreal.EditorDialog.show_message(
        "Erreurs Blueprint",
        "%d ancienne(s) epee(s) BPC_Weapon_Sword remplacee(s) par la bonne dans Rachid.\n"
        "Detail : Tools/ErreurBlueprint/erreur_blueprint.log\n"
        "Copie de la carte : Tools/ErreurBlueprint/sauvegarde\n\n"
        "Fermez Unreal (repondez NE PAS ENREGISTRER pour les blueprints FPS_Game) puis rouvrez-le."
        % len(faits),
        unreal.AppMsgType.OK)


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())
