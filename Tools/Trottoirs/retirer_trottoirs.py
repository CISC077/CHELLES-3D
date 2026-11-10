"""SCRIPT A LANCER DANS UNREAL 4.27 : File > Execute Python Script... > ce fichier.

Retire du niveau ouvert les trottoirs poses par ajouter_trottoirs.py (acteurs "Chelles_Trottoir_*")
et enregistre le niveau. Ne touche a rien d'autre. Les meshes restent dans /Game/Chelles/Trottoirs
(on peut supprimer ce dossier a la main dans le Content Browser si on n'en veut plus).
"""
import unreal

ell = unreal.EditorLevelLibrary
n = 0
for a in ell.get_all_level_actors():
    if a.get_actor_label().startswith("Chelles_Trottoir_"):
        a.destroy_actor()
        n += 1
if n and ell.save_current_level():
    unreal.log("Trottoirs retires : %d morceau(x). Niveau enregistre." % n)
else:
    unreal.log("Trottoirs retires : %d morceau(x)." % n)
