# -*- coding: utf-8 -*-
# CHELLES 3D - Importe les 2 musiques du menu dans Unreal (a lancer UNE fois)
# Unreal : File > Execute Python Script... > choisir ce fichier.
# Cree /Game/ChellesMenu/Audio/Musique_Menu et Musique_Jeu (en boucle).
import os
import unreal

PROJET = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
DOSSIER = os.path.join(PROJET, "Tools", "Menu", "Musiques")
CIBLE = "/Game/ChellesMenu/Audio"
MUSIQUES = [("Musique_Menu.wav", "Musique_Menu"), ("Musique_Jeu.wav", "Musique_Jeu")]

taches = []
for fichier, nom in MUSIQUES:
    chemin = os.path.join(DOSSIER, fichier)
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

for fichier, nom in MUSIQUES:
    chemin_asset = CIBLE + "/" + nom
    son = unreal.EditorAssetLibrary.load_asset(chemin_asset)
    if son:
        son.set_editor_property("looping", True)
        unreal.EditorAssetLibrary.save_loaded_asset(son)
        unreal.log_warning("[Menu] OK : " + chemin_asset + " (en boucle)")
    else:
        unreal.log_error("[Menu] Import rate : " + chemin_asset)

unreal.log_warning("[Menu] Termine. Clique sur Play pour entendre les musiques.")
