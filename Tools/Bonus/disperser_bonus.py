# -*- coding: utf-8 -*-
"""BONUS ET ARMES EPARPILLES au pied des murs dans la carte /Game/Maps/Rachid.

A lancer DANS L'EDITEUR (Unreal ouvert, carte Rachid) :
    File > Execute Python Script... > Tools/Bonus/disperser_bonus.py
ou double-clic sur DISPERSER_BONUS.bat (Unreal FERME : il l'ouvre, travaille, enregistre et referme).

Ce que fait le script :
  1. enregistre Rachid puis copie Rachid.umap dans Tools/Bonus/sauvegarde/ ;
  2. enleve les bonus poses par un passage precedent de ce script (etiquette ChellesBonusAjoute) ;
  3. cherche des endroits au niveau de la rue, juste devant le pied d'un mur de batiment ;
  4. y pose les bonus FPS_Game (vie, grenades, munitions) et les socles d'armes animees
     (Replicated_Factory_Rifle / Shotgun / Sniper / Laser / Launcher), tournes vers la rue ;
  5. enregistre Rachid.

Aucun Blueprint n'est modifie. Les bonus deja poses a la main ne sont pas touches.
Relancer le script = nouvelle disposition au hasard.
Pour annuler : retirer_bonus.py (ou RETIRER_BONUS.bat).
Journal : Tools/Bonus/bonus.log
"""
import math
import os
import random
import shutil
import time
import traceback

import unreal

# ------------------------------------------------------------------ reglages
CARTE = "/Game/Maps/Rachid"
SYS = "/Game/FPS_Game/Blueprints/Bonus/Replication/System/"
INTER = "/Game/FPS_Game/Blueprints/Bonus/Replication/Interactive/"

# Blueprint -> nombre a poser (mettre 0 pour ne pas en poser)
BONUS = [
    (SYS + "Replicated_Health", 10),          # vie (cube rouge)
    (SYS + "Replicated_GrenadePickup", 6),    # grenades (cube vert)
    (SYS + "Replicated_Ammo", 8),             # munitions
    (INTER + "Replicated_Factory_Rifle", 3),  # socles avec arme animee
    (INTER + "Replicated_Factory_Shotgun", 2),
    (INTER + "Replicated_Factory_Sniper", 2),
    (INTER + "Replicated_Factory_Laser", 2),
    (INTER + "Replicated_Factory_Launcher", 2),
]
# si un socle d'arme ci-dessus n'existe pas, on prend celui-ci a la place
SECOURS_ARME = SYS + "Replicated_WeaponFactory"

ESPACEMENT = 30.0       # metres minimum entre deux bonus (reduit tout seul si la place manque)
DISTANCE_MUR = 0.8      # metres entre le mur et le centre du bonus
PORTEE_MUR = 6.0        # on cherche un mur a moins de 6 m d'un point de la rue
HAUTEUR_MUR = 4.0       # un "mur" = objet d'au moins 4 m de haut (pas une voiture, un banc...)
LOIN_DU_JOUEUR = 3.0    # metres minimum entre un bonus et le depart du joueur
GRAINE = None           # mettre un nombre (ex. 7) pour retrouver toujours la meme disposition

TAG_AJOUTE = "ChellesBonusAjoute"
DOSSIER_OUTLINER = "Bonus"

ell = unreal.EditorLevelLibrary
eal = unreal.EditorAssetLibrary
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ICI = os.path.join(PROJET, "Tools", "Bonus")
JOURNAL = os.path.join(ICI, "bonus.log")


def log(msg):
    unreal.log("[Bonus] " + str(msg))
    try:
        os.makedirs(ICI, exist_ok=True)
        with open(JOURNAL, "a", encoding="utf-8") as f:
            f.write(time.strftime("%H:%M:%S ") + str(msg) + "\n")
    except Exception:
        pass


# ------------------------------------------------------------------ outils
def a_tag(acteur, tag):
    try:
        return any(str(t) == tag for t in acteur.get_editor_property("tags"))
    except Exception:
        return False


def ajouter_tag(acteur, tag):
    tags = list(acteur.get_editor_property("tags"))
    if not any(str(t) == tag for t in tags):
        tags.append(unreal.Name(tag))
        acteur.set_editor_property("tags", tags)


def nom_court(ch):
    return ch.rsplit("/", 1)[-1]


def lire_impact(hit):
    """(point, normale, acteur) d'un HitResult, selon la version de l'API."""
    try:
        return (hit.get_editor_property("impact_point"), hit.get_editor_property("impact_normal"),
                hit.get_editor_property("actor"))
    except Exception:
        pass
    t = hit.to_tuple()
    return t[5], t[7], t[9]


