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
# CHELLES 3D - Horo de decor posees a la main dans Rachid (25/11/2026)
# 5 SkeletalMeshActor rue Gambetta ont l'animation ABP_Car_Hatchback_Horo : elle cherche une vraie voiture
# BP_Car_Hatchback_Horo et ecrit "Accessed None ... Car_Hatchback_horo" (208 fois par partie).
# MODE (variable d'environnement CHELLES_HORO_MODE, mise par REPARER_HORO_DECOR.bat) :
#   ANIM      = retire seulement l'animation de ces 5 (aspect identique, plus d'avertissement)
#   SUPPRIMER = retire de Rachid TOUTES les Horo de decor (SkeletalMeshActor du pack DV_Car_Horo),
#               les voitures garees du plugin (conduisibles) restent
import os
import unreal

PROJET = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
MODE = os.environ.get("CHELLES_HORO_MODE", "ANIM").upper()
CARTE = "/Game/Maps/Rachid"
ETAT = os.path.join(PROJET, "Tools", "VoituresHoro", "etat_decor.txt")


def log(texte):
    unreal.log("[Horo] " + texte)
    print("[Horo] " + texte)


try:
    if not unreal.EditorLevelLibrary.load_level(CARTE):
        raise RuntimeError("carte %s non chargee" % CARTE)
    anim, suppr, total = 0, 0, 0
    for a in unreal.EditorLevelLibrary.get_all_level_actors():
        if not isinstance(a, unreal.SkeletalMeshActor):
            continue
        comp = a.get_editor_property("skeletal_mesh_component")
        mesh = comp.get_editor_property("skeletal_mesh") if comp else None
        if mesh is None or not mesh.get_path_name().startswith("/Game/DV_Car_Horo/"):
            continue
        total += 1
        if MODE == "SUPPRIMER":
            log("supprime %s" % a.get_actor_label())
            unreal.EditorLevelLibrary.destroy_actor(a)
            suppr += 1
        elif comp.get_editor_property("anim_class") is not None:
            comp.set_editor_property("anim_class", None)
            comp.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
            a.modify()
            log("animation retiree de %s" % a.get_actor_label())
            anim += 1
    log("%d Horo de decor trouvees, %d sans animation maintenant, %d supprimees" % (total, anim, suppr))
    if anim or suppr:
        if not unreal.EditorLevelLibrary.save_current_level():
            raise RuntimeError("sauvegarde de Rachid refusee (Unreal ouvert ailleurs ?)")
    etat = "OK"
except Exception as e:
    etat = "ERREUR : %s" % e
    log(etat)
open(ETAT, "w").write(etat)
log("FIN : " + etat)
