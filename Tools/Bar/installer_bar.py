# -*- coding: utf-8 -*-
"""BAR AMERICAIN dans la carte /Game/Maps/Rachid (pack AmericanBar).

A lancer DANS L'EDITEUR (Unreal ouvert, carte Rachid) :
    File > Execute Python Script... > Tools/Bar/installer_bar.py
ou double-clic sur INSTALLER_LE_BAR.bat (Unreal FERME : il l'ouvre, travaille, enregistre et referme).

Ce que fait le script :
  1. enregistre Rachid puis copie Rachid.umap dans Tools/Bar/sauvegarde/ ;
  2. enleve le bar d'un passage precedent (etiquette ChellesBar) ;
  3. cherche, pres du depart du joueur, une place libre et plate de 14 x 10 m collee a un mur de batiment
     (pas sur la route, rien au-dessus) ;
  4. y construit une salle en briques avec porte et vitrines sur la rue, enseigne lumineuse "CHELLES BAR",
     et la meuble avec le pack AmericanBar : comptoir, etageres a bouteilles, tabourets, 2 billards avec
     boules et queues, tables, verres, lampes ;
  5. enregistre Rachid.

Aucun Blueprint ni asset du pack n'est modifie. Rien d'autre dans la carte n'est touche.
Relancer le script = le bar est reconstruit (au meme endroit si rien n'a change).
Pour annuler : retirer_bar.py (ou RETIRER_LE_BAR.bat).
Journal : Tools/Bar/bar.log
"""
import math
import os
import shutil
import time
import traceback

import unreal

# ------------------------------------------------------------------ reglages
CARTE = "/Game/Maps/Rachid"
PACK = "/Game/AmericanBar/"

# Taille de la salle (metres)
LARGEUR = 14.0     # le long de la rue
PROFONDEUR = 10.0  # de la porte au fond
HAUTEUR = 4.2

# Place choisie a la main : None = automatique. Sinon (X, Y, Z, Yaw) en cm/degres, copies depuis l'editeur :
# X Y Z = milieu de la porte au sol, Yaw = direction "de la porte vers le fond du bar".
POSITION_MANUELLE = None

# Le bar est cherche de preference entre ces distances du depart du joueur (metres)
DISTANCE_MIN = 25.0
DISTANCE_MAX = 400.0

# Si le comptoir est pose a l'envers (le cote serveur vers la salle), mettre True et relancer
TOURNER_COMPTOIR = False

ENSEIGNE = "CHELLES BAR"
TAG = "ChellesBar"
TAG_COMPTOIR = "ChellesBarComptoir"
DOSSIER_OUTLINER = "Bar"

ell = unreal.EditorLevelLibrary
eal = unreal.EditorAssetLibrary
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ICI = os.path.join(PROJET, "Tools", "Bar")
JOURNAL = os.path.join(ICI, "bar.log")


def log(msg):
    unreal.log("[Bar] " + str(msg))
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


def lire_impact(hit):
    try:
        return (hit.get_editor_property("impact_point"), hit.get_editor_property("impact_normal"),
                hit.get_editor_property("actor"))
    except Exception:
        pass
    t = hit.to_tuple()
    return t[5], t[7], t[9]


def tracer(monde, debut, fin, complexe=False, ignores=None):
    try:
        hit = unreal.SystemLibrary.line_trace_single(
            monde, debut, fin, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, complexe, ignores or [],
            unreal.DrawDebugTrace.NONE, True)
    except Exception:
        return None
    if hit is None or isinstance(hit, tuple):
        return None
    try:
        r = lire_impact(hit)
        return None if r[0] is None else r
    except Exception:
        return None


def texte_acteur(act):
    try:
        return (act.get_class().get_name() + " " + act.get_actor_label()).lower()
    except Exception:
        return ""


MOTS_PAS_SOL = ("sky", "ciel", "neige", "snow", "cloud", "water", "eau", "tree", "arbre", "foliage")
MOTS_ROUTE = ("road", "route", "rue", "street", "asphalt", "bitume", "trottoir", "sidewalk", "spline",
              "carrefour", "rondpoint", "crossing", "passage", "rail", "tram", "train", "bus")
MOTS_PAS_MUR = MOTS_PAS_SOL + ("landscape", "lamp", "lampadaire", "voiture", "vehic", "fence", "barriere",
                               "grillage", "panneau", "pickup", "replicated", "factory", "ratelier")


def est_mur(act, cache):
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
                ok = e.z * 2.0 >= 400.0 and max(e.x, e.y) * 2.0 >= 800.0
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


