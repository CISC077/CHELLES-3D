# -*- coding: utf-8 -*-
"""COMMERCES DU QUARTIER dans la carte /Game/Maps/Rachid : BOULANGERIE et TABAC-PRESSE (pack MMSupermarket).

A lancer DANS L'EDITEUR (Unreal ouvert, carte Rachid) :
    File > Execute Python Script... > Tools/Commerces/installer_commerces.py
ou double-clic sur INSTALLER_COMMERCES.bat (Unreal FERME : il compile le plugin, ouvre Unreal, travaille,
enregistre et referme).

Ce que fait le script :
  1. enregistre Rachid puis copie Rachid.umap dans Tools/Commerces/sauvegarde/ ;
  2. enleve les commerces d'un passage precedent (etiquette ChellesCommerce) et garde leur place ;
  3. cherche, pres du CHELLES BAR (ou du depart du joueur si le bar n'est pas la), deux places libres
     et plates de 9 x 7 m collees a un mur de batiment (pas sur la route, rien au-dessus) ;
  4. y construit une BOULANGERIE et un TABAC-PRESSE ouverts : porte, vitrines, enseigne lumineuse,
     comptoir (etiquete ChellesComptoir_Boulangerie / ChellesComptoir_Tabac, c'est la que la boutique
     s'ouvre en jeu avec le plugin ChellesCommerces), etageres, pains, gateaux, caisse, frigo... ;
  5. enregistre Rachid.

Le bar (Tools/Bar) n'est pas touche : son comptoir (ChellesBarComptoir) sert deja de boutique.
Relancer le script = les commerces sont reconstruits au meme endroit.
Pour annuler : retirer_commerces.py (ou RETIRER_COMMERCES.bat).
Journal : Tools/Commerces/commerces.log
"""
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
import math
import os
import shutil
import time
import traceback

import unreal

# ------------------------------------------------------------------ reglages
CARTE = "/Game/Maps/Rachid"
PACK = "/Game/MMSupermarket/"
PACK_BAR = "/Game/AmericanBar/"

# Taille d'un commerce (metres)
LARGEUR = 9.0      # le long de la rue
PROFONDEUR = 7.0   # de la porte au fond
HAUTEUR = 3.8

# Les commerces sont cherches entre ces distances du CHELLES BAR (ou du depart du joueur), en metres
DISTANCE_MIN = 18.0
DISTANCE_MAX = 200.0

# Place choisie a la main : None = automatique. Sinon (X, Y, Z, Yaw) en cm/degres, copies depuis l'editeur :
# X Y Z = milieu de la porte au sol, Yaw = direction "de la porte vers le fond du magasin".
POSITION_BOULANGERIE = None
POSITION_TABAC = None

# Si un comptoir est pose a l'envers (le cote vendeur vers les clients), mettre True et relancer
TOURNER_COMPTOIR_BOULANGERIE = False
TOURNER_COMPTOIR_TABAC = False

TAG = "ChellesCommerce"
DOSSIER_OUTLINER = "Commerces"

ell = unreal.EditorLevelLibrary
eal = unreal.EditorAssetLibrary
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ICI = os.path.join(PROJET, "Tools", "Commerces")
JOURNAL = os.path.join(ICI, "commerces.log")


