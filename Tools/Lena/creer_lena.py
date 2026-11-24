# -*- coding: utf-8 -*-
# CHELLES 3D - LENA jouable (2026-11-24)
#
# A lancer UNE FOIS dans Unreal : menu File > Execute Python Script... > Tools\Lena\creer_lena.py
#
# Ce que fait le script (il ne touche a rien d'autre) :
#  1. importe LENA AVEC SON SQUELETTE (fichier Mixamo  Bureau\LENA3D\Animations\base_basic_shaded.fbx)
#     -> /Game/Personnages/LENA/Joueur/SK_Lena  (+ SK_Lena_Skeleton, SK_Lena_PhysicsAsset)
#  2. lui met ses couleurs (/Game/Personnages/LENA/shaded_Mat, deja dans le projet)
#  3. importe ses animations Mixamo -> /Game/Personnages/LENA/Joueur/Anims/A_Lena_...
#  4. regle sa taille dans Plugins\ChellesWraith\Wraith.txt (ligne Heros=Lena)
#
# Les modeles deja importes dans /Game/Personnages/LENA (base, base_basic_pbr...) ne sont pas modifies.

import os
import re
import unreal

DOSSIER_FBX = r"C:\Users\CISCO\Desktop\LENA3D\Animations"
DESTINATION = "/Game/Personnages/LENA/Joueur"
DEST_ANIMS = DESTINATION + "/Anims"
MATERIAU = "/Game/Personnages/LENA/shaded_Mat"
TAILLE_VOULUE = 175.0  # cm (le soldat fait environ 180 cm)

# fichier Mixamo -> nom de l'animation dans Unreal
ANIMATIONS = [
    ("Walking.fbx", "A_Lena_Marche"),
    ("Fast Run.fbx", "A_Lena_Course"),
    ("Flying.fbx", "A_Lena_Vol"),
    ("Crouching.fbx", "A_Lena_Accroupi"),
    ("Hip Hop Dancing.fbx", "A_Lena_Danse"),
    ("Rumba Dancing.fbx", "A_Lena_Rumba"),
    ("Twist Dance.fbx", "A_Lena_Twist"),
]


def dire(texte):
    unreal.log("[Lena] " + texte)


def importer(fichier, destination, nom, options):
    tache = unreal.AssetImportTask()
    tache.set_editor_property("filename", fichier)
    tache.set_editor_property("destination_path", destination)
    tache.set_editor_property("destination_name", nom)
    tache.set_editor_property("automated", True)
    tache.set_editor_property("replace_existing", True)
    tache.set_editor_property("save", True)
    tache.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([tache])
    return list(tache.get_editor_property("imported_object_paths") or [])


def trouver_fbx(nom):
    chemins = [
        os.path.join(DOSSIER_FBX, nom),
        os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", "Lena", "FBX", nom),
    ]
    for c in chemins:
        if os.path.isfile(c):
            return c
    return None


# ---------------------------------------------------------------- 1. le corps avec son squelette
fbx_corps = trouver_fbx("base_basic_shaded.fbx")
if not fbx_corps:
    raise RuntimeError("[Lena] Fichier introuvable : " + os.path.join(DOSSIER_FBX, "base_basic_shaded.fbx")
                       + "  (c'est le modele Mixamo avec squelette).")

ui = unreal.FbxImportUI()
ui.set_editor_property("import_mesh", True)
ui.set_editor_property("import_as_skeletal", True)
ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
ui.set_editor_property("import_animations", False)
ui.set_editor_property("import_materials", False)
ui.set_editor_property("import_textures", False)
ui.set_editor_property("create_physics_asset", True)
ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
dire("Import du corps : " + fbx_corps)
importer(fbx_corps, DESTINATION, "SK_Lena", ui)

corps = unreal.EditorAssetLibrary.load_asset(DESTINATION + "/SK_Lena")
if not corps or not isinstance(corps, unreal.SkeletalMesh):
    raise RuntimeError("[Lena] L'import du corps a echoue (voir Output Log).")
squelette = corps.get_editor_property("skeleton")
dire("Corps OK, squelette : " + squelette.get_path_name())