# ------------------------------------------------------------------ repere du bar
class Repere(object):
    """x = de la porte vers le fond, y = le long de la facade (gauche/droite), z = au-dessus du sol du bar."""

    def __init__(self, origine, yaw):
        self.o = origine
        self.yaw = yaw
        r = math.radians(yaw)
        self.ux = (math.cos(r), math.sin(r))
        self.uy = (-math.sin(r), math.cos(r))

    def monde(self, x, y, z=0.0):
        return unreal.Vector(self.o.x + x * self.ux[0] + y * self.uy[0],
                             self.o.y + x * self.ux[1] + y * self.uy[1],
                             self.o.z + z)


# ------------------------------------------------------------------ recherche de la place
def sol_ok(monde, rp, x, y, base_z, cache_route):
    """Sol plat au niveau base_z, rien au-dessus jusqu'a 9 m, pas une route."""
    p = rp.monde(x, y, 0.0)
    r = tracer(monde, unreal.Vector(p.x, p.y, base_z + 900.0), unreal.Vector(p.x, p.y, base_z - 250.0), True)
    if r is None:
        return False
    q, n, act = r
    if n is None or n.z < 0.85 or abs(q.z - base_z) > 50.0:
        return False
    if act is not None:
        if isinstance(act, unreal.Pawn):
            return False
        cle = act.get_name()
        if cle not in cache_route:
            t = texte_acteur(act)
            cache_route[cle] = any(m in t for m in MOTS_ROUTE) or any(m in t for m in MOTS_PAS_SOL)
        if cache_route[cle]:
            return False
    return True


def place_libre(monde, rp, base_z, cache_route):
    L, P = LARGEUR * 100.0, PROFONDEUR * 100.0
    # tout l'interieur + 1 m de marge sur les cotes, et 2 m de sol devant la porte
    xs = [-200.0 + i * (P + 200.0) / 8.0 for i in range(9)]
    ys = [-L / 2 - 50.0 + j * (L + 100.0) / 10.0 for j in range(11)]
    for x in xs:
        for y in ys:
            if not sol_ok(monde, rp, x, y, base_z, cache_route):
                return False
    # rien de bas a l'interieur (bancs, voitures, poteaux) : rayons horizontaux a 1 m et 2,5 m
    for z in (100.0, 250.0):
        for j in range(7):
            y = -L / 2 + 50.0 + j * (L - 100.0) / 6.0
            a = rp.monde(-50.0, y, z)
            b = rp.monde(P - 40.0, y, z)
            if tracer(monde, a, b, True) is not None:
                return False
    return True


DIRECTIONS = [(math.cos(math.radians(a)), math.sin(math.radians(a))) for a in range(0, 360, 20)]


def candidats(monde, centre, etendue, joueur):
    """Points au niveau de la rue, tries par distance au depart du joueur."""
    import random
    haut_z = centre.z + etendue.z
    bas_z = centre.z - etendue.z
    pts = []
    debut = time.time()
    essais = 0
    while len(pts) < 2500 and essais < 9000 and time.time() - debut < 120:
        essais += 1
        if joueur is not None and essais % 3 != 0:
            a = random.uniform(0.0, 2.0 * math.pi)
            d = random.uniform(DISTANCE_MIN, DISTANCE_MAX) * 100.0
            x, y = joueur.x + math.cos(a) * d, joueur.y + math.sin(a) * d
        else:
            x = random.uniform(centre.x - etendue.x, centre.x + etendue.x)
            y = random.uniform(centre.y - etendue.y, centre.y + etendue.y)
        r = tracer(monde, unreal.Vector(x, y, haut_z), unreal.Vector(x, y, bas_z))
        if r is None:
            continue
        p, n, act = r
        if n is None or n.z < 0.85 or (act is not None and isinstance(act, unreal.Pawn)):
            continue
        pts.append(p)
    if not pts:
        return [], 0.0
    zs = sorted(p.z for p in pts)
    rue = zs[int(len(zs) * 0.3)]
    pts = [p for p in pts if p.z <= rue + 300.0]
    if joueur is not None:
        pts.sort(key=lambda p: abs(math.hypot(p.x - joueur.x, p.y - joueur.y) / 100.0 - DISTANCE_MIN))
    log("   %d points de sol au niveau de la rue (%d tirs)" % (len(pts), essais))
    return pts, rue


