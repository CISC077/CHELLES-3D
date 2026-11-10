"""SCRIPT A LANCER DANS UNREAL 4.27, carte Maps/Rachid ouverte : File > Execute Python Script... > ce fichier.

Remet l'eclairage, les ombres, le brouillard et la neige comme avant alleger_eclairage.py
(grace a Tools/Eclairage/journal_eclairage.json). Journal : Tools/out/eclairage.log
"""
import datetime, json, os, traceback
import unreal

PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
JOURNAL_JSON = os.path.join(PROJET, "Tools", "Eclairage", "journal_eclairage.json")
FICHIER_LOG = os.path.join(PROJET, "Tools", "out", "eclairage.log")


def main():
    if not os.path.exists(JOURNAL_JSON):
        return "Pas de journal : rien a remettre."
    with open(JOURNAL_JSON, "r", encoding="utf-8") as f:
        journal = json.load(f)
    acteurs = {a.get_path_name(): a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
    remis, manquants = 0, 0
    for cle, props in journal.get("composants", {}).items():
        chemin, nom_comp = cle.split("|", 1)
        a = acteurs.get(chemin)
        comp = None
        if a:
            for c in a.get_components_by_class(unreal.ActorComponent):
                if c.get_name() == nom_comp:
                    comp = c
                    break
        if not comp:
            manquants += 1
            continue
        for prop, valeur in props.items():
            try:
                comp.set_editor_property(prop, valeur)
                remis += 1
            except Exception:
                manquants += 1
    n = journal.get("neige")
    if n:
        mi = unreal.load_asset(n["instance"])
        if mi:
            unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, n["parametre"], n["valeur"])
            unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
            remis += 1
    unreal.EditorLevelLibrary.save_current_level()
    if manquants == 0:
        os.replace(JOURNAL_JSON, JOURNAL_JSON + ".remis")
    return "%d reglage(s) remis comme avant, %d introuvable(s)." % (remis, manquants)


try:
    msg = main()
except Exception:
    msg = "ERREUR :\n" + traceback.format_exc()
    unreal.log_error(msg)
unreal.log("[Eclairage] " + msg)
try:
    unreal.EditorDialog.show_message("Eclairage", msg, unreal.AppMsgType.OK)
except Exception:
    pass
try:
    os.makedirs(os.path.dirname(FICHIER_LOG), exist_ok=True)
    with open(FICHIER_LOG, "a", encoding="utf-8") as f:
        f.write("\n===== %s =====\n%s\n" % (datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"), msg))
except Exception:
    pass
