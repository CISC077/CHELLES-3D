# -*- coding: utf-8 -*-
"""
CHELLES 3D - Cree BP_Police_Drivable a partir du pack "Sports and Police Cars - Drivable".
A lancer DANS Unreal (INSTALLER_POLICE.bat le fait tout seul, ou File > Execute Python Script).

/Game/SportVehiclesPack/Blueprints/Vehicles/BP_Police_Drivable = enfant de BP_WheeledVehicle_Sportcar1_Police :
  - copie par le reseau (multijoueur), aucune IA ne la prend ;
  - sirene et gyrophares eteints a l'arret (le plugin ChellesPolice les allume quand quelqu'un conduit).
Journal : Tools\\Police\\police.log
"""
import os
import unreal

DOSSIER = "/Game/SportVehiclesPack/Blueprints/Vehicles"
NOM = "BP_Police_Drivable"
PARENT = DOSSIER + "/BP_WheeledVehicle_Sportcar1_Police"
CHEMIN = DOSSIER + "/" + NOM
JOURNAL = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", "Police", "police.log")

_lignes = []


def log(texte):
    unreal.log("[Police] " + texte)
    _lignes.append(texte)


def reglage(objet, nom, valeur):
    try:
        objet.set_editor_property(nom, valeur)
        log("  %s = %s" % (nom, valeur))
        return True
    except Exception as e:
        log("  (%s non regle : %s)" % (nom, e))
        return False


def principal():
    eal = unreal.EditorAssetLibrary
    if not eal.does_asset_exist(PARENT):
        log("ERREUR : %s introuvable. Le pack Sports and Police Cars est-il dans Content\\SportVehiclesPack ?" % PARENT)
        return
    if eal.does_asset_exist(CHEMIN):
        log("%s existe deja : on met seulement ses reglages a jour." % CHEMIN)
        bp = eal.load_asset(CHEMIN)
    else:
        parent = eal.load_blueprint_class(PARENT)
        usine = unreal.BlueprintFactory()
        usine.set_editor_property("parent_class", parent)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NOM, DOSSIER, unreal.Blueprint, usine)
        if not bp:
            log("ERREUR : impossible de creer " + CHEMIN)
            return
        log("Cree : " + CHEMIN + " (enfant de BP_WheeledVehicle_Sportcar1_Police)")

    classe = eal.load_blueprint_class(CHEMIN)
    cdo = unreal.get_default_object(classe)
    reglage(cdo, "replicates", True)
    reglage(cdo, "replicate_movement", True)
    reglage(cdo, "always_relevant", False)
    reglage(cdo, "auto_possess_ai", unreal.AutoPossessAI.DISABLED)
    reglage(cdo, "auto_possess_player", unreal.AutoReceiveInput.DISABLED)
    # Variables du Blueprint du pack (gyrophares / sirene allumes au demarrage) : eteints a l'arret
    for nom in ("AutoStartLights", "AutoStartSiren"):
        reglage(cdo, nom, False)

    try:
        if hasattr(unreal, "BlueprintEditorLibrary"):
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            log("Blueprint compile.")
    except Exception as e:
        log("(compilation automatique impossible : %s)" % e)

    if eal.save_asset(CHEMIN, only_if_is_dirty=False):
        log("Enregistre : " + CHEMIN)
    else:
        log("ERREUR : enregistrement impossible de " + CHEMIN)


try:
    principal()
except Exception as e:
    log("ERREUR inattendue : %s" % e)
finally:
    try:
        os.makedirs(os.path.dirname(JOURNAL), exist_ok=True)
        with open(JOURNAL, "w", encoding="utf-8") as f:
            f.write("\n".join(_lignes) + "\n")
    except Exception:
        pass
    if os.environ.get("CHELLES_QUITTER") == "1":
        unreal.SystemLibrary.quit_editor()