def chercher_place(monde, zone, joueur):
    centre, etendue = zone
    pts, _rue = candidats(monde, centre, etendue, joueur)
    cache_mur, cache_route = {}, {}
    debut = time.time()
    testes = 0
    for p in pts:
        if time.time() - debut > 240:
            break
        h = p.z + 150.0
        meilleur = None
        for dx, dy in DIRECTIONS:
            fin = unreal.Vector(p.x + dx * 1200.0, p.y + dy * 1200.0, h)
            r = tracer(monde, unreal.Vector(p.x, p.y, h), fin, True)
            if r is None:
                continue
            q, n, act = r
            if n is None or abs(n.z) > 0.3 or not est_mur(act, cache_mur):
                continue
            d = math.hypot(q.x - p.x, q.y - p.y)
            if meilleur is None or d < meilleur[0]:
                meilleur = (d, q, n)
        if meilleur is None:
            continue
        testes += 1
        _d, q, n = meilleur
        lg = math.hypot(n.x, n.y) or 1.0
        nx, ny = n.x / lg, n.y / lg
        # fond du bar a 30 cm du mur, porte vers la rue
        recul = 30.0 + PROFONDEUR * 100.0
        origine = unreal.Vector(q.x + nx * recul, q.y + ny * recul, p.z)
        yaw = math.degrees(math.atan2(-ny, -nx))
        rp = Repere(origine, yaw)
        # le sol a la porte
        r = tracer(monde, unreal.Vector(origine.x, origine.y, p.z + 400.0), unreal.Vector(origine.x, origine.y, p.z - 300.0), True)
        if r is None:
            continue
        base_z = r[0].z
        rp.o = unreal.Vector(origine.x, origine.y, base_z)
        if place_libre(monde, rp, base_z, cache_route):
            log("   place trouvee apres %d murs testes" % testes)
            return rp
    log("   %d murs testes, aucune place libre de %.0f x %.0f m" % (testes, LARGEUR, PROFONDEUR))
    return None


