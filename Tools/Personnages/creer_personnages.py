# -*- coding: utf-8 -*-
# CHELLES 3D - personnages jouables de MERIDJA : LENA, SOFIA... (2026-11-24)
#
# A lancer dans Unreal : menu File > Execute Python Script... > Tools\Personnages\creer_personnages.py
# On peut le relancer sans risque : ce qui est deja fait est garde.
#
# Pour chaque personnage de la liste PERSONNAGES :
#  1. importe le corps AVEC SON SQUELETTE (fichier Mixamo du dossier Animations du Bureau)
#     -> /Game/Personnages/<DOSSIER>/Joueur/SK_<Nom>  (+ squelette, physique)
#  2. lui met ses couleurs
#  3. importe ses animations Mixamo -> /Game/Personnages/<DOSSIER>/Joueur/Anims/A_<Nom>_<Etat>_Anim
#     (et supprime les copies du corps qu'Unreal cree en plus a chaque animation : 64 Mo chacune)
#  4. regle sa taille dans Plugins\ChellesWraith\Wraith.txt (ligne Heros=<Nom>)
#
# Les modeles importes avant dans /Game/Personnages/<DOSSIER> (base, lod...) ne sont pas modifies.
#
# NOUVEAU PERSONNAGE : ajouter un bloc dans PERSONNAGES, une ligne Heros= et Skin= dans Wraith.txt,
# et une ligne dans PersosWraithInfos (Plugins\ChellesMenu\...\HabillageChelles.cpp) pour le bouton du menu.

import os
import unreal

BUREAU = r"C:\Users\CISCO\Desktop"

PERSONNAGES = [
    {
        "nom": "Lena", "dossier": "LENA", "taille_cm": 175.0,
        "fbx": os.path.join(BUREAU, "LENA3D", "Animations"),
        "corps": "base_basic_shaded.fbx",
        "materiau": "/Game/Personnages/LENA/shaded_Mat",  # deja dans le projet
        "anims": [
            ("Walking.fbx", "Marche"),
            ("Fast Run.fbx", "Course"),
            ("Flying.fbx", "Vol"),
            ("Crouching.fbx", "Accroupi"),
            ("Hip Hop Dancing.fbx", "Danse"),
            ("Rumba Dancing.fbx", "Rumba"),
            ("Twist Dance.fbx", "Twist"),
        ],
    },
    {
        "nom": "Sofia", "dossier": "SOFIA", "taille_cm": 170.0,
        "fbx": os.path.join(BUREAU, "SOFIA 3D", "Animations"),
        "corps": "lod_basic_shaded.fbx",
        "materiau": "",  # vide = couleurs prises dans le fichier FBX
        "anims": [
            ("Walking.fbx", "Marche"),
            ("Fast Run.fbx", "Course"),
            ("Flying.fbx", "Vol"),
            ("Crouching.fbx", "Accroupi"),
            ("Jump2.fbx", "Saut"),
            ("Rumba Dancing.fbx", "Danse"),
        ],
    },
]

EAL = unreal.EditorAssetLibrary
resultats = []


def dire(texte):
    unreal.log("[Personnages] " + texte)


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


def nom_court(chemin):
    return chemin.split("/")[-1].split(".")[0]


def classe(chemin):
    donnees = EAL.find_asset_data(chemin)
    try:
        return str(donnees.asset_class)
    except Exception:
        return ""


def ranger_animations(dossier_anims, nom_corps):
    """Unreal cree un corps en plus a chaque animation importee (A_X_Marche = corps, A_X_Marche_Anim = animation).
    On supprime ces corps en trop (seulement des SkeletalMesh / PhysicsAsset / Skeleton du dossier Anims)."""
    supprimes = 0
    for chemin in EAL.list_assets(dossier_anims, recursive=False):
        chemin = chemin.split(".")[0]
        c = classe(chemin)
        if c in ("SkeletalMesh", "PhysicsAsset", "Skeleton"):
            if EAL.delete_asset(chemin):
                supprimes += 1
    if supprimes:
        dire("%d copies du corps en trop supprimees dans %s" % (supprimes, dossier_anims))


def regler_taille(nom, taille):
    reglages = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_plugins_dir()),
                            "ChellesWraith", "Wraith.txt")
    if not os.path.isfile(reglages):
        return
    with open(reglages, "r", encoding="latin-1", newline="") as f:
        lignes = f.read().split("\n")
    change = False
    for i, ligne in enumerate(lignes):
        if ligne.startswith("Heros=" + nom + "|"):
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
        dire("Wraith.txt : taille de %s = %s" % (nom, taille))
    else:
        unreal.log_warning("[Personnages] Pas de ligne Heros=%s dans Wraith.txt." % nom)


