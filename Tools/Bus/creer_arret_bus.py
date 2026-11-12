# -*- coding: utf-8 -*-
"""CREATION DU BLUEPRINT D'ARRET DE BUS - lance par INSTALLER_BUS.bat (Unreal ferme).

Cree /Game/Chelles/Bus/BP_Arret_Bus : un Blueprint Actor enfant de la classe C++ "Arret de bus (Chelles)"
du plugin ChellesBus. Ensuite, glissez BP_Arret_Bus dans la carte Rachid, au bord des routes :
pendant l'invasion zombie, le bus BP_Bus_sity passe par tous ces arrets.

Le nom est ecrit sans accent (BP_Arret_Bus) : les accents dans les noms de fichiers posent des
problemes a Unreal (empaquetage du jeu, Git). Le panneau affiche bien "Arret" avec son accent.

La carte n'est PAS modifiee. Pour tout enlever : supprimer le dossier Content/Chelles/Bus.
Journal : Tools/Bus/creer_arret_bus.log
"""
import os
import traceback

import unreal

DOSSIER = "/Game/Chelles/Bus"
NOM = "BP_Arret_Bus"
CLASSE = "/Script/ChellesBus.ArretBusChelles"

DOSSIER_SCRIPT = os.path.join(unreal.SystemLibrary.get_project_directory(), "Tools", "Bus")
JOURNAL = os.path.join(DOSSIER_SCRIPT, "creer_arret_bus.log")
open(JOURNAL, "w").write("")


def log(msg):
    unreal.log("[Bus] " + str(msg))
    with open(JOURNAL, "a", encoding="utf-8") as f:
        f.write(str(msg) + "\n")


def principal():
    eal = unreal.EditorAssetLibrary
    chemin = DOSSIER + "/" + NOM
    if eal.does_asset_exist(chemin):
        log("Deja present : " + chemin + " (rien a faire).")
        return True

    parent = unreal.load_class(None, CLASSE)
    if parent is None:
        log("ERREUR : classe " + CLASSE + " introuvable. Le plugin ChellesBus est-il compile et active ?")
        return False

    if not eal.does_directory_exist(DOSSIER):
        eal.make_directory(DOSSIER)
    fabrique = unreal.BlueprintFactory()
    fabrique.set_editor_property("parent_class", parent)
    outils = unreal.AssetToolsHelpers.get_asset_tools()
    bp = outils.create_asset(NOM, DOSSIER, unreal.Blueprint, fabrique)
    if bp is None:
        log("ERREUR : creation de " + chemin + " impossible.")
        return False
    if hasattr(unreal, "BlueprintEditorLibrary"):
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp)
    log("Cree : " + chemin)
    log("Glissez-le dans la carte Rachid au bord des routes (fleche bleue vers la chaussee).")
    return True


try:
    if principal():
        log("TERMINE.")
    else:
        log("ECHEC.")
except Exception:
    log("ERREUR :\n" + traceback.format_exc())