# ------------------------------------------------------------------ construction
class Chantier(object):
    def __init__(self, rp):
        self.rp = rp
        self.acteurs = []
        self.cube = eal.load_asset("/Engine/BasicShapes/Cube.Cube")
        self.manquants = set()
        self.cache = {}

    # ---- assets
    def mesh(self, nom):
        if nom in self.cache:
            return self.cache[nom]
        m = None
        for dossier in ("props_ball", "props_bottle", "props_counter", "props_cue", "props_door", "props_lamp",
                        "props_pool", "props_restroom", "props_roof", "props_shelf", "props_stool", "props_table",
                        "props_wall", "props_window", "props_wineDrink"):
            ch = PACK + dossier + "/" + nom
            if eal.does_asset_exist(ch):
                m = eal.load_asset(ch)
                break
        if m is None:
            self.manquants.add(nom)
        self.cache[nom] = m
        return m

    def materiau(self, chemin):
        if chemin in self.cache:
            return self.cache[chemin]
        m = eal.load_asset(chemin) if eal.does_asset_exist(chemin) else None
        self.cache[chemin] = m
        return m

    def finir(self, a, nom, collision=True, ombre=True, distance=0.0):
        a.set_actor_label("Bar_%s_%03d" % (nom, len(self.acteurs) + 1))
        ajouter_tag(a, TAG)
        a.set_folder_path(DOSSIER_OUTLINER)
        if not collision:
            a.set_actor_enable_collision(False)
        try:
            c = a.get_editor_property("static_mesh_component")
            c.set_mobility(unreal.ComponentMobility.STATIC)
            if not ombre:
                c.set_editor_property("cast_shadow", False)
            if distance > 0.0:
                c.set_cull_distance(distance)
        except Exception:
            pass
        self.acteurs.append(a)
        return a

    # ---- briques de la salle
    def bloc(self, cx, cy, cz, sx, sy, sz, mat, nom, collision=True):
        """Cube aligne sur le bar : centre (cx, cy, cz) et taille (sx, sy, sz) en cm, dans le repere du bar."""
        p = self.rp.monde(cx, cy, cz)
        a = ell.spawn_actor_from_object(self.cube, p, unreal.Rotator(roll=0.0, pitch=0.0, yaw=self.rp.yaw))
        if a is None:
            return None
        a.set_actor_scale3d(unreal.Vector(sx / 100.0, sy / 100.0, sz / 100.0))
        if mat is not None:
            a.get_editor_property("static_mesh_component").set_material(0, mat)
        return self.finir(a, nom, collision)

    def mur(self, axe, position, debut, fin, ouvertures, mat, epaisseur=20.0, tuile=140.0):
        """Mur en tuiles carrees (les briques gardent leurs proportions).
        axe 'y' : mur a x = position, de y = debut a fin ; axe 'x' : mur a y = position, de x = debut a fin.
        ouvertures : (a, b, z_bas, z_haut) le long du mur."""
        H = HAUTEUR * 100.0
        coupes = sorted(set([debut, fin] + [v for o in ouvertures for v in (o[0], o[1])]))
        for i in range(len(coupes) - 1):
            a, b = coupes[i], coupes[i + 1]
            milieu = (a + b) / 2.0
            pleins = [(-60.0, H)]
            for (oa, ob, zb, zh) in ouvertures:
                if oa <= milieu <= ob:
                    nouveaux = []
                    for (z0, z1) in pleins:
                        if zh <= z0 or zb >= z1:
                            nouveaux.append((z0, z1))
                            continue
                        if zb > z0:
                            nouveaux.append((z0, zb))
                        if zh < z1:
                            nouveaux.append((zh, z1))
                    pleins = nouveaux
            nu = max(1, int(round((b - a) / tuile)))
            for (z0, z1) in pleins:
                nz = max(1, int(round((z1 - z0) / tuile)))
                for iu in range(nu):
                    u0 = a + (b - a) * iu / nu
                    u1 = a + (b - a) * (iu + 1) / nu
                    for iz in range(nz):
                        w0 = z0 + (z1 - z0) * iz / nz
                        w1 = z0 + (z1 - z0) * (iz + 1) / nz
                        if axe == "y":
                            self.bloc(position, (u0 + u1) / 2, (w0 + w1) / 2, epaisseur, u1 - u0, w1 - w0, mat, "Mur")
                        else:
                            self.bloc((u0 + u1) / 2, position, (w0 + w1) / 2, u1 - u0, epaisseur, w1 - w0, mat, "Mur")

    def dalle(self, z, epaisseur, mat, nom, tuile=200.0, collision=True):
        L, P = LARGEUR * 100.0, PROFONDEUR * 100.0
        nx = max(1, int(round(P / tuile)))
        ny = max(1, int(round(L / tuile)))
        for i in range(nx):
            for j in range(ny):
                cx = P * (i + 0.5) / nx
                cy = -L / 2 + L * (j + 0.5) / ny
                self.bloc(cx, cy, z, P / nx, L / ny, epaisseur, mat, nom, collision)

    # ---- meubles du pack
    def objet(self, nom, x, y, z_bas, yaw=0.0, taille=None, collision=True, ombre=True, long_sur=None,
              distance=6000.0, centrer=True):
        """Pose le modele 'nom' : bas a z_bas, centre en (x, y). yaw dans le repere du bar.
        taille : largeur attendue (cm) du plus grand cote ; corrige un modele importe a la mauvaise echelle.
        long_sur 'x' ou 'y' : tourne le modele pour que son grand cote suive cet axe du bar."""
        m = self.mesh(nom)
        if m is None:
            return None
        try:
            e = m.get_bounds().box_extent
        except Exception:
            e = unreal.Vector(50, 50, 50)
        if long_sur is not None:
            long_x = e.x >= e.y
            if (long_sur == "x") != long_x:
                yaw += 90.0
        a = ell.spawn_actor_from_object(m, self.rp.monde(x, y, z_bas),
                                        unreal.Rotator(roll=0.0, pitch=0.0, yaw=self.rp.yaw + yaw))
        if a is None:
            return None
        if taille:
            grand = 2.0 * max(e.x, e.y)
            if grand > 1.0:
                k = taille / grand
                if k < 0.5 or k > 2.0:
                    a.set_actor_scale3d(unreal.Vector(k, k, k))
        self.caler(a, x, y, z_bas, centrer)
        return self.finir(a, nom.replace("_SM", ""), collision, ombre, distance)

    def caler(self, a, x, y, z_bas, centrer=True):
        o, e = a.get_actor_bounds(False)
        cible = self.rp.monde(x, y, 0.0)
        l = a.get_actor_location()
        dx = (cible.x - o.x) if centrer else 0.0
        dy = (cible.y - o.y) if centrer else 0.0
        dz = (self.rp.o.z + z_bas) - (o.z - e.z)
        a.set_actor_location(unreal.Vector(l.x + dx, l.y + dy, l.z + dz), False, True)

    def panneau(self, nom, x, y, z_bas, largeur, hauteur, sur_mur_x=True, yaw_plus=0.0, collision=False):
        """Porte / fenetre ajustee a une ouverture : le cote mince du modele suit l'epaisseur du mur."""
        m = self.mesh(nom)
        if m is None:
            return None
        e = m.get_bounds().box_extent
        mince_x = e.x <= e.y
        yaw = (0.0 if mince_x else 90.0) if sur_mur_x else (90.0 if mince_x else 0.0)
        yaw += yaw_plus
        a = ell.spawn_actor_from_object(m, self.rp.monde(x, y, z_bas),
                                        unreal.Rotator(roll=0.0, pitch=0.0, yaw=self.rp.yaw + yaw))
        if a is None:
            return None
        cote = max(e.x, e.y) * 2.0
        sl = largeur / cote if cote > 1.0 else 1.0
        sh = hauteur / (e.z * 2.0) if e.z > 0.5 else 1.0
        if mince_x:
            a.set_actor_scale3d(unreal.Vector(1.0, sl, sh))
        else:
            a.set_actor_scale3d(unreal.Vector(sl, 1.0, sh))
        self.caler(a, x, y, z_bas)
        return self.finir(a, nom.replace("_SM", ""), collision, False)

    def dessus(self, a, x, y, monde_):
        """Hauteur (repere du bar) du dessus de l'acteur a au point (x, y)."""
        p = self.rp.monde(x, y, 0.0)
        o, e = a.get_actor_bounds(False)
        r = tracer(monde_, unreal.Vector(p.x, p.y, o.z + e.z + 50.0), unreal.Vector(p.x, p.y, o.z - e.z - 10.0), True)
        if r is not None and r[2] == a:
            return r[0].z - self.rp.o.z
        return o.z + e.z - self.rp.o.z

    def lumiere(self, x, y, z, couleur, intensite=4000.0, rayon=800.0, nom="Lumiere"):
        a = ell.spawn_actor_from_class(unreal.PointLight, self.rp.monde(x, y, z), unreal.Rotator(0, 0, 0))
        if a is None:
            return None
        try:
            c = a.get_editor_property("point_light_component")
            c.set_mobility(unreal.ComponentMobility.MOVABLE)  # pas besoin de recalculer l'eclairage
            c.set_intensity(intensite)
            c.set_light_color(couleur)
            c.set_attenuation_radius(rayon)
            c.set_cast_shadows(False)
        except Exception:
            pass
        a.set_actor_label("Bar_%s_%03d" % (nom, len(self.acteurs) + 1))
        ajouter_tag(a, TAG)
        a.set_folder_path(DOSSIER_OUTLINER)
        self.acteurs.append(a)
        return a

    def enseigne(self, texte, x, y, z):
        a = ell.spawn_actor_from_class(unreal.TextRenderActor, self.rp.monde(x, y, z),
                                       unreal.Rotator(roll=0.0, pitch=0.0, yaw=self.rp.yaw + 180.0))
        if a is None:
            return None
        try:
            c = a.get_editor_property("text_render")
            c.set_text(texte)
            c.set_text_render_color(unreal.Color(r=255, g=60, b=170, a=255))
            c.set_world_size(70.0)
            try:
                c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
                c.set_vertical_alignment(unreal.VerticalTextAligment.EVRTA_TEXT_CENTER)
            except Exception:
                pass
        except Exception:
            pass
        a.set_actor_label("Bar_Enseigne")
        ajouter_tag(a, TAG)
        a.set_folder_path(DOSSIER_OUTLINER)
        self.acteurs.append(a)
        return a


