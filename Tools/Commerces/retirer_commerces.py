# -*- coding: utf-8 -*-
"""Enleve la BOULANGERIE et le TABAC-PRESSE poses par installer_commerces.py (etiquette ChellesCommerce).
Le CHELLES BAR n'est pas touche. Journal : Tools/Commerces/commerces.log"""
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
import os
import time
import traceback

import unreal

TAG = "ChellesCommerce"
ell = unreal.EditorLevelLibrary
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
JOURNAL = os.path.join(PROJET, "Tools", "Commerces", "commerces.log")


def log(msg):
    unreal.log("[Commerces] " + str(msg))
    try:
        with open(JOURNAL, "a", encoding="utf-8") as f:
            f.write(time.strftime("%H:%M:%S ") + str(msg) + "\n")
    except Exception:
        pass


def main():
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        ell.load_level("/Game/Maps/Rachid")
    n = 0
    for a in ell.get_all_level_actors():
        try:
            if any(str(t) == TAG for t in a.get_editor_property("tags")):
                ell.destroy_actor(a)
                n += 1
        except Exception:
            pass
    ell.save_current_level()
    log("RETIRER : %d objets des commerces enleves, Rachid enregistree." % n)


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