def tracer(monde, debut, fin, complexe=False):
    try:
        hit = unreal.SystemLibrary.line_trace_single(
            monde, debut, fin, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, complexe, [],
            unreal.DrawDebugTrace.NONE, True)
    except Exception:
        return None
    if hit is None or isinstance(hit, tuple):
        return None
    try:
        r = lire_impact(hit)
        if r[0] is None:
            return None
        return r
    except Exception:
        return None


MOTS_PAS_SOL = ("sky", "ciel", "neige", "snow", "cloud", "water", "eau", "tree", "arbre", "foliage")
MOTS_PAS_MUR = MOTS_PAS_SOL + ("landscape", "lamp", "lampadaire", "voiture", "vehic",
                               "fence", "barriere", "grillage", "panneau", "pickup", "replicated")


def texte_acteur(act):
    try:
        return (act.get_class().get_name() + " " + act.get_actor_label()).lower()
    except Exception:
        return ""


def est_mur(act, cache):
    """Un mur de batiment : grand objet fixe, pas un personnage, un arbre, une voiture..."""
    if act is None:
        return False
    cle = act.get_name()
    if cle in cache:
        return cache[cle]
    ok = True
    if isinstance(act, unreal.Pawn):
        ok = False
    else:
        t = texte_acteur(act)
        if any(m in t for m in MOTS_PAS_MUR):
            ok = False
        else:
            try:
                _o, e = act.get_actor_bounds(False)
                ok = e.z * 2.0 >= HAUTEUR_MUR * 100.0
            except Exception:
                ok = False
    cache[cle] = ok
    return ok