def construire(monde, rp):
    ch = Chantier(rp)
    L, P, H = LARGEUR * 100.0, PROFONDEUR * 100.0, HAUTEUR * 100.0
    brique = ch.materiau(PACK + "materials/wall/M_WallBrick_Inst") or ch.materiau(PACK + "materials/wall/M_WallBrick")
    brique2 = ch.materiau(PACK + "materials/wall/M_WallBrick_Section_Inst") or brique
    plancher = ch.materiau(PACK + "materials/floor/M_Floor_Bar_Inst") or ch.materiau(PACK + "materials/floor/M_Floor_Bar")
    plafond = ch.materiau(PACK + "materials/bathroom/M_Roof_Inst") or ch.materiau(PACK + "materials/bathroom/M_Roof")
    marbre = ch.materiau(PACK + "materials/marble/M_BlackMarble_Inst")

    # ---- la salle
    porte_l, porte_h = 240.0, 270.0
    vitre_b, vitre_h = 100.0, 230.0
    vitres = [(-L / 2 + 150.0, -L / 2 + 370.0), (L / 2 - 370.0, L / 2 - 150.0)]
    ouvertures = [(-porte_l / 2, porte_l / 2, -100.0, porte_h)] + [(a, b, vitre_b, vitre_h) for a, b in vitres]
    ch.dalle(-2.0, 10.0, plancher, "Sol")
    ch.dalle(H + 7.5, 15.0, plafond, "Plafond")
    ch.mur("y", 0.0, -L / 2 - 10.0, L / 2 + 10.0, ouvertures, brique)               # facade sur la rue
    ch.mur("y", P, -L / 2 - 10.0, L / 2 + 10.0, [], brique2)                         # fond
    ch.mur("x", -L / 2, 10.0, P - 10.0, [], brique)                                  # cote gauche
    ch.mur("x", L / 2, 10.0, P - 10.0, [], brique)                                   # cote droit
    # bandeau sombre au-dessus de la porte (porte l'enseigne)
    ch.bloc(-16.0, 0.0, H - 40.0, 12.0, L * 0.6, 60.0, marbre, "Bandeau", False)

    # ---- porte d'entree (ouverte, sans collision) et vitrines
    ch.panneau("border_door_entrance", 0.0, 0.0, 0.0, porte_l + 20.0, porte_h + 10.0)
    for cote, nom in ((-1.0, "entrance_door_left_SM"), (1.0, "entrance_door_right_SM")):
        # battant ouvert vers l'interieur, le long du chambranle
        ch.panneau(nom, porte_l / 4.0 + 10.0, cote * (porte_l / 2.0 - 6.0), 0.0, porte_l / 2.0, porte_h - 10.0,
                   sur_mur_x=False)
    for a, b in vitres:
        milieu = (a + b) / 2.0
        ch.panneau("window_left_SM", 0.0, milieu - (b - a) / 4.0, vitre_b, (b - a) / 2.0, vitre_h - vitre_b, collision=True)
        ch.panneau("window_right_SM", 0.0, milieu + (b - a) / 4.0, vitre_b, (b - a) / 2.0, vitre_h - vitre_b, collision=True)

    # ---- comptoir le long du fond, etageres a bouteilles derriere
    x_comptoir = P - 260.0
    rot = 180.0 if TOURNER_COMPTOIR else 0.0
    comptoirs = []
    largeur_comptoir = 700.0
    morceaux = ["counter_bar_01_SM", "counter_bar_02_SM", "counter_bar_01_SM", "counter_bar_02_SM"]
    m = ch.mesh("counter_bar_01_SM")
    pas = 200.0
    if m is not None:
        e = m.get_bounds().box_extent
        pas = max(80.0, min(400.0, 2.0 * max(e.x, e.y)))
    n = max(2, min(6, int(round(largeur_comptoir / pas))))
    for i in range(n):
        y = -pas * (n - 1) / 2.0 + i * pas
        c = ch.objet(morceaux[i % len(morceaux)], x_comptoir, y, 0.0, rot, taille=200.0, long_sur="y")
        if c is not None:
            ajouter_tag(c, TAG_COMPTOIR)
            comptoirs.append(c)
    for i, y in enumerate((-300.0, -100.0, 100.0, 300.0)):
        ch.objet("Drink_shelf_SM" if i % 2 == 0 else "Shelf_glass_SM", P - 45.0, y, 0.0, 180.0, taille=200.0,
                 long_sur="y", ombre=False)
    # bouteilles et verres sur le comptoir
    bouteilles = ["Bottle_wine_SM", "Bottle_simple_SM", "Bottle_sparkling_SM"]
    verres = ["Glass_wine_SM", "cup_chopp", "Glass_cocktail_SM", "Glass_flutte_SM", "Glass_bordeaux_SM",
              "Glass_coupette_SM"]
    for i, c in enumerate(comptoirs):
        y = -pas * (n - 1) / 2.0 + i * pas
        haut = ch.dessus(c, x_comptoir, y, monde)
        ch.objet(bouteilles[i % 3], x_comptoir, y - 40.0, haut, 0.0, taille=10.0, collision=False, ombre=False,
                 distance=2500.0)
        ch.objet(verres[i % len(verres)], x_comptoir - 10.0, y + 30.0, haut, 0.0, taille=9.0, collision=False,
                 ombre=False, distance=2500.0)
    # tabourets devant le comptoir
    for i in range(6):
        y = -largeur_comptoir / 2.0 + 60.0 + i * (largeur_comptoir - 120.0) / 5.0
        ch.objet("StoolCounter_SM", x_comptoir - 120.0, y, 0.0, 0.0, taille=45.0, distance=5000.0)

    # ---- deux billards au milieu, lampes au-dessus, boules et queues
    for k, (nom, y) in enumerate((("Pool_green_SM", -330.0), ("Pool_red_SM", 330.0))):
        x = 400.0
        t = ch.objet(nom, x, y, 0.0, 0.0, taille=260.0, long_sur="x")
        if t is None:
            continue
        haut = ch.dessus(t, x, y, monde)
        lampe = ch.objet("Lamp_pool_SM", x, y, 0.0, 0.0, taille=180.0, collision=False, ombre=False, long_sur="x")
        if lampe is not None:
            o, e = lampe.get_actor_bounds(False)
            l = lampe.get_actor_location()
            lampe.set_actor_location(unreal.Vector(l.x, l.y, l.z + (rp.o.z + H - 5.0) - (o.z + e.z)), False, True)
        ch.lumiere(x, y, H - 90.0, unreal.LinearColor(1.0, 0.85, 0.6, 1.0), 6000.0, 500.0, "LampeBillard")
        # triangle de boules d'un cote, boule blanche de l'autre
        ordre = ["one", "nine", "two", "ten", "eight", "three", "eleven", "four", "twelve", "five", "thirteen",
                 "six", "fourteen", "seven", "fiveteen"]
        diam = 5.8
        idx = 0
        for rang in range(5):
            for j in range(rang + 1):
                if idx >= len(ordre):
                    break
                bx = x + 55.0 + rang * diam * 0.87
                by = y + (j - rang / 2.0) * diam
                ch.objet("Ball_%s_SM" % ordre[idx], bx, by, haut, 0.0, taille=diam, collision=False, ombre=False,
                         distance=2000.0)
                idx += 1
        ch.objet("Ball_white_SM", x - 60.0, y, haut, 0.0, taille=diam, collision=False, ombre=False, distance=2000.0)
        ch.objet("Cue_fire_SM" if k == 0 else "Cue_striped_SM", x, y + 35.0, haut, 0.0, taille=145.0,
                 collision=False, ombre=False, long_sur="x", distance=2500.0)

    # ---- petites tables pres des vitrines
    for a, b in vitres:
        ym = (a + b) / 2.0
        t = ch.objet("Table_Middle_SM", 160.0, ym, 0.0, 0.0, taille=80.0)
        if t is None:
            continue
        haut = ch.dessus(t, 160.0, ym, monde)
        for dy in (-70.0, 70.0):
            ch.objet("Stool_small_SM", 160.0, ym + dy, 0.0, 0.0, taille=40.0, distance=5000.0)
        ch.objet("cup_chopp", 150.0, ym - 12.0, haut, 0.0, taille=9.0, collision=False, ombre=False, distance=2500.0)
        ch.objet("Glass_wine_SM", 168.0, ym + 14.0, haut, 0.0, taille=8.0, collision=False, ombre=False,
                 distance=2500.0)
        ch.objet("Small_Lamp_SM", 172.0, ym - 2.0, haut, 0.0, taille=20.0, collision=False, ombre=False,
                 distance=3000.0)

    # ---- lampes murales, deco, toilettes au fond a gauche
    for x in (250.0, 650.0):
        for cote in (-1.0, 1.0):
            ch.objet("LampWall_SM", x, cote * (L / 2.0 - 18.0), 230.0, 90.0 * cote, taille=40.0, collision=False,
                     ombre=False, centrer=True)
    ch.objet("decoration_section", 450.0, L / 2.0 - 15.0, 120.0, -90.0, taille=250.0, collision=False, ombre=False)
    ch.panneau("door_bathroom_SM", P - 14.0, -L / 2.0 + 120.0, 0.0, 90.0, 210.0, collision=True)
    ch.objet("Lamp_alert_SM", P - 20.0, -L / 2.0 + 120.0, 225.0, 180.0, taille=30.0, collision=False, ombre=False)

    # ---- lumieres d'ambiance (sans ombres : peu couteuses)
    chaud = unreal.LinearColor(1.0, 0.7, 0.42, 1.0)
    ch.lumiere(P - 300.0, -200.0, H - 60.0, chaud, 5000.0, 700.0)
    ch.lumiere(P - 300.0, 200.0, H - 60.0, chaud, 5000.0, 700.0)
    ch.lumiere(160.0, 0.0, H - 80.0, chaud, 3500.0, 700.0)
    ch.lumiere(P - 60.0, 0.0, 120.0, unreal.LinearColor(0.2, 0.45, 1.0, 1.0), 2500.0, 450.0, "LumiereEtageres")

    # ---- enseigne sur la rue
    ch.enseigne(ENSEIGNE, -24.0, 0.0, H - 40.0)
    ch.lumiere(-120.0, 0.0, H - 40.0, unreal.LinearColor(1.0, 0.25, 0.7, 1.0), 3000.0, 600.0, "LumiereEnseigne")

    if ch.manquants:
        log("   modeles absents du pack (ignores) : %s" % ", ".join(sorted(ch.manquants)))
    return ch.acteurs


