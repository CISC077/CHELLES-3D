"""SCRIPT A LANCER DANS UNREAL 4.27 : File > Execute Python Script... > ce fichier.
(la carte Rachid doit etre ouverte)

Ajoute UN "Nav Mesh Bounds Volume" qui couvre toute la zone jouable de Rachid,
pour que les IA (passants, soldats FPS_Game...) puissent calculer leurs chemins.

- Mesure la ville (batiments, routes, trottoirs, sol) en ignorant le ciel,
  la neige, les lumieres, les volumes, les personnages...
- Affiche les dimensions et demande Oui / Non avant de toucher a la carte.
- Le volume s'appelle "NavMesh_Chelles" (etiquette ChellesNavMesh) :
  relancer le script le remplace, retirer_navmesh.py l'enleve.
- Generation "Static" : le NavMesh est calcule une fois dans l'editeur et
  enregistre avec la carte -> rien a recalculer pendant le jeu (le plus leger).
- Ctrl+Z annule.

Ensuite : Build > Build Paths, attendre la fin, puis Ctrl+S.
Touche P dans la vue de l'editeur = voir le NavMesh (en vert).
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

MARGE_XY = 3000.0          # 30 m de marge autour de la ville
MARGE_BAS = 2000.0         # 20 m sous le point le plus bas
HAUTEUR_MAX = 30000.0      # le volume ne depasse pas 300 m de haut
TAILLE_MAX_ACTEUR = 2000000.0  # un objet de plus de 20 km (ciel, ocean...) est ignore

# Ce qui ne fait pas partie du "sol" de la ville
MOTS_IGNORES = ("sky", "ciel", "neige", "snow", "sun", "soleil", "cloud", "nuage",
                "fog", "brouillard", "spawner", "invasion", "zombie", "passant",
                "circulation", "niagara", "emitter", "particle", "camera", "light",
                # le Landscape et la vegetation peuvent s'etendre tres loin hors de la ville
                "landscape", "foliage")


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


def a_ignorer(a):
    """Vrai pour tout ce qui n'est pas du decor sur lequel on marche."""
    if isinstance(a, (unreal.Volume, unreal.Light, unreal.Pawn, unreal.Controller,
                      unreal.Info, unreal.Emitter, unreal.CameraActor)):
        return True
    nom = (a.get_class().get_name() + " " + a.get_actor_label()).lower()
    return any(m in nom for m in MOTS_IGNORES)


def centile(valeurs, p):
    v = sorted(valeurs)
    if not v:
        return 0.0
    i = int(round((len(v) - 1) * p))
    return v[max(0, min(len(v) - 1, i))]


def mesurer_ville(acteurs):
    boites = []
    for a in acteurs:
        if a_ignorer(a):
            continue
        try:
            origine, etendue = a.get_actor_bounds(True)  # seulement ce qui a des collisions
        except Exception:
            continue
        if etendue.x <= 1 and etendue.y <= 1:
            continue
        if max(etendue.x, etendue.y, etendue.z) > TAILLE_MAX_ACTEUR:
            continue
        boites.append((origine, etendue))

    if not boites:
        return None, 0

    # On ecarte les objets perdus tres loin (0,5 % de chaque cote).
    cx = [o.x for o, e in boites]
    cy = [o.y for o, e in boites]
    x_lo, x_hi = centile(cx, 0.005) - 20000, centile(cx, 0.995) + 20000
    y_lo, y_hi = centile(cy, 0.005) - 20000, centile(cy, 0.995) + 20000
    gardees = [(o, e) for o, e in boites if x_lo <= o.x <= x_hi and y_lo <= o.y <= y_hi]

    min_x = min(o.x - e.x for o, e in gardees) - MARGE_XY
    max_x = max(o.x + e.x for o, e in gardees) + MARGE_XY
    min_y = min(o.y - e.y for o, e in gardees) - MARGE_XY
    max_y = max(o.y + e.y for o, e in gardees) + MARGE_XY
    min_z = centile([o.z - e.z for o, e in gardees], 0.005) - MARGE_BAS
    max_z = centile([o.z + e.z for o, e in gardees], 0.995) + 1000
    max_z = min(max_z, min_z + HAUTEUR_MAX)
    return (min_x, max_x, min_y, max_y, min_z, max_z), len(gardees)


def etat_passants(acteurs):
    lignes = []
    for a in acteurs:
        if a.get_class().get_name() != "PassantsSpawner":
            continue
        trottoirs = None
        for nom in ("trottoirs_seulement", "bTrottoirsSeulement", "b_trottoirs_seulement"):
            try:
                trottoirs = bool(a.get_editor_property(nom))
                break
            except Exception:
                pass
        if trottoirs is None:
            lignes.append("  - %s : mode inconnu" % a.get_actor_label())
        elif trottoirs:
            lignes.append("  - %s : mode trottoirs (ne se sert pas du NavMesh, rien ne change)" % a.get_actor_label())
        else:
            lignes.append("  - %s : utilisera le NavMesh (contourne les murs)" % a.get_actor_label())
    return lignes


def regler_recast(acteurs):
    """Si le RecastNavMesh existe deja : generation Static (aucun calcul pendant le jeu)."""
    msg = []
    for a in acteurs:
        if isinstance(a, unreal.RecastNavMesh):
            try:
                if a.get_editor_property("runtime_generation") != unreal.RuntimeGenerationType.STATIC:
                    a.set_editor_property("runtime_generation", unreal.RuntimeGenerationType.STATIC)
                    msg.append("RecastNavMesh '%s' regle sur Static." % a.get_actor_label())
                else:
                    msg.append("RecastNavMesh '%s' deja sur Static." % a.get_actor_label())
            except Exception as e:
                msg.append("RecastNavMesh '%s' : reglage impossible (%s)." % (a.get_actor_label(), e))
    return msg


