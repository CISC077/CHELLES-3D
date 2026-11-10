"""SCRIPT A LANCER DANS UNREAL 4.27 : File > Execute Python Script... > ce fichier.
(la carte Rachid doit etre ouverte)

Enleve le volume "NavMesh_Chelles" pose par ajouter_navmesh.py.
S'il ne reste plus aucun Nav Mesh Bounds Volume, enleve aussi le RecastNavMesh
(les donnees du NavMesh). Les autres volumes eventuels ne sont pas touches.
Demande Oui / Non avant. Ctrl+Z annule. Ensuite : Ctrl+S.
Journal : Tools/out/navmesh.log
"""
import datetime
import os
import traceback

import unreal

PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
FICHIER_LOG = os.path.join(PROJET, "Tools", "out", "navmesh.log")
NOM_VOLUME = "NavMesh_Chelles"
ETIQUETTE = "ChellesNavMesh"


def journal(texte):
    unreal.log("[NavMesh] " + texte)
    try:
        os.makedirs(os.path.dirname(FICHIER_LOG), exist_ok=True)
        with open(FICHIER_LOG, "a", encoding="utf-8") as f:
            f.write(datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S ") + texte + "\n")
    except Exception:
        pass


def est_notre_volume(a):
    try:
        if a.get_actor_label() == NOM_VOLUME:
            return True
        return any(str(t) == ETIQUETTE for t in a.get_editor_property("tags"))
    except Exception:
        return False


def main():
    acteurs = unreal.EditorLevelLibrary.get_all_level_actors()
    volumes = [a for a in acteurs if isinstance(a, unreal.NavMeshBoundsVolume)]
    nos_volumes = [a for a in volumes if est_notre_volume(a)]
    if not nos_volumes:
        return "Aucun volume %s dans cette carte : rien a retirer." % NOM_VOLUME

    plus_aucun = len(nos_volumes) == len(volumes)
    recasts = [a for a in acteurs if isinstance(a, unreal.RecastNavMesh)] if plus_aucun else []

    texte = "Retirer %d volume(s) %s" % (len(nos_volumes), NOM_VOLUME)
    if recasts:
        texte += " et les donnees du NavMesh (%d RecastNavMesh)" % len(recasts)
    texte += " ?\n\nLes IA retrouveront leur comportement d'avant (pas de NavMesh)."
    if unreal.EditorDialog.show_message("NavMesh Chelles", texte, unreal.AppMsgType.YES_NO) != unreal.AppReturnType.YES:
        return "Annule : rien n'a ete modifie."

    with unreal.ScopedEditorTransaction("Retirer NavMesh Chelles"):
        for a in nos_volumes + recasts:
            journal("Retire : %s" % a.get_actor_label())
            unreal.EditorLevelLibrary.destroy_actor(a)

    return "NavMesh retire. Pense a Ctrl+S pour enregistrer la carte. (Ctrl+Z annule)"


try:
    resultat = main()
except Exception:
    resultat = "Erreur :\n" + traceback.format_exc()
journal(resultat)
unreal.EditorDialog.show_message("NavMesh Chelles", resultat, unreal.AppMsgType.OK)
