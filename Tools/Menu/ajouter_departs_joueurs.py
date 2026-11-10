# -*- coding: utf-8 -*-
# CHELLES 3D - Ajoute 8 points de depart (PlayerStart) dans la carte Rachid.
# Sans eux, en multijoueur le joueur apparait en 0,0,0 : sous la carte.
#
# Mode d'emploi :
#  1. Ouvre la carte Rachid dans Unreal.
#  2. Place la vue de l'editeur la ou les joueurs doivent apparaitre,
#     un peu au-dessus du sol (2 ou 3 metres), dans un endroit degage.
#  3. File > Execute Python Script... > choisis ce fichier.
# Les 8 departs sont poses en cercle (2,5 m) autour de la vue, puis la carte est enregistree.
# Relancer le script remplace les anciens departs (ceux nommes Depart_Joueur_...).
import math
import unreal

TAG = "ChellesDepart"
monde = unreal.EditorLevelLibrary.get_editor_world()
if not monde or "Rachid" not in monde.get_name():
    unreal.log_error("[Departs] Ouvre d'abord la carte Rachid, puis relance ce script.")
else:
    position, rotation = unreal.EditorLevelLibrary.get_level_viewport_camera_info()

    # On enleve les departs poses par une version precedente de ce script
    for acteur in unreal.EditorLevelLibrary.get_all_level_actors():
        if isinstance(acteur, unreal.PlayerStart) and TAG in [str(t) for t in acteur.get_editor_property("tags")]:
            unreal.EditorLevelLibrary.destroy_actor(acteur)

    for i in range(8):
        angle = math.radians(i * 45.0)
        point = unreal.Vector(position.x + 250.0 * math.cos(angle),
                              position.y + 250.0 * math.sin(angle),
                              position.z)
        depart = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.PlayerStart, point, unreal.Rotator(roll=0.0, pitch=0.0, yaw=rotation.yaw))
        if depart:
            depart.set_editor_property("tags", [unreal.Name(TAG)])
            depart.set_actor_label("Depart_Joueur_%d" % (i + 1))

    unreal.EditorLevelLibrary.save_current_level()
    unreal.log_warning("[Departs] 8 departs de joueurs poses et carte Rachid enregistree. "
                       "Position : X=%.0f Y=%.0f Z=%.0f" % (position.x, position.y, position.z))
