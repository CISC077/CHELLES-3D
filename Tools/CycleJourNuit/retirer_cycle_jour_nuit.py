# -*- coding: utf-8 -*-
"""
CHELLES 3D - Annule installer_cycle_jour_nuit.py dans le niveau OUVERT :
retire le ciel GoodSky pose, re-affiche l'ancien ciel, remet le soleil dans sa mobilite d'origine.
Les assets de /Game/Chelles/CycleJourNuit/ restent (on peut les supprimer a la main).
Lancer : File > Execute Python Script... > ce fichier, puis Ctrl+S.
"""
import unreal

TAG = "ChellesCycleJourNuit"
LABEL = "Chelles_CycleJourNuit"
ell = unreal.EditorLevelLibrary


def log(m):
    unreal.log_warning("[CycleJourNuit] " + m)


for a in ell.get_all_level_actors():
    tags = [str(t) for t in a.get_editor_property("tags")]
    if a.get_actor_label() == LABEL or (TAG + "_cree") in tags:
        log("Retire : " + a.get_actor_label())
        ell.destroy_actor(a)
        continue
    garder = []
    for t in tags:
        if t == TAG:
            a.set_actor_hidden_in_game(False)
            log("Re-affiche : " + a.get_actor_label())
        elif t.startswith(TAG + "_mobilite_"):
            nom = t[len(TAG + "_mobilite_"):].upper()
            mob = getattr(unreal.ComponentMobility, nom, None)
            comp = a.get_component_by_class(unreal.DirectionalLightComponent) or a.root_component
            if mob is not None:
                comp.set_mobility(mob)
                log("Soleil '%s' remis en %s." % (a.get_actor_label(), nom))
        else:
            garder.append(unreal.Name(t))
    if len(garder) != len(tags):
        a.set_editor_property("tags", garder)

log("TERMINE. Ctrl+S pour enregistrer le niveau.")
