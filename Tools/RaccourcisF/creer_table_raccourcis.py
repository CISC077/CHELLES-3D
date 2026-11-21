# -*- coding: utf-8 -*-
"""CHELLES 3D - cree le tableau de donnees des touches F1-F12 : /Game/Chelles/Donnees/DT_Raccourcis

A lancer avec CREER_LA_TABLE.bat (Unreal FERME) ou dans l'editeur :
    File > Execute Python Script... > Tools/RaccourcisF/creer_table_raccourcis.py
Il faut que le plugin ChellesRaccourcis soit compile (structure ChellesRaccourciLigne).

  1. cree DT_Raccourcis (structure ChellesRaccourciLigne) et le remplit depuis Raccourcis.csv ;
     s'il existe deja, il n'est PAS ecrase (vos changements sont gardes), sauf si CHELLES_REMPLACER=1 ;
  2. ajoute /Game/Chelles/Donnees aux dossiers toujours exportes (Config/DefaultGame.ini, copie .avant_raccourcis).
Journal : Tools/RaccourcisF/table.log
"""
import os
import shutil
import traceback

import unreal

DOSSIER = "/Game/Chelles/Donnees"
NOM = "DT_Raccourcis"
ICI = os.path.dirname(os.path.abspath(__file__))
CSV = os.path.join(ICI, "Raccourcis.csv")
JOURNAL = os.path.join(ICI, "table.log")
LIGNES = []


def log(texte):
    LIGNES.append(texte)
    unreal.log("[Raccourcis] " + texte)
    with open(JOURNAL, "w", encoding="utf-8") as f:
        f.write("\n".join(LIGNES) + "\n")


def creer_table():
    chemin = DOSSIER + "/" + NOM
    structure = unreal.load_object(None, "/Script/ChellesRaccourcis.ChellesRaccourciLigne")
    if structure is None:
        log("ERREUR : structure ChellesRaccourciLigne introuvable. Compilez d'abord Plugins/ChellesRaccourcis (Compiler_le_plugin.bat).")
        return False
    existe = unreal.EditorAssetLibrary.does_asset_exist(chemin)
    if existe and os.environ.get("CHELLES_REMPLACER") != "1":
        log("DT_Raccourcis existe deja : garde tel quel (pour le refaire depuis le CSV : REMPLACER_LA_TABLE.bat).")
        return True
    if existe:
        table = unreal.EditorAssetLibrary.load_asset(chemin)
    else:
        usine = unreal.DataTableFactory()
        usine.set_editor_property("struct", structure)
        table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NOM, DOSSIER, unreal.DataTable, usine)
    if table is None:
        log("ERREUR : impossible de creer " + chemin)
        return False
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(table, CSV):
        log("ERREUR : lecture de " + CSV + " impossible (format ?)")
        return False
    unreal.EditorAssetLibrary.save_loaded_asset(table)
    noms = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    log("DT_Raccourcis cree/rempli : %d lignes (%s)." % (len(noms), ", ".join(str(n) for n in noms)))
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
    if creer_table():
        ajouter_au_cook()
    log("Fin.")
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
