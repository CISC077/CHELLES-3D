"""SCRIPT A LANCER DANS UNREAL 4.27, carte Maps/Rachid ouverte : File > Execute Python Script... > ce fichier.

Remet le sol et l'herbe EXACTEMENT comme avant sol_cartoon.py, grace a Tools/SolCartoon/journal_sol_cartoon.json :
materiau d'origine du Landscape et de Chelles_Tile_Ground, LOD du Landscape, distances et ombres du foliage,
densite de l'herbe automatique. Les assets M_SolCartoon / MI_SolCartoon restent dans
/Game/Chelles/SolCartoon (on peut les effacer a la main, ils ne servent plus).
Journal texte : Tools/out/sol_cartoon.log
"""
import datetime, json, os, traceback
import unreal

PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
JOURNAL_JSON = os.path.join(PROJET, "Tools", "SolCartoon", "journal_sol_cartoon.json")
FICHIER_LOG = os.path.join(PROJET, "Tools", "out", "sol_cartoon.log")
EAL = unreal.EditorAssetLibrary
ELL = unreal.EditorLevelLibrary


def charger(ch):
    return EAL.load_asset(ch) if ch else None


def _pp_ecrire(v, prop, valeur):
    pp = v.get_editor_property(prop)
    try:
        pp.set_editor_property("default", valeur)
        v.set_editor_property(prop, pp)
    except Exception:
        v.set_editor_property(prop, valeur)


def main():
    if not os.path.exists(JOURNAL_JSON):
        return "Pas de journal : rien a retirer."
    with open(JOURNAL_JSON, "r", encoding="utf-8") as f:
        j = json.load(f)

    acteurs = ELL.get_all_level_actors()
    par_chemin = {a.get_path_name(): a for a in acteurs}
    comps = {}
    for a in acteurs:
        if isinstance(a, unreal.InstancedFoliageActor):
            for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent) or []:
                comps[c.get_path_name()] = c

    n_l = n_t = n_f = n_h = 0
    manquants = []
    for k, jl in j.get("landscapes", {}).items():
        l = par_chemin.get(k)
        if l is None:
            manquants.append(k)
            continue
        if "landscape_material" in jl:
            l.set_editor_property("landscape_material", charger(jl["landscape_material"]))
        for prop in ("lod0_screen_size", "lod0_distribution_setting", "lod_distribution_setting"):
            if prop in jl:
                l.set_editor_property(prop, float(jl[prop]))
        n_l += 1

    for k, orig in j.get("tuiles_sol", {}).items():
        a = par_chemin.get(k)
        if a is None:
            manquants.append(k)
            continue
        comp = a.static_mesh_component
        for i in range(max(1, comp.get_num_materials())):
            comp.set_material(i, charger(orig[i]) if i < len(orig) else None)
        n_t += 1

    for k, o in j.get("foliage", {}).items():
        c = comps.get(k)
        if c is None:
            manquants.append(k)
            continue
        c.modify()
        c.set_cull_distances(int(o["start"]), int(o["end"]))
        c.set_editor_property("instance_start_cull_distance", int(o["start"]))
        c.set_editor_property("instance_end_cull_distance", int(o["end"]))
        c.set_cast_shadow(bool(o["cast_shadow"]))
        n_f += 1

    for ch, orig in j.get("herbe_auto", {}).items():
        gt = charger(ch)
        if gt is None:
            manquants.append(ch)
            continue
        vars_ = list(gt.get_editor_property("grass_varieties") or [])
        for i, v in enumerate(vars_):
            if i >= len(orig):
                break
            o = orig[i]
            _pp_ecrire(v, "grass_density", float(o["density"]))
            _pp_ecrire(v, "start_cull_distance", int(o["start"]))
            _pp_ecrire(v, "end_cull_distance", int(o["end"]))
            v.set_editor_property("cast_dynamic_shadow", bool(o["shadow"]))
        gt.set_editor_property("grass_varieties", vars_)
        EAL.save_loaded_asset(gt, False)
        n_h += 1
    if n_h:
        try:
            unreal.SystemLibrary.execute_console_command(ELL.get_editor_world(), "grass.FlushCache")
        except Exception:
            pass

    ELL.save_all_dirty_levels()
    if manquants:
        unreal.log_warning("[SolCartoon] introuvables (autre carte ouverte ?) : %s" % manquants)
    else:
        os.replace(JOURNAL_JSON, JOURNAL_JSON + ".retire")
    return ("Remis comme avant : %d Landscape, %d dalle(s) de sol, %d groupe(s) de foliage, %d type(s) d'herbe.%s"
            % (n_l, n_t, n_f, n_h,
               "\n%d element(s) introuvables : ouvre la carte Rachid et relance." % len(manquants) if manquants else ""))


try:
    msg = main()
except Exception:
    msg = "ERREUR :\n" + traceback.format_exc()
    unreal.log_error(msg)
unreal.log("[SolCartoon] " + msg)
try:
    unreal.EditorDialog.show_message("Sol cartoon", msg, unreal.AppMsgType.OK)
except Exception:
    pass
try:
    os.makedirs(os.path.dirname(FICHIER_LOG), exist_ok=True)
    with open(FICHIER_LOG, "a", encoding="utf-8") as f:
        f.write("\n===== RETRAIT %s =====\n%s\n" % (datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"), msg))
except Exception:
    pass