# ---------------------------------------------------------------- 2. couleurs
materiau = unreal.EditorAssetLibrary.load_asset(MATERIAU)
if materiau:
    mats = corps.get_editor_property("materials")
    for i in range(len(mats)):
        m = mats[i]
        m.set_editor_property("material_interface", materiau)
        mats[i] = m  # en Python UE, mats[i] est une COPIE : il faut la remettre
    corps.set_editor_property("materials", mats)
    unreal.EditorAssetLibrary.save_loaded_asset(corps)
    dire("Couleurs posees : " + MATERIAU)
else:
    unreal.log_warning("[Lena] " + MATERIAU + " introuvable : LENA restera grise.")

# ---------------------------------------------------------------- 3. animations
faites = 0
for fichier, nom in ANIMATIONS:
    chemin = trouver_fbx(fichier)
    if not chemin:
        unreal.log_warning("[Lena] Animation absente : " + fichier)
        continue
    ua = unreal.FbxImportUI()
    ua.set_editor_property("import_mesh", False)
    ua.set_editor_property("import_as_skeletal", True)
    ua.set_editor_property("import_animations", True)
    ua.set_editor_property("import_materials", False)
    ua.set_editor_property("import_textures", False)
    ua.set_editor_property("create_physics_asset", False)
    ua.set_editor_property("skeleton", squelette)
    ua.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    ua.anim_sequence_import_data.set_editor_property(
        "animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    importer(chemin, DEST_ANIMS, nom, ua)
    voulu = DEST_ANIMS + "/" + nom
    if not unreal.EditorAssetLibrary.does_asset_exist(voulu):
        # Unreal a pu nommer l'animation autrement (ex. nom_mixamo_com) : on la renomme
        for a in unreal.EditorAssetLibrary.list_assets(DEST_ANIMS, recursive=False):
            court = a.split("/")[-1].split(".")[0]
            if court.startswith(nom) and court != nom:
                unreal.EditorAssetLibrary.rename_asset(a.split(".")[0], voulu)
                break
    if unreal.EditorAssetLibrary.does_asset_exist(voulu):
        faites += 1
        dire("Animation OK : " + voulu)
    else:
        unreal.log_warning("[Lena] Animation non creee : " + fichier)
dire("%d animations sur %d." % (faites, len(ANIMATIONS)))

# ---------------------------------------------------------------- 4. taille dans Wraith.txt
try:
    hauteur = corps.get_bounds().box_extent.z * 2.0
except Exception:
    hauteur = 0.0
if hauteur > 10.0:
    taille = round(max(0.2, min(5.0, TAILLE_VOULUE / hauteur)), 3)
    dire("LENA mesure %.0f cm -> taille %.3f pour faire %.0f cm." % (hauteur, taille, TAILLE_VOULUE))
    reglages = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_plugins_dir()),
                            "ChellesWraith", "Wraith.txt")
    if os.path.isfile(reglages):
        with open(reglages, "r", encoding="latin-1", newline="") as f:
            texte = f.read()
        lignes = texte.split("\n")
        change = False
        for i, ligne in enumerate(lignes):
            if ligne.startswith("Heros=Lena|"):
                fin = "\r" if ligne.endswith("\r") else ""
                morceaux = ligne.rstrip("\r").split("|")
                while len(morceaux) < 11:
                    morceaux.append("")
                morceaux[9] = str(taille)
                lignes[i] = "|".join(morceaux) + fin
                change = True
        if change:
            with open(reglages, "w", encoding="latin-1", newline="") as f:
                f.write("\n".join(lignes))
            dire("Wraith.txt : taille de LENA = " + str(taille))
        else:
            unreal.log_warning("[Lena] Pas de ligne Heros=Lena dans Wraith.txt : copiez d'abord le nouveau Wraith.txt.")
else:
    unreal.log_warning("[Lena] Hauteur de LENA non mesuree : taille laissee dans Wraith.txt.")

dire("TERMINE. Fermez Unreal, compilez ChellesWraith et ChellesMenu, puis relancez.")
try:
    unreal.EditorDialog.show_message("CHELLES 3D - LENA",
                                     "LENA est prete (%d animations).\n\nFermez Unreal, compilez les plugins ChellesWraith et ChellesMenu,\npuis relancez le jeu : JOUER > HABILLAGE > LENA." % faites,
                                     unreal.AppMsgType.OK)
except Exception:
    pass
