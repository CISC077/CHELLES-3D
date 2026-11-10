"""SCRIPT A LANCER DANS UNREAL 4.27 : File > Execute Python Script... > ce fichier.

Arrete la neige : enleve du niveau ouvert les acteurs "Chelles_Neige_*" (flocons, empreintes) poses par
ajouter_neige.py, rend au Landscape son ancien materiau, puis enregistre le niveau.
Les assets de /Game/Chelles/Neige restent (relancer ajouter_neige.py pour la remettre).
"""
import unreal

TAG_ANCIEN = "ChellesNeigeAncienSol="
ell = unreal.EditorLevelLibrary
acteurs = ell.get_all_level_actors()
anciens = [a for a in acteurs if a.get_actor_label().startswith("Chelles_Neige_")]
for a in anciens:
    ell.destroy_actor(a)
unreal.log("%d acteur(s) de neige retire(s)." % len(anciens))
sols = 0
for p in acteurs:
    if not isinstance(p, unreal.LandscapeProxy):
        continue
    tags = [str(t) for t in p.get_editor_property("tags")]
    note = [t for t in tags if t.startswith(TAG_ANCIEN)]
    if not note:
        continue
    chemin = note[0][len(TAG_ANCIEN):]
    p.set_editor_property("landscape_material", unreal.load_asset(chemin) if chemin else None)
    p.set_editor_property("tags", [unreal.Name(t) for t in tags if not t.startswith(TAG_ANCIEN)])
    unreal.log("Landscape '%s' : materiau d'origine remis (%s)." % (p.get_actor_label(), chemin or "aucun"))
    sols += 1
if anciens or sols:
    if ell.save_current_level():
        unreal.log("Niveau enregistre : la neige est arretee.")
    else:
        unreal.log("Faites Ctrl+S pour enregistrer le niveau.")
