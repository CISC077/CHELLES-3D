# -*- coding: utf-8 -*-
# CHELLES 3D - Importe la bibliotheque de sons du menu et du jeu (a lancer une fois).
# Unreal : File > Execute Python Script... > choisir ce fichier.
# Les .wav sont dans Tools\Menu\Musiques, en morceaux (.part00, .part01...) : le script les recolle.
# Cree /Game/ChellesMenu/Audio/Menu_* (playlist du menu) et Jeu_* (playlist du jeu).
# Supprime aussi les anciennes musiques (Musique_Menu, Musique_Jeu, Musique_Jeu2, Musique_Jeu3).
import glob
import os
import unreal

PROJET = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
DOSSIER = os.path.join(PROJET, "Tools", "Menu", "Musiques")
CIBLE = "/Game/ChellesMenu/Audio"
SONS = ["Menu_1_RetroGame1", "Menu_2_RetroGame2", "Menu_3_RetroGame3", "Menu_4_Junkala",
        "Jeu_0_Juhani", "Jeu_1_AiFight", "Jeu_2_ElectricStream", "Jeu_3_Factory",
        "Jeu_4_JumpingCyborg", "Jeu_5_Overload", "Jeu_6_SpaceRay", "Jeu_7_Spaceship"]
ANCIENS = ["Musique_Menu", "Musique_Jeu", "Musique_Jeu2", "Musique_Jeu3"]

# 1. Anciennes musiques : assets Unreal et fichiers .wav / morceaux
for nom in ANCIENS:
    chemin_asset = CIBLE + "/" + nom
    if unreal.EditorAssetLibrary.does_asset_exist(chemin_asset):
        unreal.EditorAssetLibrary.delete_asset(chemin_asset)
        unreal.log_warning("[Menu] Ancienne musique supprimee : " + chemin_asset)
    for fichier in glob.glob(os.path.join(DOSSIER, nom + ".wav*")):
        try:
            os.remove(fichier)
        except OSError:
            pass

# 2. Recolle les morceaux
for nom in SONS:
    chemin = os.path.join(DOSSIER, nom + ".wav")
    morceaux = sorted(glob.glob(chemin + ".part*"))
    if morceaux and not os.path.isfile(chemin):
        with open(chemin, "wb") as sortie:
            for m in morceaux:
                with open(m, "rb") as entree:
                    sortie.write(entree.read())

# 3. Import
taches = []
for nom in SONS:
    chemin = os.path.join(DOSSIER, nom + ".wav")
    if not os.path.isfile(chemin):
        unreal.log_error("[Menu] Fichier introuvable : " + chemin)
        continue
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", chemin)
    t.set_editor_property("destination_path", CIBLE)
    t.set_editor_property("destination_name", nom)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    taches.append(t)
if taches:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(taches)

# 4. Pas de boucle : chaque playlist passe au morceau suivant quand un morceau se termine
for nom in SONS:
    chemin_asset = CIBLE + "/" + nom
    son = unreal.EditorAssetLibrary.load_asset(chemin_asset)
    if son:
        son.set_editor_property("looping", False)
        unreal.EditorAssetLibrary.save_loaded_asset(son)
        unreal.log_warning("[Menu] OK : " + chemin_asset)
    else:
        unreal.log_error("[Menu] Import rate : " + chemin_asset)

unreal.log_warning("[Menu] Termine. Clique sur Play pour entendre les musiques.")
