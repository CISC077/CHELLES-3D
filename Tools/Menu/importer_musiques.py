# -*- coding: utf-8 -*-
# CHELLES 3D - Importe les 2 musiques du menu dans Unreal (a lancer UNE fois)
# Unreal : File > Execute Python Script... > choisir ce fichier.
# Cree /Game/ChellesMenu/Audio/Musique_Menu (en boucle) et Musique_Jeu, Musique_Jeu2, Musique_Jeu3 (playlist).
import os
import unreal

PROJET = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
DOSSIER = os.path.join(PROJET, "Tools", "Menu", "Musiques")
CIBLE = "/Game/ChellesMenu/Audio"
# (fichier, asset, en boucle ?) : la musique du menu boucle ; celles du jeu s'enchainent (playlist).
MUSIQUES = [("Musique_Menu.wav", "Musique_Menu", True), ("Musique_Jeu.wav", "Musique_Jeu", False),
            ("Musique_Jeu2.wav", "Musique_Jeu2", False), ("Musique_Jeu3.wav", "Musique_Jeu3", False)]

# Les .wav arrivent en morceaux (part00, part01...) : on les recolle d'abord.
import glob
for fichier, nom, boucle in MUSIQUES:
    chemin = os.path.join(DOSSIER, fichier)
    morceaux = sorted(glob.glob(chemin + ".part*"))
    if morceaux and not os.path.isfile(chemin):
        with open(chemin, "wb") as sortie:
            for m in morceaux:
                with open(m, "rb") as entree:
                    sortie.write(entree.read())
        unreal.log_warning("[Menu] Recolle : " + chemin)

taches = []
for fichier, nom, boucle in MUSIQUES:
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

for fichier, nom, boucle in MUSIQUES:
    chemin_asset = CIBLE + "/" + nom
    son = unreal.EditorAssetLibrary.load_asset(chemin_asset)
    if son:
        son.set_editor_property("looping", boucle)
        # Longs mix : lus en continu depuis le disque, pas charges d'un coup en memoire.
        try:
            son.set_editor_property("streaming", True)
        except Exception:
            pass
        unreal.EditorAssetLibrary.save_loaded_asset(son)
        unreal.log_warning("[Menu] OK : " + chemin_asset + (" (en boucle)" if boucle else " (playlist)"))
    else:
        unreal.log_error("[Menu] Import rate : " + chemin_asset)

unreal.log_warning("[Menu] Termine. Clique sur Play pour entendre les musiques.")
