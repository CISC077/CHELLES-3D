"""SCRIPT A LANCER DANS UNREAL 4.27 : File > Execute Python Script... > ce fichier.

Enleve les LOD ajoutes par ajouter_lod.py (liste dans Tools/LOD/journal_lod.json).
L'objet d'origine (LOD 0) reste tel quel. Journal : Tools/out/lod.log
"""
import datetime, json, os, traceback
import unreal

PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
JOURNAL_JSON = os.path.join(PROJET, "Tools", "LOD", "journal_lod.json")
FICHIER_LOG = os.path.join(PROJET, "Tools", "out", "lod.log")


def main():
    if not os.path.exists(JOURNAL_JSON):
        return "Pas de journal : rien a retirer."
    with open(JOURNAL_JSON, "r", encoding="utf-8") as f:
        liste = json.load(f)
    restants, faits = [], 0
    with unreal.ScopedSlowTask(len(liste), "Retrait des LOD...") as t:
        t.make_dialog(True)
        for ch in liste:
            if t.should_cancel():
                restants.append(ch)
                continue
            t.enter_progress_frame(1, ch)
            try:
                m = unreal.load_asset(ch)
                unreal.EditorStaticMeshLibrary.remove_lods(m)
                unreal.EditorAssetLibrary.save_loaded_asset(m, False)
                faits += 1
            except Exception:
                restants.append(ch)
    if restants:
        with open(JOURNAL_JSON, "w", encoding="utf-8") as f:
            json.dump(restants, f, indent=1, ensure_ascii=False)
    else:
        os.replace(JOURNAL_JSON, JOURNAL_JSON + ".retire")
    return "LOD retires de %d objet(s), %d en attente." % (faits, len(restants))


try:
    msg = main()
except Exception:
    msg = "ERREUR :\n" + traceback.format_exc()
    unreal.log_error(msg)
unreal.log("[LOD] " + msg)
try:
    unreal.EditorDialog.show_message("LOD", msg, unreal.AppMsgType.OK)
except Exception:
    pass
try:
    os.makedirs(os.path.dirname(FICHIER_LOG), exist_ok=True)
    with open(FICHIER_LOG, "a", encoding="utf-8") as f:
        f.write("\n===== %s =====\n%s\n" % (datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"), msg))
except Exception:
    pass