def main():
    monde = unreal.EditorLevelLibrary.get_editor_world()
    if monde is None:
        return "Aucune carte ouverte."
    nom_carte = monde.get_name()
    acteurs = unreal.EditorLevelLibrary.get_all_level_actors()
    journal("--- Carte %s : %d acteurs" % (nom_carte, len(acteurs)))

    anciens = [a for a in acteurs if isinstance(a, unreal.NavMeshBoundsVolume) and est_notre_volume(a)]
    autres = [a for a in acteurs if isinstance(a, unreal.NavMeshBoundsVolume) and not est_notre_volume(a)]

    zone, nb = mesurer_ville(acteurs)
    if zone is None:
        return "Je n'ai trouve aucun decor avec des collisions : rien fait."
    min_x, max_x, min_y, max_y, min_z, max_z = zone
    larg, prof, haut = (max_x - min_x) / 100.0, (max_y - min_y) / 100.0, (max_z - min_z) / 100.0
    journal("Zone : X %.0f..%.0f  Y %.0f..%.0f  Z %.0f..%.0f (%d objets)" % (min_x, max_x, min_y, max_y, min_z, max_z, nb))

    texte = ("Carte : %s\n\n"
             "Zone jouable mesuree sur %d objets :\n"
             "  %.0f m x %.0f m, hauteur %.0f m (%.2f km2)\n\n" % (nom_carte, nb, larg, prof, haut, larg * prof / 1e6))
    if anciens:
        texte += "Un volume %s existe deja : il sera remplace.\n" % NOM_VOLUME
    if autres:
        texte += ("Attention : %d autre(s) Nav Mesh Bounds Volume deja dans la carte "
                  "(je n'y touche pas).\n" % len(autres))
    passants = etat_passants(acteurs)
    if passants:
        texte += "\nPassants :\n" + "\n".join(passants) + "\n"
    if larg * prof > 16e6:
        texte += ("\nGrande zone : le calcul (Build Paths) peut prendre plusieurs minutes.\n")
    texte += ("\nZombies : pas de changement (ils sont guides par leur plugin).\n"
              "Soldats IA de FPS_Game : pourront enfin se deplacer.\n\n"
              "Ajouter le NavMesh sur toute cette zone ?")

    rep = unreal.EditorDialog.show_message("NavMesh Chelles", texte, unreal.AppMsgType.YES_NO)
    if rep != unreal.AppReturnType.YES:
        journal("Annule par l'utilisateur.")
        return "Annule : rien n'a ete modifie."

    centre = unreal.Vector((min_x + max_x) / 2.0, (min_y + max_y) / 2.0, (min_z + max_z) / 2.0)
    demi = unreal.Vector((max_x - min_x) / 2.0, (max_y - min_y) / 2.0, (max_z - min_z) / 2.0)

    with unreal.ScopedEditorTransaction("Ajouter NavMesh Chelles"):
        for a in anciens:
            journal("Ancien volume retire : %s" % a.get_actor_label())
            unreal.EditorLevelLibrary.destroy_actor(a)

        vol = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.NavMeshBoundsVolume, centre, unreal.Rotator(0, 0, 0))
        if vol is None:
            return "Echec : Unreal n'a pas pu creer le volume."
        vol.set_actor_label(NOM_VOLUME)
        vol.set_editor_property("tags", [unreal.Name(ETIQUETTE)])

        # Le volume nait en cube de 2 m : on mesure sa taille de base puis on l'agrandit.
        _, base = vol.get_actor_bounds(False)
        if base.x < 1 or base.y < 1 or base.z < 1:
            unreal.EditorLevelLibrary.destroy_actor(vol)
            return ("Echec : le volume a ete cree sans forme. Il faut le poser a la main "
                    "(Place Actors > Volumes > Nav Mesh Bounds Volume).")
        vol.set_actor_location(centre, False, False)
        vol.set_actor_scale3d(unreal.Vector(demi.x / base.x, demi.y / base.y, demi.z / base.z))

        # Previent le systeme de navigation que le volume a change de taille.
        try:
            nav = unreal.NavigationSystemV1.get_navigation_system(monde)
            if nav:
                nav.on_navigation_bounds_updated(vol)
        except Exception as e:
            journal("on_navigation_bounds_updated : %s" % e)

        reglages = regler_recast(acteurs)

    _, fin = vol.get_actor_bounds(False)
    journal("Volume pose : centre %s, demi-taille %s" % (centre, fin))
    for r in reglages:
        journal(r)

    return ("NavMesh ajoute : %.0f m x %.0f m.\n\n"
            "Maintenant :\n"
            "1. Menu Build > Build Paths, et attendre la fin (barre en bas a droite).\n"
            "2. Touche P dans la vue : le sol ou l'IA peut marcher devient vert.\n"
            "3. Ctrl+S pour enregistrer la carte Rachid.\n\n"
            "Ctrl+Z annule. Journal : Tools/out/navmesh.log" % (larg, prof))


try:
    resultat = main()
except Exception:
    resultat = "Erreur :\n" + traceback.format_exc()
journal(resultat)
unreal.EditorDialog.show_message("NavMesh Chelles", resultat, unreal.AppMsgType.OK)
