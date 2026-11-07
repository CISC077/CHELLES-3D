"""SCRIPT A LANCER DANS UNREAL 4.27 : File > Execute Python Script... > ce fichier.

Arrete la neige : enleve du niveau ouvert les acteurs "Chelles_Neige_*" poses par ajouter_neige.py,
puis enregistre le niveau. Les assets de /Game/Chelles/Neige restent (relancer ajouter_neige.py pour la remettre).
"""
import unreal

ell = unreal.EditorLevelLibrary
anciens = [a for a in ell.get_all_level_actors() if a.get_actor_label().startswith("Chelles_Neige_")]
for a in anciens:
    ell.destroy_actor(a)
unreal.log("%d acteur(s) de neige retire(s)." % len(anciens))
if anciens:
    if ell.save_current_level():
        unreal.log("Niveau enregistre : la neige est arretee.")
    else:
        unreal.log("Faites Ctrl+S pour enregistrer le niveau.")
