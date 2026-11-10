"""SCRIPT A LANCER DANS UNREAL 4.27 : File > Execute Python Script... > ce fichier.
(la carte Rachid doit etre ouverte)

Remet la carte comme avant corriger_mapcheck.py, en lisant Tools\\MapCheck\\journal_mapcheck.json
(le dernier passage d'abord). Demande Oui / Non. Ensuite : Build > Build Paths, puis Ctrl+S.
Autre solution : la copie du .umap dans Tools\\MapCheck\\sauvegarde\\ (Unreal ferme).
"""
import json
import os
import traceback

import unreal

PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
FICHIER_JOURNAL = os.path.join(PROJET, "Tools", "MapCheck", "journal_mapcheck.json")


def label(a):
    try:
        return a.get_actor_label()
    except Exception:
        return a.get_name()


def charger(chemin):
    if not chemin:
        return None
    return unreal.EditorAssetLibrary.load_asset(chemin)


def composant(a, nom):
    if nom is None:
        return None
    for c in a.get_components_by_class(unreal.ActorComponent):
        if c.get_name() == nom:
            return c
    return None


def remettre(m, a):
    c = composant(a, m["composant"])
    quoi, avant = m["quoi"], m["avant"]
    try:
        a.modify()
        if c:
            c.modify()
    except Exception:
        pass
    if quoi == "transform":
        a.set_actor_scale3d(unreal.Vector(*avant["ech"]))
        a.set_actor_location(unreal.Vector(*avant["loc"]), False, False)
        return True
    if c is None:
        return False
    if quoi in ("cast_dynamic_shadow", "cast_shadow"):
        c.set_editor_property(quoi, bool(avant))
    elif quoi == "static_mesh":
        c.set_editor_property("static_mesh", charger(avant))
    elif quoi == "collision":
        c.set_collision_enabled(getattr(unreal.CollisionEnabled, avant, unreal.CollisionEnabled.QUERY_AND_PHYSICS))
    elif quoi == "override_materials":
        c.set_editor_property("override_materials", [charger(p) for p in avant])
    else:
        return False
    return True


def main():
    if not os.path.isfile(FICHIER_JOURNAL):
        unreal.EditorDialog.show_message("Map Check - annuler", "Aucun journal : rien a annuler.", unreal.AppMsgType.OK)
        return
    passages = json.load(open(FICHIER_JOURNAL, encoding="utf-8")).get("passages", [])
    total = sum(len(p["modifs"]) for p in passages)
    if not total:
        unreal.EditorDialog.show_message("Map Check - annuler", "Journal vide : rien a annuler.", unreal.AppMsgType.OK)
        return
    if unreal.EditorDialog.show_message(
            "Map Check - annuler",
            "Remettre %d modifications (%d passage(s)) comme avant ?" % (total, len(passages)),
            unreal.AppMsgType.YES_NO) != unreal.AppReturnType.YES:
        return

    acteurs = unreal.EditorLevelLibrary.get_all_level_actors()
    par_chemin = {a.get_path_name(): a for a in acteurs}
    par_label = {label(a): a for a in acteurs}
    ok, rate = 0, []
    with unreal.ScopedEditorTransaction("Annuler corrections Map Check"):
        for p in reversed(passages):
            for m in reversed(p["modifs"]):
                a = par_chemin.get(m["acteur"]) or par_label.get(m["label"])
                try:
                    if a is not None and remettre(m, a):
                        ok += 1
                        continue
                except Exception as e:
                    unreal.log_warning("[MapCheck] %s : %s" % (m["label"], e))
                rate.append("%s (%s)" % (m["label"], m["quoi"]))

    os.replace(FICHIER_JOURNAL, FICHIER_JOURNAL.replace(".json", "_annule.json"))
    unreal.EditorDialog.show_message(
        "Map Check - annuler",
        "%d modifications remises.\n%s\nMaintenant : Build > Build Paths, puis Ctrl+S."
        % (ok, ("Non retrouves : " + ", ".join(rate[:15]) + "\n") if rate else ""),
        unreal.AppMsgType.OK)


try:
    main()
except Exception:
    unreal.log_error(traceback.format_exc())
    unreal.EditorDialog.show_message("Map Check - erreur", traceback.format_exc()[-1500:], unreal.AppMsgType.OK)
