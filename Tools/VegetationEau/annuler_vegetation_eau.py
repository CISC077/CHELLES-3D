"""ANNULER vegetation + riviere + lac (Tools/VegetationEau/vegetation_eau.py).

Lance par ANNULER_VEGETATION_EAU.bat (Unreal ferme), ou dans l'editeur : File > Execute Python Script.
  - enleve tout ce que le script a pose (tag ChellesVegEau) ;
  - re-affiche ce qu'il avait cache (anciens arbres CH_*, ancien lac rond, pierres) ;
  - enleve les LOD automatiques qu'il avait ajoutes aux modeles de plantes ;
  - supprime les modeles d'eau (Content/Chelles/VegetationEau).
Journal : Tools/VegetationEau/annuler_vegetation_eau.log
"""
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
import json, os, traceback
import unreal

CARTE = "/Game/Maps/Rachid"
TAG = "ChellesVegEau"
TAG_CACHE = "ChellesVegEauCache"
DOSSIER_ASSETS = "/Game/Chelles/VegetationEau"

ICI = os.path.join(unreal.SystemLibrary.get_project_directory(), "Tools", "VegetationEau")
JOURNAL = os.path.join(ICI, "annuler_vegetation_eau.log")
ETAT = os.path.join(ICI, "journal_vegetation_eau.json")


def log(msg):
    unreal.log("[VegEau annuler] " + str(msg))
    with open(JOURNAL, "a", encoding="utf-8") as f:
        f.write(str(msg) + "\n")


def main():
    os.makedirs(ICI, exist_ok=True)
    open(JOURNAL, "w", encoding="utf-8").write("")
    ell = unreal.EditorLevelLibrary
    eal = unreal.EditorAssetLibrary
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        ell.load_level(CARTE)
    log("Carte : %s" % ell.get_editor_world().get_path_name())
    enleves = remis = 0
    for a in ell.get_all_level_actors():
        tags = [str(t) for t in a.tags]
        if TAG in tags:
            a.destroy_actor()
            enleves += 1
        elif TAG_CACHE in tags:
            if a.root_component:
                a.root_component.set_visibility(True, True)
            a.set_actor_enable_collision(True)
            a.tags = [t for t in a.tags if str(t) != TAG_CACHE]
            remis += 1
    log("%d objet(s) enleve(s), %d objet(s) re-affiche(s)" % (enleves, remis))
    if ell.save_current_level():
        log("map Rachid enregistree")
    else:
        log("ATTENTION : la map n'a pas pu etre enregistree (Ctrl+S dans Unreal)")

    lods = 0
    if os.path.exists(ETAT):
        etat = json.load(open(ETAT, encoding="utf-8"))
        for chemin in etat.get("lod", []):
            try:
                m = unreal.load_asset(chemin)
                if m and unreal.EditorStaticMeshLibrary.remove_lods(m):
                    eal.save_loaded_asset(m)
                    lods += 1
            except Exception as e:
                log("   LOD %s : %s" % (chemin, e))
        etat["lod"] = []
        json.dump(etat, open(ETAT, "w", encoding="utf-8"), indent=1)
    log("LOD automatiques retires de %d modele(s)" % lods)
    if eal.does_directory_exist(DOSSIER_ASSETS):
        eal.delete_directory(DOSSIER_ASSETS)
        log("dossier %s supprime" % DOSSIER_ASSETS)
    log("TERMINE : la map est revenue comme avant la vegetation / riviere / lac.")


def _quitter_si_bat():
    """lance par le .bat : l'editeur se ferme tout seul a la fin"""
    import unreal as _u
    if os.environ.get("CHELLES_QUITTER") == "1":
        try:
            _u.SystemLibrary.quit_editor()
        except Exception:
            pass


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())
_quitter_si_bat()
