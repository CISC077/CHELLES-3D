# -*- coding: utf-8 -*-
"""CHELLES 3D - cree les tableaux de donnees : touches F1-F12 (DT_Raccourcis) et roue des armes TAB (DT_Armes)

A lancer avec CREER_LA_TABLE.bat (Unreal FERME) ou dans l'editeur :
    File > Execute Python Script... > Tools/RaccourcisF/creer_table_raccourcis.py
Il faut que le plugin ChellesRaccourcis soit compile (structure ChellesRaccourciLigne).

  1. cree DT_Raccourcis (structure ChellesRaccourciLigne) depuis Raccourcis.csv
     et DT_Armes (structure ChellesArmeLigne) depuis Armes.csv ;
     un tableau qui existe deja n'est PAS ecrase (vos changements sont gardes), sauf si CHELLES_REMPLACER=1 ;
  2. ajoute /Game/Chelles/Donnees aux dossiers toujours exportes (Config/DefaultGame.ini, copie .avant_raccourcis).
Journal : Tools/RaccourcisF/table.log
"""
import os
import shutil
import traceback

import unreal

DOSSIER = "/Game/Chelles/Donnees"
ICI = os.path.dirname(os.path.abspath(__file__))
# (nom du tableau, structure C++, fichier CSV)
TABLEAUX = [
    ("DT_Raccourcis", "ChellesRaccourciLigne", "Raccourcis.csv"),
    ("DT_Armes", "ChellesArmeLigne", "Armes.csv"),
]
JOURNAL = os.path.join(ICI, "table.log")
LIGNES = []


def log(texte):
    LIGNES.append(texte)
    unreal.log("[Raccourcis] " + texte)
    with open(JOURNAL, "w", encoding="utf-8") as f:
        f.write("\n".join(LIGNES) + "\n")


def creer_table(nom, nom_structure, fichier_csv):
    chemin = DOSSIER + "/" + nom
    csv = os.path.join(ICI, fichier_csv)
    structure = unreal.load_object(None, "/Script/ChellesRaccourcis." + nom_structure)
    if structure is None:
        log("ERREUR : structure " + nom_structure + " introuvable. Compilez d'abord Plugins/ChellesRaccourcis (Compiler_le_plugin.bat).")
        return False
    existe = unreal.EditorAssetLibrary.does_asset_exist(chemin)
    if existe and os.environ.get("CHELLES_REMPLACER") != "1":
        log(nom + " existe deja : garde tel quel (pour le refaire depuis " + fichier_csv + " : REMPLACER_LA_TABLE.bat).")
        return True
    if existe:
        table = unreal.EditorAssetLibrary.load_asset(chemin)
    else:
        usine = unreal.DataTableFactory()
        usine.set_editor_property("struct", structure)
        table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(nom, DOSSIER, unreal.DataTable, usine)
    if table is None:
        log("ERREUR : impossible de creer " + chemin)
        return False
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(table, csv):
        log("ERREUR : lecture de " + csv + " impossible (format ?)")
        return False
    unreal.EditorAssetLibrary.save_loaded_asset(table)
    noms = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    log("%s cree/rempli : %d lignes (%s)." % (nom, len(noms), ", ".join(str(n) for n in noms)))
    return True


def ajouter_au_cook():
    projet = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    ini = os.path.join(projet, "Config", "DefaultGame.ini")
    if not os.path.exists(ini):
        log("DefaultGame.ini introuvable : ajoutez /Game/Chelles/Donnees aux dossiers a exporter a la main.")
        return
    with open(ini, "rb") as f:
        brut = f.read()
    bom = brut.startswith(b"\xef\xbb\xbf")
    texte = brut.decode("utf-8-sig")
    ligne = '+DirectoriesToAlwaysCook=(Path="/Game/Chelles/Donnees")'
    if ligne in texte:
        log("Export : /Game/Chelles/Donnees deja dans DefaultGame.ini.")
        return
    section = "[/Script/UnrealEd.ProjectPackagingSettings]"
    fin = "\r\n" if "\r\n" in texte else "\n"
    if section in texte:
        texte = texte.replace(section, section + fin + ligne, 1)
    else:
        texte = texte.rstrip() + fin + fin + section + fin + ligne + fin
    shutil.copyfile(ini, ini + ".avant_raccourcis")
    with open(ini, "wb") as f:
        f.write((b"\xef\xbb\xbf" if bom else b"") + texte.encode("utf-8"))
    log("Export : /Game/Chelles/Donnees ajoute a DefaultGame.ini (copie DefaultGame.ini.avant_raccourcis).")


try:
    log("Debut.")
    reussi = False
    for nom, structure, fichier in TABLEAUX:
        reussi = creer_table(nom, structure, fichier) or reussi
    if reussi:
        ajouter_au_cook()
    log("Fin.")
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