# ------------------------------------------------------------------ principal
def demander(texte):
    if os.environ.get("CHELLES_QUITTER") == "1":
        return True
    r = unreal.EditorDialog.show_message("Chelles 3D - Bar", texte, unreal.AppMsgType.YES_NO)
    return r == unreal.AppReturnType.YES


def main():
    open(JOURNAL, "w").close()
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        log("chargement de %s ..." % CARTE)
        ell.load_level(CARTE)
        monde = ell.get_editor_world()
    log("Carte : %s" % monde.get_path_name())
    if not eal.does_directory_exist(PACK.rstrip("/")):
        log("ABANDON : le pack %s n'est pas dans Content." % PACK)
        return

    acteurs = ell.get_all_level_actors()
    anciens = [a for a in acteurs if a_tag(a, TAG)]
    if not demander("Le script va construire un BAR AMERICAIN (%d x %d m) dans Rachid avec le pack AmericanBar.\n\n"
                    "%sUne copie de Rachid est faite avant. Continuer ?"
                    % (LARGEUR, PROFONDEUR,
                       ("Le bar du passage precedent (%d objets) sera remplace.\n\n" % len(anciens)) if anciens else "")):
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

    # ---- 2. ancien bar (on garde sa place si on reconstruit)
    ancienne_place = None
    for a in anciens:
        if a.get_actor_label() == "Bar_Enseigne":
            ancienne_place = a
    place_precedente = None
    try:
        if ancienne_place is not None:
            l = ancienne_place.get_actor_location()
            yaw = ancienne_place.get_actor_rotation().yaw - 180.0
            rp0 = Repere(unreal.Vector(0, 0, 0), yaw)
            # l'enseigne est a (-24, 0, H - 40) dans le repere du bar
            o = unreal.Vector(l.x + 24.0 * rp0.ux[0], l.y + 24.0 * rp0.ux[1], l.z - (HAUTEUR * 100.0 - 40.0))
            place_precedente = Repere(o, yaw)
    except Exception:
        place_precedente = None
    for a in anciens:
        ell.destroy_actor(a)
    log("2/5 %d objets d'un bar precedent enleves" % len(anciens))

    # ---- 3. place
    if POSITION_MANUELLE is not None:
        x, y, z, yaw = POSITION_MANUELLE
        rp = Repere(unreal.Vector(x, y, z), yaw)
        log("3/5 place choisie a la main : %.0f %.0f %.0f, yaw %.0f" % (x, y, z, yaw))
    elif place_precedente is not None:
        rp = place_precedente
        log("3/5 meme place que le bar precedent")
    else:
        zone = zone_ville(ell.get_all_level_actors())
        if zone is None:
            log("ABANDON : impossible de trouver l'etendue de la ville.")
            return
        joueur = depart_joueur(ell.get_all_level_actors())
        log("3/5 recherche d'une place libre de %.0f x %.0f m contre un mur ..." % (LARGEUR, PROFONDEUR))
        rp = chercher_place(monde, zone, joueur)
        if rp is None:
            log("ABANDON : pas de place libre trouvee. Choisissez une place a la main : voir LISEZMOI (POSITION_MANUELLE).")
            ell.save_current_level()
            return

    # ---- 4. construction
    poses = construire(monde, rp)
    log("4/5 bar construit : %d objets (dossier Outliner : %s)" % (len(poses), DOSSIER_OUTLINER))
    log("    porte du bar : X=%.0f Y=%.0f Z=%.0f (yaw %.0f)" % (rp.o.x, rp.o.y, rp.o.z, rp.yaw))
    joueur = depart_joueur(ell.get_all_level_actors())
    if joueur is not None:
        log("    a %.0f m du depart du joueur" % (math.hypot(rp.o.x - joueur.x, rp.o.y - joueur.y) / 100.0))

    # ---- 5. enregistrement
    if ell.save_current_level():
        log("5/5 Rachid enregistree.")
    else:
        log("5/5 ATTENTION : Rachid n'a pas pu etre enregistree. Faites File > Save Current.")
    log("TERMINE. Lancez Play et cherchez l'enseigne rose CHELLES BAR. Annuler : Tools/Bar/retirer_bar.py")


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
