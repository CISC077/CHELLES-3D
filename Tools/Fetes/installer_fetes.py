# -*- coding: utf-8 -*-
"""INSTALLATION DES FETES DU CALENDRIER - lance par INSTALLER_FETES.bat (Unreal ferme).

Ce que fait ce script (la carte Rachid n'est PAS modifiee) :
  1. cree /Game/Chelles/Fetes/M_Fetes_Lumiere : materiau qui brille (etincelles du feu d'artifice,
     visages des citrouilles, ecran de la scene). Parametres : Couleur, Force ;
  2. cree /Game/Chelles/Fetes/M_Fetes_Fantome : materiau transparent des fantomes. Parametres : Couleur, Force, Opacite ;
  3. ajoute dans Config/DefaultGame.ini les dossiers a emporter dans le jeu complet (packaging) :
     /Game/Chelles/Fetes, les danseurs ActorCore et les musiques /Game/WAV
     (copie de l'ancien fichier : Config/DefaultGame.ini.avant_fetes).

Journal : Tools/Fetes/installer_fetes.log
"""
import os
import shutil
import traceback

import unreal

DOSSIER = "/Game/Chelles/Fetes"
LUMIERE = DOSSIER + "/M_Fetes_Lumiere"
FANTOME = DOSSIER + "/M_Fetes_Fantome"
A_EMPORTER = [
    "/Game/Chelles/Fetes",
    "/Game/ActorCore_Sample_Motions/Dummy_Female",
    "/Game/ActorCore_Sample_Motions/Pose_dance_catwalk_Female",
    "/Game/ActorCore_Sample_Motions/Shaders",
    "/Game/WAV",
]

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
PROJET = unreal.SystemLibrary.get_project_directory()
JOURNAL = os.path.join(PROJET, "Tools", "Fetes", "installer_fetes.log")
open(JOURNAL, "w").write("")


def ecrire(texte):
    print("[Fetes] " + texte)
    with open(JOURNAL, "a") as f:
        f.write(texte + "\n")


def nouveau_materiau(chemin):
    """Cree le materiau (ou le vide s'il existe deja, pour repartir propre)."""
    if eal.does_asset_exist(chemin):
        mat = eal.load_asset(chemin)
        mel.delete_all_material_expressions(mat)
        return mat
    nom = chemin.rsplit("/", 1)[1]
    outils = unreal.AssetToolsHelpers.get_asset_tools()
    return outils.create_asset(nom, DOSSIER, unreal.Material, unreal.MaterialFactoryNew())


def parametre_couleur(mat, nom, valeur, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", nom)
    e.set_editor_property("default_value", valeur)
    return e


def parametre_nombre(mat, nom, valeur, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", nom)
    e.set_editor_property("default_value", valeur)
    return e


def materiau_lumiere():
    mat = nouveau_materiau(LUMIERE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    for usage in ("used_with_instanced_static_meshes", "used_with_skeletal_mesh"):
        try:
            mat.set_editor_property(usage, True)
        except Exception:
            pass
    couleur = parametre_couleur(mat, "Couleur", unreal.LinearColor(1.0, 0.5, 0.1, 1.0), -600, 0)
    force = parametre_nombre(mat, "Force", 10.0, -600, 200)
    fois = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 100)
    mel.connect_material_expressions(couleur, "", fois, "A")
    mel.connect_material_expressions(force, "", fois, "B")
    mel.connect_material_property(fois, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_asset(LUMIERE, only_if_is_dirty=False)
    ecrire("OK " + LUMIERE)


def materiau_fantome():
    mat = nouveau_materiau(FANTOME)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("two_sided", True)
    couleur = parametre_couleur(mat, "Couleur", unreal.LinearColor(0.75, 0.9, 1.0, 1.0), -700, 0)
    force = parametre_nombre(mat, "Force", 1.5, -700, 200)
    opacite = parametre_nombre(mat, "Opacite", 0.55, -700, 350)
    fois = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 100)
    mel.connect_material_expressions(couleur, "", fois, "A")
    mel.connect_material_expressions(force, "", fois, "B")
    # bord plus lumineux (effet drap qui brille)
    fresnel = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -700, 500)
    plus = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -200, 150)
    mel.connect_material_expressions(fois, "", plus, "A")
    mel.connect_material_expressions(fresnel, "", plus, "B")
    mel.connect_material_property(plus, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacite, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(mat)
    eal.save_asset(FANTOME, only_if_is_dirty=False)
    ecrire("OK " + FANTOME)


def dossiers_a_emporter():
    ini = os.path.join(PROJET, "Config", "DefaultGame.ini")
    if not os.path.isfile(ini):
        ecrire("ATTENTION : Config/DefaultGame.ini introuvable, packaging non regle.")
        return
    lignes = open(ini, "r", encoding="utf-8-sig", errors="replace").read().splitlines()
    section = "[/Script/UnrealEd.ProjectPackagingSettings]"
    if section not in [l.strip() for l in lignes]:
        lignes += ["", section]
    debut = [l.strip() for l in lignes].index(section)
    fin = len(lignes)
    for i in range(debut + 1, len(lignes)):
        if lignes[i].strip().startswith("["):
            fin = i
            break
    deja = "\n".join(lignes[debut:fin])
    ajouts = []
    for d in A_EMPORTER:
        if '(Path="%s")' % d not in deja:
            ajouts.append('+DirectoriesToAlwaysCook=(Path="%s")' % d)
    if not ajouts:
        ecrire("Packaging : dossiers deja presents dans DefaultGame.ini.")
        return
    sauvegarde = ini + ".avant_fetes"
    if not os.path.isfile(sauvegarde):
        shutil.copy2(ini, sauvegarde)
    # on ajoute apres la derniere ligne non vide de la section
    position = fin
    while position > debut + 1 and not lignes[position - 1].strip():
        position -= 1
    lignes[position:position] = ajouts
    with open(ini, "w", encoding="utf-8", newline="\r\n") as f:
        f.write("\n".join(lignes) + "\n")
    for a in ajouts:
        ecrire("Packaging : " + a)


try:
    if not eal.does_directory_exist(DOSSIER):
        eal.make_directory(DOSSIER)
    materiau_lumiere()
    materiau_fantome()
    dossiers_a_emporter()
    ecrire("")
    ecrire("TERMINE. Ouvrez CHELLES 3D et faites Play.")
    ecrire("Pour essayer sans attendre la date : console (touche ²) puis")
    ecrire("   Chelles.Fetes Halloween   ou   Chelles.Fetes Juillet   ou   Chelles.Fetes Musique")
except Exception:
    ecrire("ERREUR :")
    ecrire(traceback.format_exc())
