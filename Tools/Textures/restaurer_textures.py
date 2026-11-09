"""SCRIPT A LANCER DANS UNREAL 4.27 : File > Execute Python Script... > ce fichier.

Remet les textures comme avant reduire_textures.py, grace a Tools/Textures/journal_textures.json.
Journal : Tools/out/textures.log
"""
import datetime, json, os, traceback
import unreal

PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
JOURNAL_JSON = os.path.join(PROJET, "Tools", "Textures", "journal_textures.json")
FICHIER_LOG = os.path.join(PROJET, "Tools", "out", "textures.log")


def main():
    if not os.path.exists(JOURNAL_JSON):
        unreal.EditorDialog.show_message("Textures", "Pas de journal : rien a restaurer.", unreal.AppMsgType.OK)
        return 0, 0
    with open(JOURNAL_JSON, "r", encoding="utf-8") as f:
        journal = json.load(f)
    remises, restantes = 0, {}
    with unreal.ScopedSlowTask(len(journal), "Restauration des textures...") as t:
        t.make_dialog(True)
        for ch, ancien in journal.items():
            if t.should_cancel():
                restantes[ch] = ancien
                continue
            t.enter_progress_frame(1, ch)
            try:
                tex = unreal.load_asset(ch)
                tex.set_editor_property("max_texture_size", int(ancien))
                unreal.EditorAssetLibrary.save_loaded_asset(tex, False)
                remises += 1
            except Exception:
                restantes[ch] = ancien
    if restantes:
        with open(JOURNAL_JSON, "w", encoding="utf-8") as f:
            json.dump(restantes, f, indent=1, ensure_ascii=False)
    else:
        os.replace(JOURNAL_JSON, JOURNAL_JSON + ".restaure")
    unreal.EditorDialog.show_message("Textures", "%d texture(s) remises comme avant.%s" % (
        remises, ("\n%d non restaurees (relancer le script)." % len(restantes)) if restantes else ""),
        unreal.AppMsgType.OK)
    return remises, len(restantes)


try:
    r, n = main()
    msg = "Restauration : %d remise(s), %d en attente." % (r, n)
except Exception:
    msg = "ERREUR restauration :\n" + traceback.format_exc()
    unreal.log_error(msg)
unreal.log("[Textures] " + msg)
try:
    os.makedirs(os.path.dirname(FICHIER_LOG), exist_ok=True)
    with open(FICHIER_LOG, "a", encoding="utf-8") as f:
        f.write("\n===== %s =====\n%s\n" % (datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"), msg))
except Exception:
    pass