def log(msg):
    unreal.log("[Commerces] " + str(msg))
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
    def __init__(self, rp, prefixe):
        self.rp = rp
        self.prefixe = prefixe          # "Boulangerie" / "Tabac"
        self.acteurs = []
        self.cube = eal.load_asset("/Engine/BasicShapes/Cube.Cube")
        self.manquants = set()
        self.cache = {}

    # ---- assets
    def mesh(self, chemin):
        """chemin relatif au pack MMSupermarket (ex. 'Equipment/Mesh/SM_BreadShelf'), ou chemin /Game/... complet."""
        if chemin in self.cache:
            return self.cache[chemin]
        complet = chemin if chemin.startswith("/Game/") else PACK + chemin
        m = eal.load_asset(complet) if eal.does_asset_exist(complet) else None
        if m is None:
            self.manquants.add(chemin.split("/")[-1])
        self.cache[chemin] = m
        return m

    def materiau(self, chemin):
        if chemin in self.cache:
            return self.cache[chemin]
        m = eal.load_asset(chemin) if eal.does_asset_exist(chemin) else None
        self.cache[chemin] = m
        return m

    def finir(self, a, nom, collision=True, ombre=True, distance=0.0):
        a.set_actor_label("Commerce_%s_%s_%03d" % (self.prefixe, nom, len(self.acteurs) + 1))
        ajouter_tag(a, TAG)
        a.set_folder_path(DOSSIER_OUTLINER + "/" + self.prefixe)
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
        p = self.rp.monde(cx, cy, cz)
        a = ell.spawn_actor_from_object(self.cube, p, unreal.Rotator(roll=0.0, pitch=0.0, yaw=self.rp.yaw))
        if a is None:
            return None
        a.set_actor_scale3d(unreal.Vector(sx / 100.0, sy / 100.0, sz / 100.0))
        if mat is not None:
            a.get_editor_property("static_mesh_component").set_material(0, mat)
        return self.finir(a, nom, collision)

    def mur(self, axe, position, debut, fin, ouvertures, mat, epaisseur=20.0, tuile=140.0):
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
    def objet(self, chemin, x, y, z_bas, yaw=0.0, taille=None, collision=True, ombre=True, long_sur=None,
              distance=6000.0, centrer=True):
        """Pose le modele : bas a z_bas, centre en (x, y), yaw dans le repere du magasin.
        taille : largeur attendue (cm) du plus grand cote (corrige un modele a la mauvaise echelle).
        long_sur 'x' ou 'y' : tourne le modele pour que son grand cote suive cet axe."""
        m = self.mesh(chemin)
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
        return self.finir(a, chemin.split("/")[-1].replace("SM_", ""), collision, ombre, distance)

    def caler(self, a, x, y, z_bas, centrer=True):
        o, e = a.get_actor_bounds(False)
        cible = self.rp.monde(x, y, 0.0)
        l = a.get_actor_location()
        dx = (cible.x - o.x) if centrer else 0.0
        dy = (cible.y - o.y) if centrer else 0.0
        dz = (self.rp.o.z + z_bas) - (o.z - e.z)
        a.set_actor_location(unreal.Vector(l.x + dx, l.y + dy, l.z + dz), False, True)

    def largeur_le_long(self, a):
        """Largeur (cm) de l'acteur le long de la facade (axe y du magasin)."""
        o, e = a.get_actor_bounds(False)
        return 2.0 * (abs(e.x * self.rp.uy[0]) + abs(e.y * self.rp.uy[1]))

    def dessus(self, a, x, y, monde_):
        p = self.rp.monde(x, y, 0.0)
        o, e = a.get_actor_bounds(False)
        r = tracer(monde_, unreal.Vector(p.x, p.y, o.z + e.z + 50.0), unreal.Vector(p.x, p.y, o.z - e.z - 10.0), True)
        if r is not None and r[2] == a:
            return r[0].z - self.rp.o.z
        return o.z + e.z - self.rp.o.z

    def lumiere(self, x, y, z, couleur, intensite=4000.0, rayon=700.0, nom="Lumiere"):
        a = ell.spawn_actor_from_class(unreal.PointLight, self.rp.monde(x, y, z), unreal.Rotator(0, 0, 0))
        if a is None:
            return None
        try:
            c = a.get_editor_property("point_light_component")
            c.set_mobility(unreal.ComponentMobility.MOVABLE)
            c.set_intensity(intensite)
            c.set_light_color(couleur)
            c.set_attenuation_radius(rayon)
            c.set_cast_shadows(False)
        except Exception:
            pass
        a.set_actor_label("Commerce_%s_%s_%03d" % (self.prefixe, nom, len(self.acteurs) + 1))
        ajouter_tag(a, TAG)
        a.set_folder_path(DOSSIER_OUTLINER + "/" + self.prefixe)
        self.acteurs.append(a)
        return a

    def texte(self, texte, x, y, z, couleur, taille=60.0, vers_rue=True, label=None):
        yaw = self.rp.yaw + (180.0 if vers_rue else 0.0)
        a = ell.spawn_actor_from_class(unreal.TextRenderActor, self.rp.monde(x, y, z),
                                       unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        if a is None:
            return None
        try:
            c = a.get_editor_property("text_render")
            c.set_text(texte)
            c.set_text_render_color(couleur)
            c.set_world_size(taille)
            try:
                c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
                c.set_vertical_alignment(unreal.VerticalTextAligment.EVRTA_TEXT_CENTER)
            except Exception:
                pass
        except Exception:
            pass
        a.set_actor_label(label or ("Commerce_%s_Texte_%03d" % (self.prefixe, len(self.acteurs) + 1)))
        ajouter_tag(a, TAG)
        a.set_folder_path(DOSSIER_OUTLINER + "/" + self.prefixe)
        self.acteurs.append(a)
        return a


def salle(ch, monde):
    """Murs, sol, plafond, porte ouverte, vitrines. Renvoie (L, P, H)."""
    L, P, H = LARGEUR * 100.0, PROFONDEUR * 100.0, HAUTEUR * 100.0
    brique = ch.materiau(PACK_BAR + "materials/wall/M_WallBrick_Inst") or ch.materiau(PACK + "SMBuilding/Materials/MI_PerimeterWall_Inst")
    plancher = ch.materiau(PACK + "SMBuilding/Materials/MI_Marble_Inst")
    plafond = ch.materiau(PACK + "SMBuilding/Materials/MI_Ceiling_Inst")
    verre = ch.materiau(PACK + "SMBuilding/Materials/MM_Glass_01_Mat")
    sombre = ch.materiau(PACK_BAR + "materials/marble/M_BlackMarble_Inst")

    porte_l, porte_h = 220.0, 260.0
    vitre_b, vitre_h = 90.0, 230.0
    vitres = [(-L / 2 + 90.0, -porte_l / 2 - 60.0), (porte_l / 2 + 60.0, L / 2 - 90.0)]
    ouvertures = [(-porte_l / 2, porte_l / 2, -100.0, porte_h)] + [(a, b, vitre_b, vitre_h) for a, b in vitres]
    ch.dalle(-2.0, 10.0, plancher, "Sol")
    ch.dalle(H + 7.5, 15.0, plafond, "Plafond")
    ch.mur("y", 0.0, -L / 2 - 10.0, L / 2 + 10.0, ouvertures, brique)   # facade sur la rue
    ch.mur("y", P, -L / 2 - 10.0, L / 2 + 10.0, [], brique)             # fond
    ch.mur("x", -L / 2, 10.0, P - 10.0, [], brique)                     # cote gauche
    ch.mur("x", L / 2, 10.0, P - 10.0, [], brique)                      # cote droit
    # vitres (minces, avec collision) et bandeau sombre au-dessus de la porte
    for a, b in vitres:
        ch.bloc(0.0, (a + b) / 2.0, (vitre_b + vitre_h) / 2.0, 4.0, b - a, vitre_h - vitre_b, verre, "Vitre")
    ch.bloc(-16.0, 0.0, H - 40.0, 12.0, L * 0.75, 60.0, sombre, "Bandeau", False)
    return L, P, H


def comptoirs(ch, monde, chemins, x, largeur_voulue, tourner, tag):
    """Comptoir le long de la facade, a x du mur de facade. Renvoie la liste des morceaux poses."""
    poses = []
    m = ch.mesh(chemins[0])
    pas = 180.0
    if m is not None:
        e = m.get_bounds().box_extent
        pas = max(80.0, min(320.0, 2.0 * max(e.x, e.y)))
    n = max(1, min(4, int(round(largeur_voulue / pas))))
    rot = 180.0 if tourner else 0.0
    for i in range(n):
        y = -pas * (n - 1) / 2.0 + i * pas
        c = ch.objet(chemins[i % len(chemins)], x, y, 0.0, rot, taille=180.0, long_sur="y")
        if c is not None:
            ajouter_tag(c, tag)
            ajouter_tag(c, "ChellesComptoir")
            poses.append((c, y))
    return poses


def construire_boulangerie(monde, rp):
    ch = Chantier(rp, "Boulangerie")
    L, P, H = salle(ch, monde)
    x_comptoir = P - 260.0
    morceaux = comptoirs(ch, monde, ["Equipment/Mesh/SM_BakeryCounter01", "Equipment/Mesh/SM_BakeryCounter02"],
                         x_comptoir, 420.0, TOURNER_COMPTOIR_BOULANGERIE, "ChellesComptoir_Boulangerie")
    if not morceaux:
        # pack absent : un comptoir simple en cube pour que la boutique marche quand meme
        c = ch.bloc(x_comptoir, 0.0, 50.0, 70.0, 380.0, 100.0, ch.materiau(PACK_BAR + "materials/marble/M_BlackMarble_Inst"), "Comptoir")
        if c is not None:
            ajouter_tag(c, "ChellesComptoir_Boulangerie")
            ajouter_tag(c, "ChellesComptoir")
            morceaux = [(c, 0.0)]
    # caisse, gateaux et donuts sur le comptoir
    gourmandises = ["Products/Mesh/SM_Cake01", "Products/Mesh/SM_Donut01", "Products/Mesh/SM_Cake02",
                    "Products/Mesh/SM_Donut02", "Products/Mesh/SM_Cake03", "Products/Mesh/SM_Donut03"]
    for i, (c, y) in enumerate(morceaux):
        haut = ch.dessus(c, x_comptoir, y, monde)
        if i == 0:
            ch.objet("Equipment/Mesh/SM_CashRegister", x_comptoir, y - 40.0, haut, 180.0, taille=45.0,
                     collision=False, ombre=False, distance=3000.0)
        ch.objet(gourmandises[(2 * i) % len(gourmandises)], x_comptoir, y + 30.0, haut, 0.0, taille=28.0,
                 collision=False, ombre=False, distance=2500.0)
        ch.objet(gourmandises[(2 * i + 1) % len(gourmandises)], x_comptoir - 10.0, y + 70.0, haut, 0.0, taille=12.0,
                 collision=False, ombre=False, distance=2500.0)
    # etageres a pain au fond, baguettes, presentoir et tableau du menu
    for y in (-200.0, 200.0):
        ch.objet("Equipment/Mesh/SM_BreadShelf", P - 45.0, y, 0.0, 180.0, taille=180.0, long_sur="y", ombre=False)
    ch.objet("Products/Mesh/SM_Baguette_Stacked", P - 60.0, 0.0, 0.0, 0.0, taille=70.0, collision=False, ombre=False)
    ch.objet("Equipment/Mesh/SM_BakeryRack", 250.0, L / 2.0 - 60.0, 0.0, -90.0, taille=150.0, long_sur="x")
    ch.objet("Equipment/Mesh/SM_BakeryStand01", 200.0, -L / 2.0 + 70.0, 0.0, 90.0, taille=120.0, long_sur="x")
    ch.objet("Equipment/Mesh/SM_BakeryMenu", P - 15.0, 0.0, 240.0, 180.0, taille=150.0, collision=False, ombre=False)
    for i, x in enumerate((180.0, 420.0)):
        ch.objet("Props/Mesh/SM_BakeryPicture0%d" % (2 + i), x, -L / 2.0 + 14.0, 170.0, 90.0, taille=80.0,
                 collision=False, ombre=False)
        ch.objet("Props/Mesh/SM_BakeryPicture0%d" % (4 + i), x, L / 2.0 - 14.0, 170.0, -90.0, taille=80.0,
                 collision=False, ombre=False)
    # lumieres chaudes et enseigne doree
    chaud = unreal.LinearColor(1.0, 0.78, 0.5, 1.0)
    ch.lumiere(P - 260.0, 0.0, H - 50.0, chaud, 5000.0, 650.0)
    ch.lumiere(200.0, 0.0, H - 60.0, chaud, 3500.0, 600.0)
    ch.texte("BOULANGERIE", -24.0, 0.0, H - 40.0, unreal.Color(r=255, g=190, b=70, a=255), 62.0,
             label="Commerce_Boulangerie_Enseigne")
    ch.lumiere(-120.0, 0.0, H - 40.0, unreal.LinearColor(1.0, 0.7, 0.25, 1.0), 2500.0, 500.0, "LumiereEnseigne")
    # petit panneau au-dessus du comptoir (lu de l'interieur)
    ch.texte("BOUTIQUE : approche-toi du comptoir", x_comptoir - 40.0, 0.0, H - 70.0,
             unreal.Color(r=255, g=230, b=160, a=255), 16.0, vers_rue=True)
    return ch


def construire_tabac(monde, rp):
    ch = Chantier(rp, "Tabac")
    L, P, H = salle(ch, monde)
    x_comptoir = P - 260.0
    morceaux = comptoirs(ch, monde, ["Equipment/Mesh/SM_CheckoutCounter_01"], x_comptoir, 300.0,
                         TOURNER_COMPTOIR_TABAC, "ChellesComptoir_Tabac")
    if not morceaux:
        c = ch.bloc(x_comptoir, 0.0, 50.0, 70.0, 300.0, 100.0, ch.materiau(PACK_BAR + "materials/marble/M_BlackMarble_Inst"), "Comptoir")
        if c is not None:
            ajouter_tag(c, "ChellesComptoir_Tabac")
            ajouter_tag(c, "ChellesComptoir")
            morceaux = [(c, 0.0)]
    bonbons = ["Products/Mesh/SM_BCSChocolate1_Stacked", "Products/Mesh/SM_BCSChocolate2_Stacked",
               "Products/Mesh/SM_BCSChocolate3_Stacked"]
    for i, (c, y) in enumerate(morceaux):
        haut = ch.dessus(c, x_comptoir, y, monde)
        if i == 0:
            ch.objet("Equipment/Mesh/SM_CashRegister", x_comptoir, y - 50.0, haut, 180.0, taille=45.0,
                     collision=False, ombre=False, distance=3000.0)
            ch.objet("Equipment/Mesh/SM_POS", x_comptoir - 15.0, y + 10.0, haut, 180.0, taille=20.0,
                     collision=False, ombre=False, distance=2500.0)
        ch.objet(bonbons[i % len(bonbons)], x_comptoir, y + 60.0, haut, 0.0, taille=30.0,
                 collision=False, ombre=False, distance=2500.0)
    # etageres au fond (bonbons, cafe), frigo boissons, distributeur, presentoir a journaux
    for y in (-220.0, 220.0):
        ch.objet("Equipment/Mesh/SM_Shelf_01", P - 45.0, y, 0.0, 180.0, taille=180.0, long_sur="y", ombre=False)
    ch.objet("Equipment/Mesh/SM_Refrigerator_01", 330.0, L / 2.0 - 55.0, 0.0, -90.0, taille=120.0, long_sur="x")
    ch.objet("Equipment/Mesh/SM_RFVending_01", 160.0, L / 2.0 - 55.0, 0.0, -90.0, taille=100.0)
    ch.objet("Ads/Mesh/SM_Stand01", 180.0, -L / 2.0 + 70.0, 0.0, 90.0, taille=90.0)
    ch.objet("Products/Mesh/SM_TCCoffee1_Stacked", P - 60.0, 0.0, 0.0, 0.0, taille=45.0, collision=False, ombre=False)
    # lumieres blanches, enseigne rouge (la "carotte" des tabacs) et bleue
    blanc = unreal.LinearColor(0.95, 0.95, 1.0, 1.0)
    ch.lumiere(P - 260.0, 0.0, H - 50.0, blanc, 5000.0, 650.0)
    ch.lumiere(200.0, 0.0, H - 60.0, blanc, 3500.0, 600.0)
    ch.texte("TABAC - PRESSE", -24.0, 0.0, H - 40.0, unreal.Color(r=255, g=60, b=45, a=255), 56.0,
             label="Commerce_Tabac_Enseigne")
    rouge = ch.materiau(PACK + "SMBuilding/Materials/MI_SMSign_Inst")
    ch.bloc(-60.0, L / 2.0 - 60.0, H - 40.0, 20.0, 40.0, 90.0, rouge, "Carotte", False)
    ch.lumiere(-120.0, 0.0, H - 40.0, unreal.LinearColor(1.0, 0.2, 0.15, 1.0), 2500.0, 500.0, "LumiereEnseigne")
    ch.texte("BOUTIQUE : approche-toi du comptoir", x_comptoir - 40.0, 0.0, H - 70.0,
             unreal.Color(r=200, g=230, b=255, a=255), 16.0, vers_rue=True)
    return ch


# ------------------------------------------------------------------ principal
def demander(texte):
    if os.environ.get("CHELLES_QUITTER") == "1":
        return True
    r = unreal.EditorDialog.show_message("Chelles 3D - Commerces", texte, unreal.AppMsgType.YES_NO)
    return r == unreal.AppReturnType.YES


def repere_depuis_enseigne(a):
    """L'enseigne est a (-24, 0, H - 40) dans le repere du magasin, tournee vers la rue (yaw + 180)."""
    l = a.get_actor_location()
    yaw = a.get_actor_rotation().yaw - 180.0
    rp0 = Repere(unreal.Vector(0, 0, 0), yaw)
    o = unreal.Vector(l.x + 24.0 * rp0.ux[0], l.y + 24.0 * rp0.ux[1], l.z - (HAUTEUR * 100.0 - 40.0))
    return Repere(o, yaw)


def main():
    open(JOURNAL, "w").close()
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        log("chargement de %s ..." % CARTE)
        ell.load_level(CARTE)
        monde = ell.get_editor_world()
    log("Carte : %s" % monde.get_path_name())
    if not eal.does_directory_exist(PACK.rstrip("/")):
        log("ATTENTION : le pack %s n'est pas dans Content, les magasins seront presque vides." % PACK)

    acteurs = ell.get_all_level_actors()
    anciens = [a for a in acteurs if a_tag(a, TAG)]
    if not demander("Le script va construire une BOULANGERIE et un TABAC-PRESSE (%d x %d m) dans Rachid, "
                    "pres du CHELLES BAR.\n\n%sUne copie de Rachid est faite avant. Continuer ?"
                    % (LARGEUR, PROFONDEUR,
                       ("Les commerces du passage precedent (%d objets) seront remplaces.\n\n" % len(anciens)) if anciens else "")):
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

    # ---- 2. anciens commerces (on garde leur place)
    places = {}
    for a in anciens:
        lab = a.get_actor_label()
        for nom in ("Boulangerie", "Tabac"):
            if lab == "Commerce_%s_Enseigne" % nom:
                try:
                    places[nom] = repere_depuis_enseigne(a)
                except Exception:
                    pass
    for a in anciens:
        ell.destroy_actor(a)
    log("2/5 %d objets de commerces precedents enleves" % len(anciens))

    # ---- 3. point de depart : le bar, sinon le depart du joueur
    acteurs = ell.get_all_level_actors()
    centre = None
    for a in acteurs:
        if a.get_actor_label() == "Bar_Enseigne":
            centre = a.get_actor_location()
            log("3/5 CHELLES BAR trouve : les commerces seront cherches autour (%.0f %.0f)" % (centre.x, centre.y))
            break
    if centre is None:
        centre = depart_joueur(acteurs)
        log("3/5 pas de CHELLES BAR dans la carte (Tools\\Bar\\INSTALLER_LE_BAR.bat) : recherche autour du depart du joueur")
    zone = zone_ville(acteurs)

    manuel = {"Boulangerie": POSITION_BOULANGERIE, "Tabac": POSITION_TABAC}
    construits = 0
    for nom, fabrique in (("Boulangerie", construire_boulangerie), ("Tabac", construire_tabac)):
        if manuel[nom] is not None:
            x, y, z, yaw = manuel[nom]
            rp = Repere(unreal.Vector(x, y, z), yaw)
            log("   %s : place choisie a la main" % nom)
        elif nom in places:
            rp = places[nom]
            log("   %s : meme place que la fois precedente" % nom)
        else:
            if zone is None:
                log("ABANDON : impossible de trouver l'etendue de la ville.")
                break
            log("   %s : recherche d'une place libre de %.0f x %.0f m contre un mur ..." % (nom, LARGEUR, PROFONDEUR))
            rp = chercher_place(monde, zone, centre)
            if rp is None:
                log("   %s : pas de place libre. Mettez une place a la main (voir LISEZMOI, POSITION_%s)."
                    % (nom, nom.upper()))
                continue
        ch = fabrique(monde, rp)
        construits += 1
        log("4/5 %s construite : %d objets, porte X=%.0f Y=%.0f Z=%.0f (yaw %.0f)"
            % (nom, len(ch.acteurs), rp.o.x, rp.o.y, rp.o.z, rp.yaw))
        if ch.manquants:
            log("    modeles absents du pack (ignores) : %s" % ", ".join(sorted(ch.manquants)))

    # ---- 5. enregistrement
    if ell.save_current_level():
        log("5/5 Rachid enregistree (%d commerce(s) construit(s))." % construits)
    else:
        log("5/5 ATTENTION : Rachid n'a pas pu etre enregistree. Faites File > Save Current.")
    log("TERMINE. Lancez Play et cherchez les enseignes BOULANGERIE et TABAC - PRESSE pres du CHELLES BAR. "
        "Annuler : Tools/Commerces/RETIRER_COMMERCES.bat")


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