def creer(p):
    nom, dossier = p["nom"], p["dossier"]
    destination = "/Game/Personnages/%s/Joueur" % dossier
    dossier_anims = destination + "/Anims"
    chemin_corps = "%s/SK_%s" % (destination, nom)

    # ---- 1. corps avec squelette
    if not EAL.does_asset_exist(chemin_corps):
        fbx = os.path.join(p["fbx"], p["corps"])
        if not os.path.isfile(fbx):
            unreal.log_error("[Personnages] %s : fichier introuvable %s" % (nom, fbx))
            return "%s : fichier %s introuvable" % (nom, fbx)
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_as_skeletal", True)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
        ui.set_editor_property("import_animations", False)
        avec_couleurs = not p["materiau"]
        ui.set_editor_property("import_materials", avec_couleurs)
        ui.set_editor_property("import_textures", avec_couleurs)
        ui.set_editor_property("create_physics_asset", True)
        ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
        dire("%s : import du corps %s" % (nom, fbx))
        importer(fbx, destination, "SK_" + nom, ui)
    corps = EAL.load_asset(chemin_corps)
    if not corps or not isinstance(corps, unreal.SkeletalMesh):
        unreal.log_error("[Personnages] %s : l'import du corps a echoue (voir Output Log)." % nom)
        return "%s : import du corps rate" % nom
    squelette = corps.get_editor_property("skeleton")

    # ---- 2. couleurs (materiau deja dans le projet)
    if p["materiau"]:
        materiau = EAL.load_asset(p["materiau"])
        if materiau:
            mats = corps.get_editor_property("materials")
            for i in range(len(mats)):
                m = mats[i]
                m.set_editor_property("material_interface", materiau)
                mats[i] = m  # en Python UE, mats[i] est une COPIE : il faut la remettre
            corps.set_editor_property("materials", mats)
            EAL.save_loaded_asset(corps)
        else:
            unreal.log_warning("[Personnages] %s introuvable : %s restera gris(e)." % (p["materiau"], nom))

    # ---- 3. animations
    faites = 0
    for fichier, etat in p["anims"]:
        voulu = "%s/A_%s_%s_Anim" % (dossier_anims, nom, etat)
        if EAL.does_asset_exist(voulu):
            faites += 1
            continue
        fbx = os.path.join(p["fbx"], fichier)
        if not os.path.isfile(fbx):
            unreal.log_warning("[Personnages] %s : animation absente %s" % (nom, fbx))
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
        importer(fbx, dossier_anims, "A_%s_%s" % (nom, etat), ua)
        if not EAL.does_asset_exist(voulu):
            # l'animation a pu garder le nom sans "_Anim"
            sans = "%s/A_%s_%s" % (dossier_anims, nom, etat)
            if EAL.does_asset_exist(sans) and classe(sans) == "AnimSequence":
                EAL.rename_asset(sans, voulu)
        if EAL.does_asset_exist(voulu):
            faites += 1
        else:
            unreal.log_warning("[Personnages] %s : animation non creee (%s)" % (nom, fichier))
    ranger_animations(dossier_anims, "SK_" + nom)
    dire("%s : %d animations sur %d." % (nom, faites, len(p["anims"])))

    # ---- 4. taille
    try:
        hauteur = corps.get_bounds().box_extent.z * 2.0
    except Exception:
        hauteur = 0.0
    if hauteur > 10.0:
        taille = round(max(0.2, min(5.0, p["taille_cm"] / hauteur)), 3)
        dire("%s mesure %.0f cm -> taille %s" % (nom, hauteur, taille))
        regler_taille(nom, taille)
    return "%s : pret(e), %d animations" % (nom.upper(), faites)


for perso in PERSONNAGES:
    resultats.append(creer(perso))
EAL.save_directory("/Game/Personnages", only_if_is_dirty=True, recursive=True)

texte = "\n".join(resultats)
dire("TERMINE.\n" + texte)
try:
    unreal.EditorDialog.show_message(
        "CHELLES 3D - personnages",
        texte + "\n\nFermez Unreal, compilez ChellesWraith et ChellesMenu,\npuis JOUER > HABILLAGE.",
        unreal.AppMsgType.OK)
except Exception:
    pass
