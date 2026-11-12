# -*- coding: utf-8 -*-
"""YEUX ROUGES LUMINEUX des zombies (plugin ChellesZombies).

Cree UNE SEULE FOIS le materiau /Game/Chelles/Zombies/M_YeuxZombies :
un rouge qui brille dans le noir (materiau "Unlit" + emissive).

A lancer DANS L'EDITEUR : File > Execute Python Script... > Tools/Zombies/creer_yeux_zombies.py
ou double-clic sur CREER_YEUX_ZOMBIES.bat (Unreal FERME).

Sans ce materiau, le plugin met quand meme des yeux rouges, mais ils ne brillent pas.
Aucun autre fichier du projet n'est touche. Relancer le script = le materiau est refait.
Journal : Tools/Zombies/yeux.log
"""
import os
import traceback

import unreal

DOSSIER = "/Game/Chelles/Zombies"
NOM = "M_YeuxZombies"
COULEUR = unreal.LinearColor(1.0, 0.03, 0.0, 1.0)  # rouge
INTENSITE = 60.0  # plus haut = brille plus fort

ICI = os.path.dirname(os.path.abspath(__file__))
JOURNAL = os.path.join(ICI, "yeux.log")


def ecrire(texte):
    unreal.log("[YeuxZombies] " + texte)
    with open(JOURNAL, "a", encoding="utf-8") as f:
        f.write(texte + "\n")


def creer():
    chemin = DOSSIER + "/" + NOM
    if unreal.EditorAssetLibrary.does_asset_exist(chemin):
        unreal.EditorAssetLibrary.delete_asset(chemin)
        ecrire("Ancien materiau supprime, on le refait.")
    outils = unreal.AssetToolsHelpers.get_asset_tools()
    mat = outils.create_asset(NOM, DOSSIER, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        raise RuntimeError("Impossible de creer " + chemin)

    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mel = unreal.MaterialEditingLibrary

    couleur = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 0)
    couleur.set_editor_property("parameter_name", "Couleur")
    couleur.set_editor_property("default_value", COULEUR)

    force = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 250)
    force.set_editor_property("parameter_name", "Intensite")
    force.set_editor_property("default_value", INTENSITE)

    fois = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 100)
    mel.connect_material_expressions(couleur, "", fois, "A")
    mel.connect_material_expressions(force, "", fois, "B")
    mel.connect_material_property(fois, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(chemin, only_if_is_dirty=False)
    ecrire("OK : " + chemin + " cree (rouge lumineux, intensite %.0f)." % INTENSITE)


def principal():
    with open(JOURNAL, "w", encoding="utf-8") as f:
        f.write("Yeux rouges des zombies - CHELLES 3D\n")
    try:
        creer()
        ecrire("Termine. Relancez le jeu : les zombies ont les yeux rouges lumineux.")
    except Exception:
        ecrire("ERREUR :\n" + traceback.format_exc())
    if os.environ.get("CHELLES_QUITTER") == "1":
        unreal.SystemLibrary.quit_editor()


principal()
