"""SCRIPT A LANCER DANS UNREAL 4.27 : File > Execute Python Script... > ce fichier.

Remet Rachid comme AVANT installer_stylized_landscape.py :
  - le Landscape et les dalles Chelles_Tile_Ground reprennent leur ancien materiau
    (note dans Tools/StylizedLandscape/journal_stylized.json, en general le sol cartoon MI_SolCartoon) ;
  - les objets AJOUTES par le script (etiquette ChellesDecorStylized, dossier Chelles > DecorStylized
    de l'Outliner) sont retires de la carte. Rien d'autre n'est touche.
Le pack Stylized_Landscape et /Game/Chelles/StylizedLandscape restent dans le projet.
Si quelque chose ne va toujours pas : fermer Unreal et remettre la copie
Tools/StylizedLandscape/sauvegarde/Rachid.umap dans Content/Maps/.
Journal texte : Tools/out/stylized_landscape.log
"""
import datetime, json, os, traceback
import unreal

CARTE = "/Game/Maps/Rachid"
TAG = "ChellesDecorStylized"
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DOSSIER_OUTIL = os.path.join(PROJET, "Tools", "StylizedLandscape")
JOURNAL_JSON = os.path.join(DOSSIER_OUTIL, "journal_stylized.json")
FICHIER_LOG = os.path.join(PROJET, "Tools", "out", "stylized_landscape.log")
ELL = unreal.EditorLevelLibrary
EAL = unreal.EditorAssetLibrary
TITRE = "Annuler Stylized Landscape"
_lignes = []


def log(msg):
    unreal.log("[Stylized] " + msg)
    _lignes.append(msg)


def ecrire_log():
    try:
        os.makedirs(os.path.dirname(FICHIER_LOG), exist_ok=True)
        with open(FICHIER_LOG, "a", encoding="utf-8") as f:
            f.write("\n===== ANNULATION %s =====\n%s\n" % (datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"), "\n".join(_lignes)))
    except Exception:
        pass


def message(texte, oui_non=False):
    t = unreal.AppMsgType.YES_NO if oui_non else unreal.AppMsgType.OK
    return unreal.EditorDialog.show_message(TITRE, texte, t) == unreal.AppReturnType.YES


def main():
    monde = ELL.get_editor_world()
    if monde is None or monde.get_name() != CARTE.split("/")[-1]:
        if not message("Ouvrir la carte Rachid pour l'annulation ?", True):
            return
        ELL.save_all_dirty_levels()
        if not ELL.load_level(CARTE):
            message("Impossible d'ouvrir %s" % CARTE)
            return
    try:
        with open(JOURNAL_JSON, "r", encoding="utf-8") as f:
            j = json.load(f)
    except Exception:
        j = {"landscapes": {}, "dalles": {}}

    acteurs = ELL.get_all_level_actors()
    ajoutes = []
    for a in acteurs:
        try:
            if unreal.Name(TAG) in list(a.get_editor_property("tags") or []):
                ajoutes.append(a)
        except Exception:
            pass
    par_chemin = {a.get_path_name(): a for a in acteurs}
    lands = [k for k in j.get("landscapes", {}) if k in par_chemin]
    dalles = [k for k in j.get("dalles", {}) if k in par_chemin]
    if not (ajoutes or lands or dalles):
        message("Rien a annuler : le sol et le decor Stylized ne sont pas dans Rachid.")
        return
    if not message("A remettre comme avant :\n- %d Landscape, %d dalle(s) de sol (ancien materiau)\n"
                   "- %d objet(s) du decor Stylized retires de la carte\n\nContinuer ?"
                   % (len(lands), len(dalles), len(ajoutes)), True):
        log("Annulation abandonnee.")
        return

    for k in lands:
        ancien = j["landscapes"][k]
        mat = EAL.load_asset(ancien) if ancien else None
        par_chemin[k].set_editor_property("landscape_material", mat)
        log("Landscape %s -> %s" % (par_chemin[k].get_actor_label(), ancien))
    for k in dalles:
        comp = par_chemin[k].static_mesh_component
        anciens = [EAL.load_asset(p) if p else None for p in j["dalles"][k]]
        comp.set_editor_property("override_materials", anciens)
        log("Dalle %s -> materiaux d'avant" % par_chemin[k].get_actor_label())
    for a in ajoutes:
        ELL.destroy_actor(a)
    log("%d objet(s) du decor retires." % len(ajoutes))
    try:
        unreal.SystemLibrary.execute_console_command(ELL.get_editor_world(), "grass.FlushCache")
    except Exception:
        pass
    # le journal est vide : un prochain lancement du script repartira de l'etat actuel
    try:
        os.replace(JOURNAL_JSON, JOURNAL_JSON + ".annule_" + datetime.datetime.now().strftime("%Y%m%d_%H%M"))
    except Exception:
        pass
    ELL.save_current_level()
    message("C'est remis comme avant et enregistre.")


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())
    unreal.log_error(traceback.format_exc())
    try:
        message("Erreur : voir Tools/out/stylized_landscape.log")
    except Exception:
        pass
finally:
    ecrire_log()