def zone_ville(acteurs):
    for a in acteurs:
        if isinstance(a, unreal.NavMeshBoundsVolume) and (a.get_actor_label() == "NavMesh_Chelles"
                                                          or a_tag(a, "ChellesNavMesh")):
            return a.get_actor_bounds(False)
    vols = [a for a in acteurs if isinstance(a, unreal.NavMeshBoundsVolume)]
    if vols:
        return vols[0].get_actor_bounds(False)
    # pas de NavMesh : boite autour des objets fixes
    pts = [a.get_actor_location() for a in acteurs if isinstance(a, unreal.StaticMeshActor)]
    if not pts:
        return None
    xs = sorted(p.x for p in pts)
    ys = sorted(p.y for p in pts)
    zs = sorted(p.z for p in pts)
    q = lambda l, f: l[int((len(l) - 1) * f)]
    mn = unreal.Vector(q(xs, 0.05), q(ys, 0.05), q(zs, 0.02) - 5000.0)
    mx = unreal.Vector(q(xs, 0.95), q(ys, 0.95), q(zs, 0.98) + 10000.0)
    return (unreal.Vector((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, (mn.z + mx.z) / 2),
            unreal.Vector((mx.x - mn.x) / 2, (mx.y - mn.y) / 2, (mx.z - mn.z) / 2))


def depart_joueur(acteurs):
    for a in acteurs:
        if isinstance(a, unreal.PlayerStart):
            return a.get_actor_location()
    return None


# ------------------------------------------------------------------ recherche des places
def points_de_rue(monde, centre, etendue, voulu):
    """Tirs de rayons verticaux sur toute la ville ; garde le niveau de la rue (pas les toits)."""
    haut_z = centre.z + etendue.z
    bas_z = centre.z - etendue.z
    impacts = []
    debut = time.time()
    essais = 0
    while len(impacts) < voulu and essais < voulu * 4 and time.time() - debut < 120:
        essais += 1
        x = random.uniform(centre.x - etendue.x, centre.x + etendue.x)
        y = random.uniform(centre.y - etendue.y, centre.y + etendue.y)
        r = tracer(monde, unreal.Vector(x, y, haut_z), unreal.Vector(x, y, bas_z))
        if r is None:
            continue
        p, n, act = r
        if n is None or n.z < 0.8:
            continue
        if act is not None:
            if isinstance(act, unreal.Pawn):
                continue
            if any(m in texte_acteur(act) for m in MOTS_PAS_SOL):
                continue
        impacts.append(p)
    log("   %d points de sol trouves (%d tirs)" % (len(impacts), essais))
    if not impacts:
        return [], 0.0
    zs = sorted(p.z for p in impacts)
    rue = zs[int(len(zs) * 0.3)]
    gardes = [p for p in impacts if p.z <= rue + 350.0]
    log("   niveau de la rue ~ %.1f m, %d points gardes (toits exclus)" % (rue / 100.0, len(gardes)))
    return gardes, rue


DIRECTIONS = [(math.cos(math.radians(a)), math.sin(math.radians(a))) for a in range(0, 360, 30)]


def place_devant_mur(monde, p, cache):
    """Depuis un point de la rue, cherche le mur le plus proche ; rend (sol, yaw) devant ce mur ou None."""
    hauteur = p.z + 60.0
    meilleur = None
    for dx, dy in DIRECTIONS:
        fin = unreal.Vector(p.x + dx * PORTEE_MUR * 100.0, p.y + dy * PORTEE_MUR * 100.0, hauteur)
        r = tracer(monde, unreal.Vector(p.x, p.y, hauteur), fin, True)
        if r is None:
            continue
        q, n, act = r
        if n is None or abs(n.z) > 0.3 or not est_mur(act, cache):
            continue
        d = math.hypot(q.x - p.x, q.y - p.y)
        if meilleur is None or d < meilleur[0]:
            meilleur = (d, q, n)
    if meilleur is None:
        return None
    _d, q, n = meilleur
    lg = math.hypot(n.x, n.y) or 1.0
    nx, ny = n.x / lg, n.y / lg
    x = q.x + nx * DISTANCE_MUR * 100.0
    y = q.y + ny * DISTANCE_MUR * 100.0
    # le sol juste devant le mur, au meme niveau que la rue
    r = tracer(monde, unreal.Vector(x, y, p.z + 200.0), unreal.Vector(x, y, p.z - 300.0))
    if r is None:
        return None
    sol, ns, act = r
    if ns is None or ns.z < 0.8 or abs(sol.z - p.z) > 80.0:
        return None
    if act is not None and isinstance(act, unreal.Pawn):
        return None
    # de la place au-dessus (pas sous un escalier, un auvent tres bas...)
    if tracer(monde, unreal.Vector(x, y, sol.z + 10.0), unreal.Vector(x, y, sol.z + 180.0), True) is not None:
        return None
    yaw = math.degrees(math.atan2(ny, nx))  # tourne vers la rue
    return sol, yaw


def chercher_places(monde, nombre, centre, etendue, joueur):
    points, _rue = points_de_rue(monde, centre, etendue, nombre * 60)
    random.shuffle(points)
    cache = {}
    places = []
    debut = time.time()
    for p in points:
        if time.time() - debut > 180 or len(places) >= nombre * 15:
            break
        r = place_devant_mur(monde, p, cache)
        if r is not None:
            places.append(r)
    log("   %d places devant un mur trouvees" % len(places))

    def dist(a, b):
        return math.hypot(a.x - b.x, a.y - b.y) / 100.0

    if joueur is not None:
        places = [pl for pl in places if dist(pl[0], joueur) >= LOIN_DU_JOUEUR] or places
    espacement = ESPACEMENT
    while True:
        random.shuffle(places)
        choisis = []
        for pl in places:
            if all(dist(pl[0], c[0]) >= espacement for c in choisis):
                choisis.append(pl)
                if len(choisis) == nombre:
                    log("   espacement utilise : %.0f m" % espacement)
                    return choisis
        if espacement < 4:
            log("   espacement utilise : %.0f m (place limitee)" % espacement)
            return choisis
        espacement *= 0.8


# ------------------------------------------------------------------ pose
def bas_du_modele(acteur):
    """Hauteur du bas des modeles visibles (socle) de l'acteur, ou None."""
    bas = None
    for classe in (unreal.StaticMeshComponent, unreal.SkeletalMeshComponent):
        for c in acteur.get_components_by_class(classe):
            try:
                if c.get_editor_property("hidden_in_game") or not c.get_editor_property("visible"):
                    continue
                o, e, _r = unreal.SystemLibrary.get_component_bounds(c)
                z = o.z - e.z
                bas = z if bas is None else min(bas, z)
            except Exception:
                pass
    return bas


def poser(classe, nom, i, sol, yaw):
    a = ell.spawn_actor_from_class(classe, unreal.Vector(sol.x, sol.y, sol.z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    if a is None:
        return None
    bas = bas_du_modele(a)
    if bas is not None and abs(bas - sol.z) < 300.0:
        l = a.get_actor_location()
        a.set_actor_location(unreal.Vector(l.x, l.y, l.z + (sol.z - bas) + 2.0), False, True)
    a.set_actor_label("%s_Chelles_%02d" % (nom, i))
    ajouter_tag(a, TAG_AJOUTE)
    a.set_folder_path(DOSSIER_OUTLINER)
    return a


# ------------------------------------------------------------------ principal
def demander(texte):
    if os.environ.get("CHELLES_QUITTER") == "1":
        return True
    r = unreal.EditorDialog.show_message("Chelles 3D - Bonus", texte, unreal.AppMsgType.YES_NO)
    return r == unreal.AppReturnType.YES


def main():
    open(JOURNAL, "w").close()
    if GRAINE is not None:
        random.seed(GRAINE)
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        log("chargement de %s ..." % CARTE)
        ell.load_level(CARTE)
        monde = ell.get_editor_world()
    log("Carte : %s" % monde.get_path_name())

    # classes a poser
    classes = {}
    for ch, nb in BONUS:
        if nb <= 0:
            continue
        c = eal.load_blueprint_class(ch) if eal.does_asset_exist(ch) else None
        if c is None and "Factory" in ch and eal.does_asset_exist(SECOURS_ARME):
            log("   %s introuvable -> %s" % (nom_court(ch), nom_court(SECOURS_ARME)))
            c = eal.load_blueprint_class(SECOURS_ARME)
        if c is None:
            log("   ATTENTION : %s introuvable, ignore" % ch)
            continue
        classes[ch] = c
    a_poser = [ch for ch, nb in BONUS if ch in classes for _ in range(nb)]
    if not a_poser:
        log("ABANDON : aucun Blueprint de bonus trouve.")
        return

    acteurs = ell.get_all_level_actors()
    anciens = [a for a in acteurs if a_tag(a, TAG_AJOUTE)]
    zone = zone_ville(acteurs)
    if zone is None:
        log("ABANDON : impossible de trouver l'etendue de la ville.")
        return

    resume = "\n".join("- %s x%d" % (nom_court(ch), nb) for ch, nb in BONUS if ch in classes)
    if not demander("Le script va poser %d bonus au pied des murs de Rachid :\n%s\n\n"
                    "%s"
                    "Une copie de Rachid est faite avant. Continuer ?"
                    % (len(a_poser), resume,
                       ("Les %d bonus du passage precedent seront remplaces.\n\n" % len(anciens)) if anciens else "")):
        log("Annule par l'utilisateur. Rien n'a ete modifie.")
        return

    # ---- 1. sauvegarde
    ell.save_current_level()
    fichier = os.path.join(PROJET, "Content", "Maps", "Rachid.umap")
    dossier = os.path.join(ICI, "sauvegarde")
    os.makedirs(dossier, exist_ok=True)
    copie = os.path.join(dossier, "Rachid_%s.umap" % time.strftime("%Y-%m-%d_%H%M"))
    shutil.copy2(fichier, copie)
    log("1/5 sauvegarde : %s" % copie)

    # ---- 2. anciens bonus du script
    for a in anciens:
        ell.destroy_actor(a)
    log("2/5 %d bonus d'un passage precedent enleves" % len(anciens))

    # ---- 3. places
    centre, etendue = zone
    joueur = depart_joueur(ell.get_all_level_actors())
    log("3/5 recherche de places devant les murs (%.0f x %.0f m) ..." % (etendue.x / 50.0, etendue.y / 50.0))
    places = chercher_places(monde, len(a_poser), centre, etendue, joueur)
    if len(places) < len(a_poser):
        log("   ATTENTION : seulement %d places pour %d bonus. Envoyez Tools/Bonus/bonus.log." % (len(places), len(a_poser)))

    # ---- 4. pose (melange des sortes, les armes d'abord si la place manque)
    random.shuffle(a_poser)
    a_poser.sort(key=lambda ch: "Factory" not in ch)
    a_poser = a_poser[:len(places)]
    random.shuffle(a_poser)
    compte = {}
    for ch, (sol, yaw) in zip(a_poser, places):
        nom = nom_court(ch)
        compte[nom] = compte.get(nom, 0) + 1
        if poser(classes[ch], nom, compte[nom], sol, yaw) is None:
            log("   ERREUR : impossible de poser %s" % nom)
            compte[nom] -= 1
    log("4/5 poses : %s (dossier Outliner : %s)" %
        (", ".join("%s x%d" % kv for kv in sorted(compte.items())), DOSSIER_OUTLINER))

    # ---- 5. enregistrement
    if ell.save_current_level():
        log("5/5 Rachid enregistree.")
    else:
        log("5/5 ATTENTION : Rachid n'a pas pu etre enregistree. Faites File > Save Current.")
    log("TERMINE. Lancez Play pour voir les bonus. Annuler : Tools/Bonus/retirer_bonus.py")


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
