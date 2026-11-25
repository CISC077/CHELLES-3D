# >>> SAUVEGARDE AUTO CHELLES 3D (pose par Tools\Sauvegarde\brancher_sauvegarde_auto.py, ne pas modifier)
try:
    import os as _sa_os, sys as _sa_sys
    _sa_d = _sa_os.path.dirname(_sa_os.path.abspath(globals().get('__file__') or '.'))
    _sa_m = None
    while _sa_d:
        if _sa_os.path.isfile(_sa_os.path.join(_sa_d, 'Tools', 'Sauvegarde', 'sauvegarde_auto.py')):
            _sa_m = _sa_os.path.join(_sa_d, 'Tools', 'Sauvegarde')
            break
        _sa_p = _sa_os.path.dirname(_sa_d)
        _sa_d = None if _sa_p == _sa_d else _sa_p
    if _sa_m is None:
        import unreal as _sa_u
        _sa_m = _sa_os.path.join(_sa_u.Paths.convert_relative_path_to_full(_sa_u.Paths.project_dir()), 'Tools', 'Sauvegarde')
    if _sa_m not in _sa_sys.path:
        _sa_sys.path.insert(0, _sa_m)
    import sauvegarde_auto as _sa_mod
    _sa_mod.avant(globals().get('__file__') or 'script')
except Exception as _sa_e:
    print('[SauvegardeAuto] non faite : %s' % _sa_e)
# <<< SAUVEGARDE AUTO CHELLES 3D
# CHELLES 3D - 25/11/2026 : les vehicules ne traversent plus les murs ni les batiments de Rachid.
# Les meshes de Rachid sans aucune collision (batiments, murs, routes, barrieres, mobilier...) recoivent
# la collision "complexe comme simple" (la forme exacte du mesh), modifiee dans le mesh lui-meme.
# Laisses sans collision expres : eau, arbres/buissons/plantes, rideaux, coulures, pietons, decorations,
# vehicules, armes, contenu /Engine. Les teleporteurs et JumpPads coupes sur place ne sont pas touches.
# Liste de ce qui est change : Tools\out\collisions_reparees.txt
import os
import unreal

PROJET = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SORTIE = os.path.join(PROJET, "Tools", "out", "collisions_reparees.txt")
ETAT = os.path.join(PROJET, "Tools", "Collisions", "etat.txt")
EXCLUS = ("/engine/", "eau", "water", "pour", "tree", "arbre", "bush", "vines", "plant", "foliage", "flower",
          "billboard", "garland", "curtain", "leaks", "pietons", "jumppad", "snowmobile", "/voitures/",
          "weapon", "machinegun", "snowman", "decoration", "/images/", "fakeinterior", "grass", "herbe")
lib = unreal.EditorStaticMeshLibrary


def log(t):
    unreal.log("[Collisions] " + t)


try:
    unreal.EditorLevelLibrary.load_level("/Game/Maps/Rachid")
    vus, changes = set(), []
    for a in unreal.EditorLevelLibrary.get_all_level_actors():
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            m = c.get_editor_property("static_mesh")
            if m is None:
                continue
            chemin = m.get_path_name()
            if chemin in vus:
                continue
            vus.add(chemin)
            if any(x in chemin.lower() for x in EXCLUS):
                continue
            if c.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION:
                continue
            bs = m.get_editor_property("body_setup")
            if bs is None or lib.get_simple_collision_count(m) > 0:
                continue
            if bs.get_editor_property("collision_trace_flag") == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE:
                continue
            bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            m.modify()
            unreal.EditorAssetLibrary.save_loaded_asset(m, False)
            changes.append(chemin)
    open(SORTIE, "w", encoding="utf-8").write("%d meshes avec collision ajoutee\n" % len(changes) + "\n".join(changes))
    log("%d meshes ont maintenant une collision (liste : %s)" % (len(changes), SORTIE))
    etat = "OK"
except Exception as e:
    etat = "ERREUR : %s" % e
    log(etat)
open(ETAT, "w").write(etat)
log("FIN : " + etat)
