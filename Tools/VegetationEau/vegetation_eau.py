"""VEGETATION + RIVIERE + LAC D'APRES OPENSTREETMAP - carte Rachid (Unreal 4.27)

Normalement lance par INSTALLER_VEGETATION_EAU.bat (double-clic, Unreal ferme) qui compile d'abord le
plugin ChellesVegetation. On peut aussi le lancer dans l'editeur (map Rachid ouverte) :
File > Execute Python Script... > Tools/VegetationEau/vegetation_eau.py  (le plugin doit etre compile).

Ce que fait le script :
  1. telecharge les donnees vegetation d'OpenStreetMap du quartier (arbres, alignements, haies, parcs,
     jardins, bois...) ; meme carre de 1 km et meme calage que les routes (Tools/Routes) ;
  2. "regarde" la map vue du ciel tous les 5 m (batiments, routes, decor deja pose) ;
  3. trace une RIVIERE qui traverse toute la map d'un bord a l'autre en evitant le centre-ville
     (300 m autour de la Mairie), les batiments et le coin medieval ; ponts = les routes passent dessus
     (l'eau s'arrete sous la route) + garde-corps en bois ;
  4. pose un PETIT LAC naturel a cote des constructions medievales (ruines du decor Stylized) ;
     l'ancien lac rond (cylindre) est cache, pas supprime ;
  5. pose arbres, buissons, haies, fleurs, roseaux et pierres : donnees OSM + jardins + haies le long des
     rues + berges de la riviere et du lac ;
  6. OPTIMISATION : tout est en instances (HISM, une seule commande de dessin par modele), disparition
     au loin (arbres 300 m, buissons 120 m, fleurs/roseaux 45 m), pas d'ombre pour les petites plantes,
     LOD automatiques sur les modeles qui n'en ont pas, rien ne touche au NavMesh. Les anciens arbres/haies
     poses un par un (CH_tree_*, CH_bush_*, CH_hedge_*) sont regroupes en instances eux aussi.

Avant tout : copie de la map dans Tools/VegetationEau/Sauvegardes/<date>/.
Relancer le script remplace seulement ce qu'il a pose.  ANNULER : ANNULER_VEGETATION_EAU.bat
Journal : Tools/VegetationEau/vegetation_eau.log   Carte de controle : Tools/VegetationEau/carte_controle.bmp
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
# <<< SAUVEGARDE AUTO CHELLES 3D
import datetime, heapq, json, math, os, random, shutil, traceback, urllib.parse, urllib.request

# =============================================================================================== reglages
CARTE = "/Game/Maps/Rachid"
CENTER = (48.8795, 2.5912)          # meme centre et meme carre que build_roads.py / routes_quartier.py
SIZE_M = 1000.0
CELLULE_M = 5.0                     # la map est "regardee" tous les 5 m

RAYON_CENTRE_VILLE_M = 300.0        # la riviere reste a plus de 300 m de la Mairie
LARGEUR_RIVIERE_M = 12.0            # largeur moyenne de l'eau (varie de +/- 20 %)
LARGEUR_BERGE_M = 1.6               # bordure de pierre de chaque cote
EAU_AU_DESSUS_DU_SOL_CM = 8.0       # l'eau est 8 cm au-dessus du sol (l'herbe ne passe pas au travers)
DEMI_LARGEUR_ROUTE_M = 6.3          # route du pack 2 voies + trottoirs (12 m) : l'eau s'arrete dessous
LAC_RAYON_M = 16.0                  # rayon du petit lac (reduit tout seul s'il manque de place)
LAC_RAYON_MIN_M = 9.0

FAIRE_RIVIERE = True
FAIRE_LAC = True
FAIRE_VEGETATION = True
REGROUPER_ANCIENNE_VEGETATION = True   # CH_tree_/CH_bush_/CH_hedge_ -> instances (originaux caches)
FAIRE_LOD = True                       # LOD auto sur les modeles de plantes qui n'en ont pas

# densites (m2 par plante) et limites (performances)
JARDIN_M2_PAR_ARBRE = 450.0
JARDIN_M2_PAR_BUISSON = 110.0
PARC_M2_PAR_ARBRE = 170.0
PARC_M2_PAR_BUISSON = 60.0
PARC_M2_PAR_FLEUR = 25.0
BOIS_M2_PAR_ARBRE = 55.0
BROUSSAILLES_M2_PAR_BUISSON = 20.0
HAIE_PAS_M = 1.2
HAIE_DECALAGE_RUE_M = 8.2           # haies de jardin a 8,2 m de l'axe des rues (derriere le trottoir)
SANS_HAIE_RUE_PRES_MAIRIE_M = 200.0
MAX = {"arbre": 4000, "buisson": 9000, "haie": 10000, "fleur": 5000, "roseau": 6000, "pierre": 3000, "barriere": 1500}

# categories : distance de fondu debut/fin (cm), ombres, collision
CATEGORIES = {
    "arbre":    (22000, 30000, True, True),
    "buisson":  (8000, 12000, False, False),
    "haie":     (10000, 15000, False, True),
    "fleur":    (3000, 4500, False, False),
    "roseau":   (3000, 4500, False, False),
    "pierre":   (6000, 9000, False, True),
    "barriere": (8000, 12000, False, True),
}

# modeles (les absents sont ignores) : chemin, hauteur visee en m (min, max)
MODELES = {
    "arbre": [("/Game/Stylized_Landscape/meshes/trees/SM_tree_03", (8, 12))]
             + [("/Game/AssetsvilleTown/Meshes/Nature/SM_tree_0%d" % i, (7, 11)) for i in range(1, 5)]
             + [("/Game/Venice_fast_building/Meshes/Vegetation/SM_maple_tree_%03d" % i, (7, 11)) for i in range(1, 9)],
    "arbre_eau": [("/Game/Nature_Pack/Trees/Willow/Willow", (9, 13)),
                  ("/Game/Stylized_Landscape/meshes/trees/SM_tree_03", (8, 11))],
    "buisson": [("/Game/Stylized_Landscape/meshes/plants/SM_bush_%s" % c, (0.9, 1.7)) for c in "abcde"]
               + [("/Game/AssetsvilleTown/Meshes/Nature/SM_bush_01", (1.0, 1.6))],
    "haie": [("/Game/Stylized_Landscape/meshes/plants/SM_bush_%s" % c, (1.3, 1.7)) for c in "abc"]
            + [("/Game/AssetsvilleTown/Meshes/Nature/SM_bush_01", (1.3, 1.7))],
    "fleur": [("/Game/Stylized_Landscape/meshes/plants/SM_flower_%s" % c, (0.35, 0.7)) for c in "abc"],
    "roseau": [("/Game/Flowers_Plants_Part1/Meshes/SM_Carex_01", (0.8, 1.3)),
               ("/Game/Stylized_Landscape/meshes/plants/SM_grass_a", (0.5, 0.9))],
    "pierre": [("/Game/Stylized_Landscape/meshes/cliff/SM_cliff_stone_0%d" % i, (0.4, 1.1)) for i in range(1, 5)],
    "barriere": [("/Game/Stylized_Landscape/meshes/props/SM_fence", (1.0, 1.2))],
}
EAU_MATERIAU_CHELLES = True        # eau opaque faite par le script (rapide, pas de refraction, toujours visible)
COULEUR_EAU = (0.015, 0.10, 0.13)
TEXTURE_VAGUES = "/Game/Stylized_Landscape/textures/water_textures/T_noise_water_normal_A"
MATERIAU_EAU = ["/Game/Stylized_Landscape/materials/water_materials/MI_water_Inst",
                "/Game/StarterContent/Materials/M_Water_Lake", "/Game/StarterContent/Materials/M_Water_Ocean"]
MATERIAU_BERGE = ["/Game/Stylized_Landscape/materials/cliff_materials/MI_cliff_stones_A_Inst",
                  "/Game/Stylized_Landscape/materials/building_materials/MI_paving_stones_A_Inst",
                  "/Game/StarterContent/Materials/M_Rock_Basalt"]

TAG = "ChellesVegEau"              # tout ce que le script pose
TAG_CACHE = "ChellesVegEauCache"   # ce que le script a cache (a re-afficher pour annuler)
DOSSIER_NIVEAU = "Chelles/VegetationEau"
DOSSIER_ASSETS = "/Game/Chelles/VegetationEau"
MOTS_SOL = ("landscape", "ground", "sol", "terrain", "floor")
MOTS_ROUTE = ("road", "route", "trottoir", "street", "asphalt")
MOTS_VEGETAL = ("tree", "bush", "hedge", "plant", "flower", "grass", "vegetation", "foliage", "fern", "carex",
                "willow", "maple", "oak", "pine", "birch", "elm", "leaf", "leaves")
MOTS_IGNORES = ("neige", "snow", "flocon", "sky", "ciel", "nuage", "cloud", "pluie", "rain", "weather", "meteo",
                "godray", "fog", "brume")   # ce qui flotte dans l'air : traverse par les tirs vus du ciel
MOTS_MEDIEVAL = ("/stylized_landscape/meshes/ruins", "/stylized_landscape/meshes/building", "castle", "chateau",
                 "medieval", "viking", "ruin", "donjon", "rempart", "abbaye")

OVERPASS = ["https://overpass-api.de/api/interpreter", "https://overpass.kumi.systems/api/interpreter",
            "https://overpass.private.coffee/api/interpreter"]

lat0, lon0 = CENTER
M_LAT = 111320.0
M_LON = 111320.0 * math.cos(math.radians(lat0))
HALF = SIZE_M / 2
N = int(round(SIZE_M / CELLULE_M))
SOL, ROUTE, VEGETAL, DUR, INCONNU = 1, 2, 3, 4, 0


# =============================================================================================== outils
def to_xy(lat, lon):
    return ((lon - lon0) * M_LON, (lat - lat0) * M_LAT)


def cellule(x, y):
    return int((x + HALF) // CELLULE_M), int((y + HALF) // CELLULE_M)


def centre_cellule(i, j):
    return (-HALF + (i + 0.5) * CELLULE_M, -HALF + (j + 0.5) * CELLULE_M)


def dans_carte(i, j):
    return 0 <= i < N and 0 <= j < N


def grille(v=0):
    return [[v] * N for _ in range(N)]


def dist_seg(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    L2 = dx * dx + dy * dy
    t = 0.0 if L2 == 0 else max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / L2))
    return math.hypot(p[0] - a[0] - t * dx, p[1] - a[1] - t * dy), t


def dans_polygone(p, poly):
    x, y = p
    c = False
    for k in range(len(poly)):
        x1, y1 = poly[k - 1]
        x2, y2 = poly[k]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            c = not c
    return c


def aire(poly):
    return abs(sum(poly[k - 1][0] * poly[k][1] - poly[k][0] * poly[k - 1][1] for k in range(len(poly)))) / 2


def remplir_polygone(g, poly, val):
    ys = [p[1] for p in poly]
    j0 = max(0, cellule(0, min(ys))[1])
    j1 = min(N - 1, cellule(0, max(ys))[1])
    for j in range(j0, j1 + 1):
        y = -HALF + (j + 0.5) * CELLULE_M
        xs = []
        for k in range(len(poly)):
            (x1, y1), (x2, y2) = poly[k - 1], poly[k]
            if (y1 > y) != (y2 > y):
                xs.append(x1 + (y - y1) * (x2 - x1) / (y2 - y1))
        xs.sort()
        for a, b in zip(xs[::2], xs[1::2]):
            for i in range(max(0, int((a + HALF) // CELLULE_M)), min(N, int((b + HALF) // CELLULE_M) + 1)):
                g[j][i] = val


class IndexSegments:
    """recherche rapide de la distance a un ensemble de polylignes (rues)"""
    def __init__(self, polylignes, pas=20.0):
        self.pas = pas
        self.cases = {}
        for n, pl in enumerate(polylignes):
            for a, b in zip(pl, pl[1:]):
                x0, x1 = sorted((a[0], b[0]))
                y0, y1 = sorted((a[1], b[1]))
                for cx in range(int(math.floor(x0 / pas)), int(math.floor(x1 / pas)) + 1):
                    for cy in range(int(math.floor(y0 / pas)), int(math.floor(y1 / pas)) + 1):
                        self.cases.setdefault((cx, cy), []).append((a, b, n))

    def proche(self, p, rayon):
        """(distance, segment a, segment b, n) du segment le plus proche a moins de rayon, sinon None"""
        best = None
        r = int(math.ceil(rayon / self.pas))
        cx, cy = int(math.floor(p[0] / self.pas)), int(math.floor(p[1] / self.pas))
        for dx in range(-r, r + 1):
            for dy in range(-r, r + 1):
                for a, b, n in self.cases.get((cx + dx, cy + dy), ()):
                    d, _ = dist_seg(p, a, b)
                    if d <= rayon and (best is None or d < best[0]):
                        best = (d, a, b, n)
        return best


def chamfer(source):
    """distance (m) de chaque cellule a la cellule 'source' (True) la plus proche (approximation 8 voisins)"""
    INF = 1e9
    d = [[0.0 if source[j][i] else INF for i in range(N)] for j in range(N)]
    a, b = CELLULE_M, CELLULE_M * 1.4142
    for j in range(N):
        for i in range(N):
            v = d[j][i]
            if v == 0.0:
                continue
            if i > 0: v = min(v, d[j][i - 1] + a)
            if j > 0:
                v = min(v, d[j - 1][i] + a)
                if i > 0: v = min(v, d[j - 1][i - 1] + b)
                if i < N - 1: v = min(v, d[j - 1][i + 1] + b)
            d[j][i] = v
    for j in range(N - 1, -1, -1):
        for i in range(N - 1, -1, -1):
            v = d[j][i]
            if v == 0.0:
                continue
            if i < N - 1: v = min(v, d[j][i + 1] + a)
            if j < N - 1:
                v = min(v, d[j + 1][i] + a)
                if i < N - 1: v = min(v, d[j + 1][i + 1] + b)
                if i > 0: v = min(v, d[j + 1][i - 1] + b)
            d[j][i] = v
    return d


def chaikin(pts, tours=3):
    for _ in range(tours):
        nv = [pts[0]]
        for a, b in zip(pts, pts[1:]):
            nv.append((0.75 * a[0] + 0.25 * b[0], 0.75 * a[1] + 0.25 * b[1]))
            nv.append((0.25 * a[0] + 0.75 * b[0], 0.25 * a[1] + 0.75 * b[1]))
        nv.append(pts[-1])
        pts = nv
    return pts


def reechantillonner(pts, pas):
    out = [pts[0]]
    reste = 0.0
    for a, b in zip(pts, pts[1:]):
        L = math.hypot(b[0] - a[0], b[1] - a[1])
        if L < 1e-6:
            continue
        d = pas - reste
        while d <= L:
            out.append((a[0] + (b[0] - a[0]) * d / L, a[1] + (b[1] - a[1]) * d / L))
            d += pas
        reste = L - (d - pas)
    if math.hypot(out[-1][0] - pts[-1][0], out[-1][1] - pts[-1][1]) > pas * 0.3:
        out.append(pts[-1])
    return out


def longueur(pts):
    return sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(pts, pts[1:]))


# =============================================================================================== donnees OSM
def telecharger_osm(cache, log):
    if os.path.exists(cache):
        try:
            return json.load(open(cache, encoding="utf-8")), "cache"
        except Exception:
            pass
    s, w = lat0 - HALF / M_LAT, lon0 - HALF / M_LON
    n, e = lat0 + HALF / M_LAT, lon0 + HALF / M_LON
    bb = "%f,%f,%f,%f" % (s, w, n, e)
    corps = ('node["natural"="tree"](BBOX);way["natural"="tree_row"](BBOX);way["barrier"="hedge"](BBOX);'
             'way["natural"~"^(wood|scrub|heath|grassland)$"](BBOX);'
             'way["landuse"~"^(forest|grass|meadow|village_green|flowerbed|orchard|allotments|cemetery|recreation_ground)$"](BBOX);'
             'way["leisure"~"^(park|garden|playground|nature_reserve)$"](BBOX);').replace("BBOX", bb)
    q = "[out:json][timeout:120];(" + corps + ");out geom tags;"
    for essai in range(2):
        for u in OVERPASS:
            try:
                req = urllib.request.Request(u, urllib.parse.urlencode({"data": q}).encode(),
                                             {"User-Agent": "chelles3d-vegetation/1.0"})
                data = json.load(urllib.request.urlopen(req, timeout=120))
                if data.get("elements"):
                    json.dump(data, open(cache, "w", encoding="utf-8"))
                    return data, u
            except Exception as ex:
                log("   OpenStreetMap %s : %s" % (u, ex))
    return None, None


def lire_donnees(out_dir, ici, log):
    """batiments, rues, mairie (fichiers deja utilises pour construire la map) + vegetation OSM"""
    bats, mairie = [], None
    for el in json.load(open(os.path.join(out_dir, "osm.json"), encoding="utf-8")).get("elements", []):
        g = [to_xy(q["lat"], q["lon"]) for q in el.get("geometry", [])]
        if len(g) >= 3:
            bats.append(g)
            if el.get("tags", {}).get("amenity") == "townhall":
                mairie = (sum(p[0] for p in g) / len(g), sum(p[1] for p in g) / len(g))
    if mairie is None:
        mairie = (-78.5, -193.1)     # Mairie de Chelles dans le repere de la map
    rues = []
    ways_routes = json.load(open(os.path.join(out_dir, "osm_ways.json"), encoding="utf-8")).get("elements", [])
    for el in ways_routes:
        if el.get("tags", {}).get("highway") in ("secondary", "tertiary", "residential", "unclassified", "living_street"):
            g = [to_xy(q["lat"], q["lon"]) for q in el.get("geometry", [])]
            if len(g) >= 2:
                rues.append(g)
    veg, source = telecharger_osm(os.path.join(ici, "osm_vegetation.json"), log)
    if veg is None:
        log("   OpenStreetMap injoignable : on utilise les donnees deja telechargees (Tools/out/osm_nodes.json + osm_ways.json)")
        els = []
        for f in ("osm_nodes.json", "osm_ways.json"):
            try:
                els += json.load(open(os.path.join(out_dir, f), encoding="utf-8")).get("elements", [])
            except Exception:
                pass
        veg, source = {"elements": els}, "fichiers du projet"
    arbres, rangees, haies, zones = [], [], [], []
    for el in veg.get("elements", []):
        t = el.get("tags", {})
        if el.get("type") == "node":
            if t.get("natural") == "tree":
                arbres.append(to_xy(el["lat"], el["lon"]))
            continue
        g = [to_xy(q["lat"], q["lon"]) for q in el.get("geometry", [])]
        if len(g) < 2:
            continue
        if t.get("natural") == "tree_row":
            rangees.append(g)
        elif t.get("barrier") == "hedge":
            if len(g) >= 4 and g[0] == g[-1] and aire(g) > 6 and "area" in t:
                zones.append(("buissons", g))
            else:
                haies.append(g)
        elif len(g) >= 4 and g[0] == g[-1]:
            nat, lu, le = t.get("natural"), t.get("landuse"), t.get("leisure")
            if nat == "wood" or lu in ("forest", "orchard"):
                genre = "bois"
            elif nat in ("scrub", "heath"):
                genre = "buissons"
            elif le in ("park", "garden", "nature_reserve") or lu in ("village_green", "flowerbed", "recreation_ground", "cemetery", "allotments"):
                genre = "parc"
            elif lu in ("grass", "meadow") or nat == "grassland" or le == "playground":
                genre = "herbe"
            else:
                continue
            zones.append((genre, g))
    log("1/9 donnees : %d batiments, %d rues, Mairie a (%.0f, %.0f) m ; OSM (%s) : %d arbres, %d alignements, %d haies, %d zones vertes"
        % (len(bats), len(rues), mairie[0], mairie[1], source, len(arbres), len(rangees), len(haies), len(zones)))
    return {"bats": bats, "rues": rues, "mairie": mairie, "arbres": arbres, "rangees": rangees, "haies": haies, "zones": zones}


# =============================================================================================== riviere
def chemin_riviere(cout, log):
    """plus court chemin (A* sans heuristique = Dijkstra) entre deux bords opposes ; renvoie la liste de cellules"""
    def dijkstra(departs, arrivee):
        dist, prev, pq = {}, {}, []
        for s in departs:
            dist[s] = cout[s[1]][s[0]]
            heapq.heappush(pq, (dist[s], s))
        while pq:
            d, u = heapq.heappop(pq)
            if d > dist.get(u, 1e18):
                continue
            if arrivee(u):
                p = [u]
                while p[-1] in prev:
                    p.append(prev[p[-1]])
                return d, p[::-1]
            for di in (-1, 0, 1):
                for dj in (-1, 0, 1):
                    if di == 0 and dj == 0:
                        continue
                    v = (u[0] + di, u[1] + dj)
                    if dans_carte(*v):
                        nd = d + cout[v[1]][v[0]] * (1.4142 if di and dj else 1.0)
                        if nd < dist.get(v, 1e18):
                            dist[v] = nd
                            prev[v] = u
                            heapq.heappush(pq, (nd, v))
        return 1e18, []

    essais = []
    for nom, dep, arr in (("ouest -> est", [(0, j) for j in range(N)], lambda u: u[0] == N - 1),
                          ("nord -> sud", [(i, N - 1) for i in range(N)], lambda u: u[1] == 0)):
        c, p = dijkstra(dep, arr)
        essais.append((c, nom, p))
        log("   trace %s : cout %.0f, %d m" % (nom, c, len(p) * CELLULE_M))
    essais.sort(key=lambda e: e[0])
    return essais[0]


def profil_riviere(pts):
    """largeur de l'eau le long de la riviere (m), varie doucement"""
    res, s = [], 0.0
    for k, p in enumerate(pts):
        if k:
            s += math.hypot(p[0] - pts[k - 1][0], p[1] - pts[k - 1][1])
        res.append(LARGEUR_RIVIERE_M * (1 + 0.13 * math.sin(s / 61.0) + 0.07 * math.sin(s / 23.0 + 1.3)))
    return res


def normales(pts):
    res = []
    for k in range(len(pts)):
        a = pts[max(0, k - 1)]
        b = pts[min(len(pts) - 1, k + 1)]
        dx, dy = b[0] - a[0], b[1] - a[1]
        L = math.hypot(dx, dy) or 1.0
        res.append((-dy / L, dx / L))
    return res


# =============================================================================================== calcul (sans Unreal)
def calculer(d, kind, zsol, exclusions, lac, log, rnd, sans_plantes=()):
    """d = donnees OSM ; kind/zsol = ce que la map contient (grilles) ; exclusions = [(x, y, r)] (medieval...)
    lac = (cx, cy, rayon) ou None. Renvoie riviere, ponts, plantes."""
    res = {"riviere": None, "lac": lac, "plantes": [], "ponts": [], "riviere_cout": None}
    idx_rues = IndexSegments(d["rues"])
    bat = grille(False)
    for g in d["bats"]:
        remplir_polygone(bat, g, True)
    dur = [[bat[j][i] or kind[j][i] == DUR for i in range(N)] for j in range(N)]
    route = [[kind[j][i] == ROUTE or idx_rues.proche(centre_cellule(i, j), DEMI_LARGEUR_ROUTE_M) is not None
              for i in range(N)] for j in range(N)]
    d_dur = chamfer(dur)
    d_route = chamfer(route)
    mx, my = d["mairie"]

    # ---------------- riviere
    eau = grille(False)
    if lac:
        lx, ly, lr = lac
        for j in range(N):
            for i in range(N):
                x, y = centre_cellule(i, j)
                if math.hypot(x - lx, y - ly) <= lr + 1.0:
                    eau[j][i] = True
    if FAIRE_RIVIERE:
        demi = LARGEUR_RIVIERE_M / 2 + LARGEUR_BERGE_M
        cout = grille(1.0)
        for j in range(N):
            for i in range(N):
                x, y = centre_cellule(i, j)
                c = 1.0
                if math.hypot(x - mx, y - my) < RAYON_CENTRE_VILLE_M:
                    c += 1e6
                if any(math.hypot(x - ex, y - ey) < er for ex, ey, er in exclusions):
                    c += 1e6
                if dur[j][i]:
                    c += 4000
                elif d_dur[j][i] < demi + 2.0:
                    c += 300 * (demi + 2.0 - d_dur[j][i])
                if route[j][i]:
                    c += 30                       # traverser une rue : oui, la longer : non
                elif d_route[j][i] < demi + 10.0:
                    c += 14
                if kind[j][i] == VEGETAL:
                    c += 2
                bord = min(x + HALF, HALF - x, y + HALF, HALF - y)
                if bord < 100:
                    c += 3 * (100 - bord) / 100
                cout[j][i] = c
        total, nom, cells = chemin_riviere(cout, log)
        res["riviere_cout"] = total
        if not cells or total >= 1e6:
            log("   ATTENTION : aucun trace possible sans passer par le centre-ville : pas de riviere")
        else:
            pts = [centre_cellule(i, j) for i, j in cells]
            # prolonge jusqu'au bord de la dalle (map 1,2 km) pour qu'elle sorte vraiment de la carte
            def prolonger(a, b):
                dx, dy = a[0] - b[0], a[1] - b[1]
                L = math.hypot(dx, dy) or 1.0
                return (a[0] + dx / L * 90, a[1] + dy / L * 90)
            pts = [prolonger(pts[0], pts[min(6, len(pts) - 1)])] + pts[::3] + [prolonger(pts[-1], pts[max(0, len(pts) - 7)])]
            pts = reechantillonner(chaikin(pts, 4), 1.5)
            larg = profil_riviere(pts)
            nrm = normales(pts)
            # la riviere se resserre la ou les maisons sont proches (jamais moins de 5 m)
            idx_bats = IndexSegments(d["bats"] and [g + [g[0]] for g in d["bats"]], pas=15.0)
            place = []
            for p in pts:
                pr = idx_bats.proche(p, 15.0)
                libre_m = pr[0] if pr else 15.0
                ci, cj = cellule(*p)
                if dans_carte(ci, cj):
                    libre_m = min(libre_m, d_dur[cj][ci] - 0.5 * CELLULE_M)
                place.append(max(2.5, libre_m - LARGEUR_BERGE_M - 0.6))
            lisse = []
            for k in range(len(place)):
                lisse.append(min(place[max(0, k - 6):k + 7]))
            larg = [min(w, 2 * pl) for w, pl in zip(larg, lisse)]
            larg = [sum(larg[max(0, k - 4):k + 5]) / len(larg[max(0, k - 4):k + 5]) for k in range(len(larg))]
            sous_route = [idx_rues.proche(p, DEMI_LARGEUR_ROUTE_M) is not None for p in pts]
            touches = 0
            for p, w, nr in zip(pts, larg, nrm):
                for s in (-0.5, 0.0, 0.5):
                    ci, cj = cellule(p[0] + nr[0] * w * s, p[1] + nr[1] * w * s)
                    if dans_carte(ci, cj) and dur[cj][ci]:
                        touches += 1
            res["riviere"] = {"pts": pts, "larg": larg, "nrm": nrm, "sous_route": sous_route, "sens": nom}
            log("3/9 riviere : %s, %.0f m, largeur %.0f-%.0f m, %d passage(s) sous une rue, %d point(s) sur un batiment"
                % (nom, longueur(pts), min(larg), max(larg), sum(1 for k in range(1, len(sous_route)) if sous_route[k] and not sous_route[k - 1]), touches))
            # cellules d'eau
            for p, w, nr in zip(pts, larg, nrm):
                for s in range(-int(w / 2 + LARGEUR_BERGE_M), int(w / 2 + LARGEUR_BERGE_M) + 1, 2):
                    ci, cj = cellule(p[0] + nr[0] * s, p[1] + nr[1] * s)
                    if dans_carte(ci, cj):
                        eau[cj][ci] = True
            # ponts : garde-corps des deux cotes de chaque rue au-dessus de l'eau
            k = 0
            while k < len(pts):
                if not sous_route[k]:
                    k += 1
                    continue
                k0 = k
                while k < len(pts) and sous_route[k]:
                    k += 1
                mid = pts[(k0 + k - 1) // 2]
                pr = idx_rues.proche(mid, DEMI_LARGEUR_ROUTE_M + 2)
                if pr is None:
                    continue
                _, a, b, _ = pr
                L = math.hypot(b[0] - a[0], b[1] - a[1]) or 1.0
                ux, uy = (b[0] - a[0]) / L, (b[1] - a[1]) / L
                _, t = dist_seg(mid, a, b)
                c = (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
                rn = nrm[(k0 + k - 1) // 2]
                cosang = abs(ux * rn[0] + uy * rn[1])
                portee = larg[(k0 + k - 1) // 2] / max(0.35, cosang) + 2.5
                res["ponts"].append({"centre": c, "dir": (ux, uy), "portee": portee})
            log("   %d pont(s) avec garde-corps" % len(res["ponts"]))

    d_eau = chamfer(eau)
    res["eau"] = eau
    if not FAIRE_VEGETATION:
        return res

    # ---------------- vegetation
    plantes = res["plantes"]
    occupe = {}

    def cle(p, pas):
        return (int(math.floor(p[0] / pas)), int(math.floor(p[1] / pas)))

    def proche_de(p, rayon, pas=4.0):
        cx, cy = cle(p, pas)
        r = int(math.ceil(rayon / pas))
        for dx in range(-r, r + 1):
            for dy in range(-r, r + 1):
                for q in occupe.get((cx + dx, cy + dy), ()):
                    if math.hypot(p[0] - q[0], p[1] - q[1]) < rayon:
                        return True
        return False

    def noter(p):
        occupe.setdefault(cle(p, 4.0), []).append(p)

    for p in d.get("existants", []):
        noter(p)

    compte = {}

    def libre(p, marge_dur, marge_route=0.0, marge_eau=1.0, sol_requis=True):
        i, j = cellule(*p)
        if not dans_carte(i, j):
            return False
        if sol_requis and kind[j][i] not in (SOL, INCONNU):
            return False
        if bat[j][i] or d_dur[j][i] < marge_dur:
            return False
        if marge_route and (route[j][i] or d_route[j][i] < marge_route - CELLULE_M / 2):
            return False
        if d_eau[j][i] < marge_eau:
            return False
        return True

    def hors_decor(p):
        return not any(math.hypot(p[0] - zx, p[1] - zy) < zr for zx, zy, zr in sans_plantes)

    def poser(cat, p, yaw=None, ech=1.0, etire=1.0, sous_cat=None):
        if compte.get(cat, 0) >= MAX[cat]:
            return False
        i, j = cellule(*p)
        z = zsol[j][i] if dans_carte(i, j) else None
        plantes.append({"cat": cat, "modele": sous_cat or cat, "x": p[0], "y": p[1], "z": z,
                        "yaw": rnd.uniform(0, 360) if yaw is None else yaw, "ech": ech, "etire": etire})
        compte[cat] = compte.get(cat, 0) + 1
        return True

    # 1. arbres OSM isoles + alignements
    for p in d["arbres"]:
        if libre(p, 2.0, 0.0, 2.0) and not proche_de(p, 3.0):
            if poser("arbre", p):
                noter(p)
    for g in d["rangees"]:
        for a, b in zip(g, g[1:]):
            L = math.hypot(b[0] - a[0], b[1] - a[1])
            for k in range(max(1, int(L / 8.0))):
                p = (a[0] + (b[0] - a[0]) * k * 8.0 / max(L, 1e-6), a[1] + (b[1] - a[1]) * k * 8.0 / max(L, 1e-6))
                if libre(p, 2.0, 0.0, 2.0, sol_requis=False) and not proche_de(p, 4.0):
                    if poser("arbre", p):
                        noter(p)

    # 2. haies OSM
    for g in d["haies"]:
        for a, b in zip(g, g[1:]):
            L = math.hypot(b[0] - a[0], b[1] - a[1])
            if L < 0.5:
                continue
            yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))     # sens de la haie (repere en m)
            for k in range(int(L / HAIE_PAS_M) + 1):
                p = (a[0] + (b[0] - a[0]) * k * HAIE_PAS_M / L, a[1] + (b[1] - a[1]) * k * HAIE_PAS_M / L)
                if libre(p, 0.6, 0.0, 1.0, sol_requis=False):
                    poser("haie", p, yaw, rnd.uniform(0.95, 1.1), 1.35)

    # 3. zones vertes OSM
    densites = {"bois": [("arbre", BOIS_M2_PAR_ARBRE, 3.0), ("buisson", 90.0, 1.5)],
                "buissons": [("buisson", BROUSSAILLES_M2_PAR_BUISSON, 1.2), ("arbre", 400.0, 3.5)],
                "parc": [("arbre", PARC_M2_PAR_ARBRE, 3.5), ("buisson", PARC_M2_PAR_BUISSON, 1.5), ("fleur", PARC_M2_PAR_FLEUR, 0.5)],
                "herbe": [("arbre", 600.0, 3.5), ("buisson", 160.0, 1.5), ("fleur", 90.0, 0.5)]}
    for genre, g in d["zones"]:
        xs, ys = [p[0] for p in g], [p[1] for p in g]
        A = aire(g)
        for cat, m2, ecart in densites[genre]:
            for _ in range(int(A / m2)):
                p = (rnd.uniform(min(xs), max(xs)), rnd.uniform(min(ys), max(ys)))
                if not dans_polygone(p, g) or not hors_decor(p):
                    continue
                if cat == "arbre":
                    if libre(p, 3.0, 4.0, 3.0) and not proche_de(p, 4.5):
                        if poser("arbre", p):
                            noter(p)
                elif libre(p, ecart, 3.0, 1.0):
                    poser(cat, p, ech=rnd.uniform(0.8, 1.2))

    # 4. haies de jardin le long des rues (avec des entrees de garage)
    for g in d["rues"]:
        for cote in (-1, 1):
            prochain_trou = rnd.uniform(8, 25)
            s = 0.0
            for a, b in zip(g, g[1:]):
                L = math.hypot(b[0] - a[0], b[1] - a[1])
                if L < 0.5:
                    continue
                ux, uy = (b[0] - a[0]) / L, (b[1] - a[1]) / L
                yaw = math.degrees(math.atan2(uy, ux))
                t = 0.0
                while t < L:
                    s += HAIE_PAS_M
                    t += HAIE_PAS_M
                    if s >= prochain_trou:
                        if s >= prochain_trou + 3.5:
                            prochain_trou = s + rnd.uniform(8, 25)
                        continue
                    p = (a[0] + ux * t - uy * HAIE_DECALAGE_RUE_M * cote, a[1] + uy * t + ux * HAIE_DECALAGE_RUE_M * cote)
                    if math.hypot(p[0] - mx, p[1] - my) < SANS_HAIE_RUE_PRES_MAIRIE_M:
                        continue
                    pr = idx_rues.proche(p, HAIE_DECALAGE_RUE_M - 0.6)
                    if pr is not None:                       # trop pres d'une autre rue (carrefour)
                        continue
                    if libre(p, 1.2, 0.0, 1.5) and hors_decor(p):
                        poser("haie", p, yaw, rnd.uniform(0.95, 1.1), 1.35)

    # 5. jardins : tout le sol libre loin des rues et des maisons
    for j in range(N):
        for i in range(N):
            if kind[j][i] != SOL or route[j][i] or d_route[j][i] < 9.0 or d_eau[j][i] < 3.0 or d_dur[j][i] < 2.5:
                continue
            for cat, m2, marge, ecart in (("arbre", JARDIN_M2_PAR_ARBRE, 4.0, 5.0), ("buisson", JARDIN_M2_PAR_BUISSON, 2.0, 0)):
                if rnd.random() < CELLULE_M * CELLULE_M / m2:
                    x, y = centre_cellule(i, j)
                    p = (x + rnd.uniform(-2.4, 2.4), y + rnd.uniform(-2.4, 2.4))
                    if d_dur[j][i] < marge or not hors_decor(p):
                        continue
                    if cat == "arbre":
                        if not proche_de(p, ecart) and poser("arbre", p):
                            noter(p)
                    else:
                        poser("buisson", p, ech=rnd.uniform(0.8, 1.25))

    # 6. berges de la riviere
    riv = res["riviere"]
    if riv:
        pts, larg, nrm, sr = riv["pts"], riv["larg"], riv["nrm"], riv["sous_route"]
        prochain_saule = rnd.uniform(15, 40)
        s = 0.0
        for k in range(0, len(pts), 2):
            s += 3.0
            if sr[k]:
                continue
            p, w, nr = pts[k], larg[k], nrm[k]
            for cote in (-1, 1):
                bord = w / 2 + LARGEUR_BERGE_M
                if rnd.random() < 0.75:
                    q = (p[0] + nr[0] * cote * (bord + rnd.uniform(0.2, 1.5)), p[1] + nr[1] * cote * (bord + rnd.uniform(0.2, 1.5)))
                    if libre(q, 0.5, 2.0, 0.0, sol_requis=False):
                        poser("roseau", q, ech=rnd.uniform(0.8, 1.3))
                if rnd.random() < 0.25:
                    q = (p[0] + nr[0] * cote * (w / 2 - 0.3), p[1] + nr[1] * cote * (w / 2 - 0.3))
                    poser("roseau", q, ech=rnd.uniform(0.7, 1.1))
                if rnd.random() < 0.18:
                    q = (p[0] + nr[0] * cote * (w / 2 + LARGEUR_BERGE_M * 0.4), p[1] + nr[1] * cote * (w / 2 + LARGEUR_BERGE_M * 0.4))
                    poser("pierre", q, ech=rnd.uniform(0.7, 1.3))
                if rnd.random() < 0.10:
                    q = (p[0] + nr[0] * cote * (bord + rnd.uniform(2.0, 4.0)), p[1] + nr[1] * cote * (bord + rnd.uniform(2.0, 4.0)))
                    if libre(q, 1.5, 3.0, 0.0):
                        poser("buisson", q, ech=rnd.uniform(0.9, 1.3))
            if s >= prochain_saule:
                cote = rnd.choice((-1, 1))
                q = (p[0] + nr[0] * cote * (w / 2 + 5.0), p[1] + nr[1] * cote * (w / 2 + 5.0))
                if libre(q, 4.0, 5.0, 0.0) and not proche_de(q, 6.0):
                    if poser("arbre", q, sous_cat="arbre_eau"):
                        noter(q)
                    prochain_saule = s + rnd.uniform(35, 70)

    # 7. tour du lac
    if lac:
        lx, ly, lr = lac
        for k in range(int(2 * math.pi * lr / 1.6)):
            a = k * 1.6 / lr
            r = rayon_lac(lr, a)
            if rnd.random() < 0.55:
                poser("pierre", (lx + math.cos(a) * (r + 0.4), ly + math.sin(a) * (r + 0.4)), ech=rnd.uniform(0.6, 1.4))
            if rnd.random() < 0.7:
                rr = r + rnd.uniform(-0.6, 2.5)
                poser("roseau", (lx + math.cos(a) * rr, ly + math.sin(a) * rr), ech=rnd.uniform(0.8, 1.3))
            if rnd.random() < 0.25:
                rr = r + rnd.uniform(3.0, 6.0)
                q = (lx + math.cos(a) * rr, ly + math.sin(a) * rr)
                if libre(q, 1.0, 2.0, 0.0, sol_requis=False):
                    poser("fleur", q, ech=rnd.uniform(0.8, 1.2))
        for k in range(4):
            a = k * math.pi / 2 + rnd.uniform(-0.4, 0.4)
            rr = rayon_lac(lr, a) + rnd.uniform(5.0, 8.0)
            q = (lx + math.cos(a) * rr, ly + math.sin(a) * rr)
            if libre(q, 3.0, 3.0, 0.0, sol_requis=False) and not proche_de(q, 5.0):
                if poser("arbre", q, sous_cat="arbre_eau"):
                    noter(q)

    log("6/9 plantes calculees : %s" % ", ".join("%s %d" % (k, v) for k, v in sorted(compte.items())))
    res["eau"] = eau
    return res


def rayon_lac(r, a):
    """bord du lac : un cercle deforme (forme naturelle, toujours la meme)"""
    return r * (1 + 0.16 * math.sin(2 * a + 0.7) + 0.09 * math.sin(3 * a + 2.1) + 0.05 * math.sin(5 * a + 0.3))


# =============================================================================================== maillages (OBJ)
def maillage_riviere(riv, z_de, dz):
    """triangles (x, y, z, u, v) en cm, monde ; eau + berges ; coupe sous les rues"""
    eau, berge = [], []
    pts, larg, nrm, sr = riv["pts"], riv["larg"], riv["nrm"], riv["sous_route"]
    s = [0.0]
    for k in range(1, len(pts)):
        s.append(s[-1] + math.hypot(pts[k][0] - pts[k - 1][0], pts[k][1] - pts[k - 1][1]))
    def sommets(k, decal, dzc):
        p, nr = pts[k], nrm[k]
        x, y = p[0] + nr[0] * decal, p[1] + nr[1] * decal
        return (x, y, z_de(x, y) + dzc)
    for k in range(len(pts) - 1):
        if sr[k] or sr[k + 1]:
            continue
        w0, w1 = larg[k] / 2, larg[k + 1] / 2
        u0, u1 = s[k] / 6.0, s[k + 1] / 6.0
        a = sommets(k, -w0, dz); b = sommets(k, w0, dz)
        c = sommets(k + 1, w1, dz); e = sommets(k + 1, -w1, dz)
        eau += [(a + (u0, 0.0), b + (u0, 2.0), c + (u1, 2.0)), (a + (u0, 0.0), c + (u1, 2.0), e + (u1, 0.0))]
        for cote in (-1, 1):
            i0 = sommets(k, cote * (w0 - 0.4), dz + 4.0)
            i1 = sommets(k + 1, cote * (w1 - 0.4), dz + 4.0)
            h0 = sommets(k, cote * (w0 + 0.3), dz + 10.0)
            h1 = sommets(k + 1, cote * (w1 + 0.3), dz + 10.0)
            o0 = sommets(k, cote * (w0 + LARGEUR_BERGE_M), 0.5)
            o1 = sommets(k + 1, cote * (w1 + LARGEUR_BERGE_M), 0.5)
            for (p0, p1, q0, q1, v0, v1) in ((i0, i1, h0, h1, 0.0, 0.3), (h0, h1, o0, o1, 0.3, 1.0)):
                berge += [(p0 + (u0, v0), q0 + (u0, v1), q1 + (u1, v1)), (p0 + (u0, v0), q1 + (u1, v1), p1 + (u1, v0))]
    return eau, berge


def maillage_lac(lac, z_de, dz):
    lx, ly, lr = lac
    eau, berge = [], []
    n = 72
    zc = z_de(lx, ly) + dz
    centre = (lx, ly, zc, lx / 6.0, ly / 6.0)
    for k in range(n):
        a0, a1 = 2 * math.pi * k / n, 2 * math.pi * (k + 1) / n
        r0, r1 = rayon_lac(lr, a0), rayon_lac(lr, a1)
        p0 = (lx + math.cos(a0) * r0, ly + math.sin(a0) * r0)
        p1 = (lx + math.cos(a1) * r1, ly + math.sin(a1) * r1)
        eau.append((centre, (p0[0], p0[1], zc, p0[0] / 6.0, p0[1] / 6.0), (p1[0], p1[1], zc, p1[0] / 6.0, p1[1] / 6.0)))
        def anneau(r_plus, a, dzc):
            r = rayon_lac(lr, a) + r_plus
            x, y = lx + math.cos(a) * r, ly + math.sin(a) * r
            return (x, y, (zc if dzc is not None else z_de(x, y)) + (dzc if dzc is not None else 0.5))
        u0, u1 = a0 * lr / 6.0, a1 * lr / 6.0
        i0, i1 = anneau(-0.5, a0, 4.0), anneau(-0.5, a1, 4.0)
        h0, h1 = anneau(0.3, a0, 10.0), anneau(0.3, a1, 10.0)
        o0, o1 = anneau(LARGEUR_BERGE_M + 0.4, a0, None), anneau(LARGEUR_BERGE_M + 0.4, a1, None)
        for (q0, q1, r0_, r1_, v0, v1) in ((i0, i1, h0, h1, 0.0, 0.3), (h0, h1, o0, o1, 0.3, 1.0)):
            berge += [(q0 + (u0, v0), r0_ + (u0, v1), r1_ + (u1, v1)), (q0 + (u0, v0), r1_ + (u1, v1), q1 + (u1, v0))]
    return eau, berge


def ecrire_obj(chemin, tris, origine, axes):
    """tris (x, y, z, u, v) : x, y en m dans le repere monde/100, z en cm -> fichier OBJ.
    axes[k] = (axe_fichier, signe, echelle) : axe Unreal k = signe * echelle * axe_fichier.
    Chaque triangle est ecrit deux fois (dans les deux sens, sur des sommets separes) : la surface est
    visible de dessus comme de dessous, quel que soit le sens choisi par l'importeur."""
    inv = [None, None, None]
    for k, (src, sg, ech) in enumerate(axes):
        inv[src] = (k, sg * ech)
    verts, uvs, faces = [], [], []
    for tri in tris:
        ids = []
        for (x, y, z, u, v) in tri:
            ue = (x * 100.0 - origine[0], y * 100.0 - origine[1], z - origine[2])
            verts.append(tuple(ue[inv[src][0]] / inv[src][1] for src in range(3)))
            uvs.append((u, v))
            ids.append(len(verts))
        faces.append(ids)
    n1 = len(verts)
    with open(chemin, "w", encoding="ascii") as fo:
        fo.write("# Chelles 3D - genere par vegetation_eau.py (deux faces)\n")
        for _ in range(2):
            for f in verts:
                fo.write("v %.3f %.3f %.3f\n" % f)
        for _ in range(2):
            for u, v in uvs:
                fo.write("vt %.4f %.4f\n" % (u, v))
        fo.write("g surface\n")
        for a, b, c in faces:
            fo.write("f %d/%d %d/%d %d/%d\n" % (a, a, b, b, c, c))
        for a, b, c in faces:
            fo.write("f %d/%d %d/%d %d/%d\n" % (a + n1, a + n1, c + n1, c + n1, b + n1, b + n1))
    return len(faces) * 2


# =============================================================================================== carte de controle
def carte_controle(chemin, d, kind, res, echelle=2):
    """image BMP vue du ciel (s'ouvre avec Windows) : gris = bati, noir = route, bleu = eau, vert = plantes,
    cercle rouge = centre-ville evite par la riviere"""
    S = N * echelle
    img = bytearray([225, 235, 235] * S * S)          # BMP : bleu, vert, rouge
    def px(x, y, c):
        i, j = int((x + HALF) / SIZE_M * S), int((y + HALF) / SIZE_M * S)    # BMP : la 1re ligne est en bas
        if 0 <= i < S and 0 <= j < S:
            img[(j * S + i) * 3:(j * S + i) * 3 + 3] = bytes((c[2], c[1], c[0]))
    def case(i, j, c):
        for a in range(echelle):
            for b in range(echelle):
                ii, jj = i * echelle + a, j * echelle + b
                img[(jj * S + ii) * 3:(jj * S + ii) * 3 + 3] = bytes((c[2], c[1], c[0]))
    for j in range(N):
        for i in range(N):
            c = {DUR: (150, 150, 150), ROUTE: (60, 60, 60), VEGETAL: (170, 210, 160)}.get(kind[j][i])
            if c:
                case(i, j, c)
    eau = res.get("eau")
    if eau:
        for j in range(N):
            for i in range(N):
                if eau[j][i]:
                    case(i, j, (40, 110, 230))
    couleurs = {"arbre": (0, 110, 0), "buisson": (60, 170, 40), "haie": (0, 70, 30), "fleur": (230, 60, 160),
                "roseau": (120, 150, 40), "pierre": (110, 90, 70), "barriere": (140, 80, 20)}
    for p in res["plantes"]:
        px(p["x"], p["y"], couleurs.get(p["cat"], (0, 0, 0)))
    mx, my = d["mairie"]
    for a in range(0, 360):
        px(mx + math.cos(math.radians(a)) * RAYON_CENTRE_VILLE_M, my + math.sin(math.radians(a)) * RAYON_CENTRE_VILLE_M, (220, 0, 0))
    ligne = S * 3
    pad = (4 - ligne % 4) % 4
    data = b"".join(bytes(img[j * ligne:(j + 1) * ligne]) + b"\0" * pad for j in range(S))
    import struct
    entete = b"BM" + struct.pack("<IHHI", 54 + len(data), 0, 0, 54) + struct.pack("<IiiHHIIiiII", 40, S, S, 1, 24, 0, len(data), 2835, 2835, 0, 0)
    with open(chemin, "wb") as f:
        f.write(entete + data)


# =============================================================================================== Unreal
def executer_dans_unreal():
    import unreal

    eal = unreal.EditorAssetLibrary
    ell = unreal.EditorLevelLibrary
    proj = unreal.SystemLibrary.get_project_directory()
    ici = os.path.join(proj, "Tools", "VegetationEau")
    out_dir = os.path.join(proj, "Tools", "out")
    os.makedirs(ici, exist_ok=True)
    journal = os.path.join(ici, "vegetation_eau.log")
    open(journal, "w", encoding="utf-8").write("")
    etat_json = os.path.join(ici, "journal_vegetation_eau.json")

    def log(msg):
        unreal.log("[VegEau] " + str(msg))
        with open(journal, "a", encoding="utf-8") as f:
            f.write(str(msg) + "\n")

    log("Vegetation + riviere + lac d'apres OpenStreetMap  (%s)" % datetime.datetime.now().strftime("%d/%m/%Y %H:%M"))
    if not hasattr(unreal, "ChellesVegetationActor"):
        log("ABANDON : le plugin ChellesVegetation n'est pas compile. Lancez INSTALLER_VEGETATION_EAU.bat "
            "(Unreal ferme) : il le compile puis relance ce script. Rien n'a ete modifie.")
        return
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        log("chargement de %s ..." % CARTE)
        ell.load_level(CARTE)
        monde = ell.get_editor_world()
    log("Carte ouverte : %s" % monde.get_path_name())
    for f in ("osm.json", "osm_ways.json"):
        if not os.path.exists(os.path.join(out_dir, f)):
            log("ABANDON : Tools/out/%s introuvable (donnees qui ont servi a construire la map). Rien n'a ete modifie." % f)
            return

    # ---- 0. sauvegarde de la map
    pkg = monde.get_outermost().get_name()
    rel = pkg[len("/Game/"):] if pkg.startswith("/Game/") else None
    dest = os.path.join(ici, "Sauvegardes", datetime.datetime.now().strftime("%Y-%m-%d_%Hh%M"))
    copies = 0
    if rel:
        base = os.path.join(unreal.SystemLibrary.get_project_content_directory(), *rel.split("/"))
        os.makedirs(dest, exist_ok=True)
        for suffixe in (".umap", "_BuiltData.uasset"):
            if os.path.exists(base + suffixe):
                shutil.copy2(base + suffixe, dest)
                copies += 1
    if not copies:
        log("ABANDON : impossible de copier le .umap (enregistrez la map puis relancez). Rien n'a ete modifie.")
        return
    log("0/9 copie de la map : %s" % dest)

    etat = {}
    if os.path.exists(etat_json):
        try:
            etat = json.load(open(etat_json, encoding="utf-8"))
        except Exception:
            etat = {}
    etat.setdefault("lod", [])

    # ---- nettoyage d'un passage precedent (on remet tout comme avant ce script)
    def tags_de(a):
        return [str(t) for t in a.tags]

    def cacher(a):
        if a.root_component:
            a.root_component.set_visibility(False, True)
        a.set_actor_enable_collision(False)
        t = list(a.tags)
        if unreal.Name(TAG_CACHE) not in t:
            t.append(unreal.Name(TAG_CACHE))
        a.tags = t

    def montrer(a):
        if a.root_component:
            a.root_component.set_visibility(True, True)
        a.set_actor_enable_collision(True)
        a.tags = [t for t in a.tags if str(t) != TAG_CACHE]

    enleves = remis = 0
    for a in ell.get_all_level_actors():
        tg = tags_de(a)
        if TAG in tg:
            a.destroy_actor()
            enleves += 1
        elif TAG_CACHE in tg:
            montrer(a)
            remis += 1
    if enleves or remis:
        log("   passage precedent defait : %d objet(s) enleve(s), %d re-affiche(s)" % (enleves, remis))
    acteurs = ell.get_all_level_actors()

    # ---- repere (meme methode que Tools/Routes/routes_quartier.py)
    ysign = -1.0
    try:
        tuiles = [a for a in acteurs if a.get_actor_label().startswith("Chelles_Tile_Road_")]
        if tuiles:
            ref = sorted(tuiles, key=lambda a: a.get_actor_label())[0]
            o, _ = ref.get_actor_bounds(False)
            man = json.load(open(os.path.join(out_dir, "manifest.json")))
            bx = next(t["box_cm"] for t in man["tiles"] if t["name"] == ref.get_actor_label())
            ysign = -1.0 if abs(o.y + bx[1]) <= abs(o.y - bx[1]) else 1.0
    except Exception as e:
        log("   (sens de l'axe Y non mesure : %s)" % e)

    def vers_monde(x, y):
        return x * 100.0, ysign * y * 100.0

    def depuis_monde(X, Y):
        return X / 100.0, ysign * Y / 100.0

    # ---- 1. donnees
    d = lire_donnees(out_dir, ici, log)

    # ---- 2. la map vue du ciel
    def label(a):
        try:
            return a.get_actor_label()
        except Exception:
            return ""

    def chemin_mesh(a):
        try:
            c = a.get_component_by_class(unreal.StaticMeshComponent)
            m = c.get_editor_property("static_mesh") if c else None
            return m.get_path_name().lower() if m else ""
        except Exception:
            return ""

    def genre_acteur(a):
        if isinstance(a, (unreal.Landscape, unreal.LandscapeProxy)):
            return SOL
        lab = label(a).lower()
        if lab.startswith("chelles_tile_road") or lab.startswith("chelles_trottoir") or "ChellesRoutePRG" in tags_de(a):
            return ROUTE
        if any(m in lab for m in MOTS_SOL):
            return SOL
        if any(m in lab for m in MOTS_ROUTE):
            return ROUTE
        if isinstance(a, unreal.InstancedFoliageActor) or lab.startswith(("ch_tree", "ch_bush", "ch_hedge")):
            return VEGETAL
        mp = chemin_mesh(a)
        if mp and any(m in mp for m in MOTS_VEGETAL):
            return VEGETAL
        return DUR

    def elements(hit):
        if hit is None:
            return []
        if isinstance(hit, (tuple, list)):
            return list(hit)
        try:
            return list(hit.to_tuple())
        except Exception:
            return [hit]

    cache_genre = {}
    ignores = [a for a in acteurs if isinstance(a, unreal.Volume) or any(m in label(a).lower() for m in MOTS_IGNORES)]
    hauts = [a.get_actor_bounds(False) for a in acteurs if "ground" in label(a).lower() or isinstance(a, unreal.LandscapeProxy)]
    z_ref = max(o.z + e.z for o, e in hauts) if hauts else None

    def tirer(X, Y):
        for _ in range(4):
            hit = unreal.SystemLibrary.line_trace_single(monde, unreal.Vector(X, Y, 500000.0), unreal.Vector(X, Y, -500000.0),
                                                         unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignores,
                                                         unreal.DrawDebugTrace.NONE, True)
            el = elements(hit)
            if not el or not next((e for e in el if isinstance(e, bool)), False):
                return None, None
            act = next((e for e in el if isinstance(e, unreal.Actor)), None)
            vecs = [e for e in el if isinstance(e, unreal.Vector)]
            if act is None or not vecs:
                return None, None
            if z_ref is not None and vecs[0].z > z_ref + 12000.0:     # a plus de 120 m du sol : decor du ciel
                ignores.append(act)
                continue
            nom = act.get_path_name()
            if nom not in cache_genre:
                cache_genre[nom] = genre_acteur(act)
            return cache_genre[nom], vecs[0].z
        return None, None

    kind = grille(INCONNU)
    zs = grille(None)
    touches = 0
    with unreal.ScopedSlowTask(N, "Chelles 3D : la map est regardee vue du ciel...") as tache:
        tache.make_dialog(True)
        for j in range(N):
            if tache.should_cancel():
                log("ABANDON : arrete par l'utilisateur. Rien n'a ete modifie.")
                return
            tache.enter_progress_frame(1)
            for i in range(N):
                X, Y = vers_monde(*centre_cellule(i, j))
                g, z = tirer(X, Y)
                if g is not None:
                    kind[j][i] = g
                    touches += 1
                    if g == SOL:
                        zs[j][i] = z
    sols = sorted(zs[j][i] for j in range(N) for i in range(N) if zs[j][i] is not None)
    if touches < N * N * 0.3 or not sols:
        # les tirs ne marchent pas (cas rare en mode ligne de commande) : sol = dessus de la dalle du quartier
        z_def = z_ref if z_ref is not None else 0.0
        log("   ATTENTION : seulement %d/%d tirs ont touche quelque chose ; on se fie a OpenStreetMap, sol a z=%.0f cm" % (touches, N * N, z_def))
        kind = grille(INCONNU)
        zs = grille(z_def)
    else:
        z_def = sols[len(sols) // 2]
        # trous (sous un toit, un arbre...) : hauteur des voisins
        for _ in range(3):
            for j in range(N):
                for i in range(N):
                    if zs[j][i] is None:
                        v = [zs[b][a] for a in (i - 1, i, i + 1) for b in (j - 1, j, j + 1) if dans_carte(a, b) and zs[b][a] is not None]
                        if v:
                            zs[j][i] = sum(v) / len(v)
        for j in range(N):
            for i in range(N):
                if zs[j][i] is None:
                    zs[j][i] = z_def
    nb = {g: sum(1 for j in range(N) for i in range(N) if kind[j][i] == g) for g in (SOL, ROUTE, VEGETAL, DUR)}
    log("2/9 map regardee (%d x %d cases de %.0f m) : sol %d, routes %d, plantes %d, constructions %d ; sol median z=%.0f cm"
        % (N, N, CELLULE_M, nb[SOL], nb[ROUTE], nb[VEGETAL], nb[DUR], z_def))

    def z_de(x, y):
        """hauteur du sol (cm) au point (m) : interpolation entre les cases"""
        fx, fy = (x + HALF) / CELLULE_M - 0.5, (y + HALF) / CELLULE_M - 0.5
        i0, j0 = int(math.floor(fx)), int(math.floor(fy))
        tx, ty = fx - i0, fy - j0
        def z(i, j):
            i = min(N - 1, max(0, i)); j = min(N - 1, max(0, j))
            return zs[j][i]
        return (z(i0, j0) * (1 - tx) * (1 - ty) + z(i0 + 1, j0) * tx * (1 - ty)
                + z(i0, j0 + 1) * (1 - tx) * ty + z(i0 + 1, j0 + 1) * tx * ty)

    # ---- coin medieval + ancien lac
    medievaux = []
    ancien_lac, pierres_lac = None, []
    for a in acteurs:
        lab = label(a)
        if lab.startswith("Stylized_Lac"):
            ancien_lac = a
            continue
        if lab.startswith("Stylized_PierreLac"):
            pierres_lac.append(a)
            continue
        mp = chemin_mesh(a)
        txt = (lab.lower() + " " + mp)
        if any(m in txt for m in MOTS_MEDIEVAL):
            l = a.get_actor_location()
            medievaux.append(depuis_monde(l.x, l.y))
    exclusions = []
    lac = None
    if medievaux:
        best = max(medievaux, key=lambda p: sum(1 for q in medievaux if math.hypot(p[0] - q[0], p[1] - q[1]) < 60))
        groupe = [q for q in medievaux if math.hypot(best[0] - q[0], best[1] - q[1]) < 60]
        mcx = sum(q[0] for q in groupe) / len(groupe)
        mcy = sum(q[1] for q in groupe) / len(groupe)
        rayon_m = max(25.0, max(math.hypot(q[0] - mcx, q[1] - mcy) for q in groupe) + 12.0)
        exclusions.append((mcx, mcy, rayon_m))
        log("   coin medieval : %d element(s) autour de (%.0f, %.0f) m (rayon %.0f m)" % (len(groupe), mcx, mcy, rayon_m))
    else:
        mcx = mcy = None
        log("   coin medieval introuvable (aucune ruine / chateau dans la map)")

    if FAIRE_LAC:
        dur_lac = [[kind[j][i] == DUR for i in range(N)] for j in range(N)]
        if ancien_lac is not None:          # l'ancien lac et ses pierres ne genent pas
            for a in [ancien_lac] + pierres_lac:
                o, e = a.get_actor_bounds(False)
                cx_, cy_ = depuis_monde(o.x, o.y)
                r_ = max(e.x, e.y) / 100.0 + 2.0
                for j in range(N):
                    for i in range(N):
                        x_, y_ = centre_cellule(i, j)
                        if math.hypot(x_ - cx_, y_ - cy_) <= r_ + CELLULE_M:
                            dur_lac[j][i] = False
        bat_g = grille(False)
        for g in d["bats"]:
            remplir_polygone(bat_g, g, True)
        idx = IndexSegments(d["rues"])
        d_dur_lac = chamfer([[dur_lac[j][i] or bat_g[j][i] for i in range(N)] for j in range(N)])

        def place_pour(cx, cy):
            ci, cj = cellule(cx, cy)
            if not dans_carte(ci, cj):
                return 0.0
            r = d_dur_lac[cj][ci] - 4.0
            pr = idx.proche((cx, cy), 60.0)
            if pr is not None:
                r = min(r, pr[0] - DEMI_LARGEUR_ROUTE_M - 4.0)
            return min(r / 1.3, LAC_RAYON_M)

        cands = []
        if ancien_lac is not None:
            l = ancien_lac.get_actor_location()
            cands.append(depuis_monde(l.x, l.y))
        if mcx is not None:
            for r in range(20, 110, 5):
                for k in range(24):
                    a = 2 * math.pi * k / 24
                    cands.append((mcx + math.cos(a) * r, mcy + math.sin(a) * r))
        meilleur = None
        for n, (cx, cy) in enumerate(cands):
            r = place_pour(cx, cy)
            bonus = 3.0 if n == 0 and ancien_lac is not None else 0.0
            if r >= LAC_RAYON_MIN_M and (meilleur is None or r + bonus > meilleur[2] + meilleur[3]):
                meilleur = (cx, cy, r, bonus)
        if meilleur:
            lac = (meilleur[0], meilleur[1], meilleur[2])
            if mcx is not None:
                exclusions.append((lac[0], lac[1], lac[2] + 15.0))
            log("4/9 lac : centre (%.0f, %.0f) m, rayon %.0f m%s" % (lac[0], lac[1], lac[2],
                " (a la place de l'ancien lac rond)" if meilleur[3] else ""))
        else:
            log("4/9 lac : pas de place libre a cote du coin medieval (pas de lac)")

    # ---- 3. calcul (riviere, ponts, plantes)
    existants = []
    for a in acteurs:
        if genre_acteur(a) == VEGETAL and not isinstance(a, unreal.InstancedFoliageActor):
            lab = label(a).lower()
            mp = chemin_mesh(a)
            if "tree" in lab or "tree" in mp or "willow" in mp or "maple" in mp:
                l = a.get_actor_location()
                existants.append(depuis_monde(l.x, l.y))
    d["existants"] = existants
    rnd = random.Random(7750077)
    res = calculer(d, kind, zs, exclusions, lac, log, rnd,
                   sans_plantes=[exclusions[0]] if medievaux else [])
    try:
        carte_controle(os.path.join(ici, "carte_controle.bmp"), d, kind, res)
    except Exception as e:
        log("   (carte de controle non ecrite : %s)" % e)

    # ---- 4. eau : sonde d'orientation des fichiers OBJ, puis maillages riviere + lac
    if not eal.does_directory_exist(DOSSIER_ASSETS):
        eal.make_directory(DOSSIER_ASSETS)
    tools = unreal.AssetToolsHelpers.get_asset_tools()

    def importer_obj(fichier, nom):
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ui.set_editor_property("import_as_skeletal", False)
        ui.set_editor_property("import_animations", False)
        sm = ui.get_editor_property("static_mesh_import_data")
        sm.set_editor_property("combine_meshes", True)
        sm.set_editor_property("auto_generate_collision", False)
        sm.set_editor_property("generate_lightmap_u_vs", False)
        try:
            sm.set_editor_property("remove_degenerates", False)
        except Exception:
            pass
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", fichier)
        t.set_editor_property("destination_path", DOSSIER_ASSETS)
        t.set_editor_property("destination_name", nom)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", False)
        t.set_editor_property("options", ui)
        tools.import_asset_tasks([t])
        return eal.load_asset("%s/%s" % (DOSSIER_ASSETS, nom))

    def charger_materiau(liste):
        for p in liste:
            if eal.does_asset_exist(p):
                m = eal.load_asset(p)
                if m:
                    return m
        return None

    axes = None
    if (FAIRE_RIVIERE and res["riviere"]) or (FAIRE_LAC and lac):
        sonde = os.path.join(ici, "sonde.obj")
        with open(sonde, "w") as f:
            f.write("v 0 0 0\nv 100 0 0\nv 0 200 0\nv 0 0 300\nf 1 2 3\nf 1 3 4\nf 1 2 4\n")
        m = importer_obj(sonde, "SM_Sonde_Orientation")
        if m:
            b = m.get_bounding_box()
            mins, maxs = (b.min.x, b.min.y, b.min.z), (b.max.x, b.max.y, b.max.z)
            ext = [maxs[k] - mins[k] for k in range(3)]
            ech = sum(ext) / 600.0
            axes = []
            for k in range(3):
                src = min(range(3), key=lambda q: abs(ext[k] / max(ech, 1e-6) - (100.0, 200.0, 300.0)[q]))
                if min(abs(mins[k]), abs(maxs[k])) > 0.1 * max(ext[k], 1e-6):
                    axes = None                 # l'importeur a deplace le modele : on ne sait pas lire
                    break
                axes.append((src, 1.0 if abs(maxs[k]) >= abs(mins[k]) else -1.0, ech))
            if axes is None:
                axes = [(0, 1.0, 0.0), (0, 1.0, 0.0), (0, 1.0, 0.0)]
            if sorted(a[0] for a in axes) != [0, 1, 2] or min(a[2] for a in axes) <= 0:
                log("   ATTENTION : orientation des fichiers OBJ non comprise (%s / %s) : pas d'eau" % (mins, maxs))
                axes = None
            else:
                log("   orientation OBJ mesuree : %s" % ", ".join("UE %s = %s%s x%.2f" % ("XYZ"[k], "+-"[a[1] < 0], "xyz"[a[0]], a[2]) for k, a in enumerate(axes)))
            eal.delete_asset("%s/%s" % (DOSSIER_ASSETS, "SM_Sonde_Orientation"))
        try:
            os.remove(sonde)
        except Exception:
            pass

    def materiau_eau_chelles():
        """eau opaque : couleur profonde + petites vagues qui bougent (2 normales qui glissent)"""
        mel = unreal.MaterialEditingLibrary
        chemin = DOSSIER_ASSETS + "/M_Eau_Chelles"
        if eal.does_asset_exist(chemin):
            eal.delete_asset(chemin)
        mat = tools.create_asset("M_Eau_Chelles", DOSSIER_ASSETS, unreal.Material, unreal.MaterialFactoryNew())
        coul = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -500, -200)
        coul.set_editor_property("constant", unreal.LinearColor(COULEUR_EAU[0], COULEUR_EAU[1], COULEUR_EAU[2], 1.0))
        mel.connect_material_property(coul, "", unreal.MaterialProperty.MP_BASE_COLOR)
        rug = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 0)
        rug.set_editor_property("r", 0.06)
        mel.connect_material_property(rug, "", unreal.MaterialProperty.MP_ROUGHNESS)
        spec = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 100)
        spec.set_editor_property("r", 0.8)
        mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
        tex = eal.load_asset(TEXTURE_VAGUES) if eal.does_asset_exist(TEXTURE_VAGUES) else None
        if tex is not None:
            normale = str(tex.get_editor_property("compression_settings")).upper().find("NORMAL") >= 0
            somme = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -250, 300)
            for n, (vx, vy, tuile) in enumerate(((0.025, 0.010, 1.0), (-0.015, 0.022, 0.6))):
                uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1100, 250 + n * 250)
                uv.set_editor_property("u_tiling", tuile)
                uv.set_editor_property("v_tiling", tuile)
                pan = mel.create_material_expression(mat, unreal.MaterialExpressionPanner, -900, 250 + n * 250)
                pan.set_editor_property("speed_x", vx)
                pan.set_editor_property("speed_y", vy)
                mel.connect_material_expressions(uv, "", pan, "Coordinate")
                ts = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -650, 250 + n * 250)
                ts.set_editor_property("texture", tex)
                ts.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normale
                                       else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
                mel.connect_material_expressions(pan, "", ts, "UVs")
                mel.connect_material_expressions(ts, "RGB", somme, "A" if n == 0 else "B")
            if normale:
                mel.connect_material_property(somme, "", unreal.MaterialProperty.MP_NORMAL)
        mel.recompile_material(mat)
        eal.save_loaded_asset(mat)
        return mat

    mat_eau = None
    if EAU_MATERIAU_CHELLES and axes:
        try:
            mat_eau = materiau_eau_chelles()
        except Exception as e:
            log("   materiau d'eau Chelles non cree (%s) : materiau du pack a la place" % e)
    if mat_eau is None:
        mat_eau = charger_materiau(MATERIAU_EAU)
    mat_berge = charger_materiau(MATERIAU_BERGE)

    def poser_maillage(tris, nom, mat, label_acteur):
        if not tris or not axes:
            return None
        xs = [v[0] for t in tris for v in t]
        ys = [v[1] for t in tris for v in t]
        zz = [v[2] for t in tris for v in t]
        cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
        fichier = os.path.join(ici, nom + ".obj")
        # le repere "monde" des triangles : x = est, y = ysign * nord (comme les routes)
        tris_monde = [[(v[0], ysign * v[1], v[2], v[3], v[4]) for v in t] for t in tris]
        origine = (round(cx * 100.0), round(ysign * cy * 100.0), round(min(zz)))
        ecrire_obj(fichier, tris_monde, origine, axes)
        m = importer_obj(fichier, nom)
        if m is None:
            log("   ATTENTION : import de %s rate" % nom)
            return None
        if mat:
            m.set_material(0, mat)
        try:
            bs = m.get_editor_property("body_setup")
            if bs:
                bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
        except Exception:
            pass
        eal.save_loaded_asset(m)
        # recalage : le coin du modele doit tomber exactement au bon endroit
        b = m.get_bounding_box()
        attendu_min = (min(v[0] for t in tris_monde for v in t) * 100.0 - origine[0],
                       min(v[1] for t in tris_monde for v in t) * 100.0 - origine[1], 0.0)
        corr = unreal.Vector(attendu_min[0] - b.min.x, attendu_min[1] - b.min.y, attendu_min[2] - b.min.z)
        a = ell.spawn_actor_from_object(m, unreal.Vector(origine[0] + corr.x, origine[1] + corr.y, origine[2] + corr.z),
                                        unreal.Rotator(0, 0, 0))
        a.set_actor_label(label_acteur)
        a.set_folder_path(DOSSIER_NIVEAU)
        a.tags = [unreal.Name(TAG)]
        c = a.static_mesh_component
        c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        c.set_editor_property("cast_shadow", False)
        c.set_editor_property("can_ever_affect_navigation", False)
        if abs(corr.x) > 5 or abs(corr.y) > 5 or abs(corr.z) > 5:
            log("   %s recale de (%.0f, %.0f, %.0f) cm" % (label_acteur, corr.x, corr.y, corr.z))
        return a

    nb_eau = 0
    if FAIRE_RIVIERE and res["riviere"]:
        eau, berge = maillage_riviere(res["riviere"], z_de, EAU_AU_DESSUS_DU_SOL_CM)
        if poser_maillage(eau, "SM_Riviere_Eau", mat_eau, "Chelles_Riviere_Eau"):
            nb_eau += 1
        if poser_maillage(berge, "SM_Riviere_Berges", mat_berge, "Chelles_Riviere_Berges"):
            nb_eau += 1
    if FAIRE_LAC and lac:
        eau, berge = maillage_lac(lac, z_de, EAU_AU_DESSUS_DU_SOL_CM)
        if poser_maillage(eau, "SM_Lac_Eau", mat_eau, "Chelles_Lac_Eau"):
            nb_eau += 1
        if poser_maillage(berge, "SM_Lac_Berges", mat_berge, "Chelles_Lac_Berges"):
            nb_eau += 1
        if ancien_lac is not None:
            cacher(ancien_lac)
            n_p = 0
            for pa in pierres_lac:
                l = pa.get_actor_location()
                x, y = depuis_monde(l.x, l.y)
                ang = math.atan2(y - lac[1], x - lac[0])
                if math.hypot(x - lac[0], y - lac[1]) < rayon_lac(lac[2], ang) - 0.8:
                    cacher(pa)
                    n_p += 1
            log("   ancien lac rond cache (+ %d pierre(s) qui seraient dans l'eau)" % n_p)
    log("5/9 eau posee : %d maillage(s) (materiau eau : %s)" % (nb_eau, mat_eau.get_path_name() if mat_eau else "AUCUN"))

    # ce qui est dans l'eau (arbres, buissons poses un par un) : cache
    eau_g = res.get("eau")
    n_c = 0
    if eau_g:
        for a in ell.get_all_level_actors():
            if TAG in tags_de(a) or TAG_CACHE in tags_de(a):
                continue
            if genre_acteur(a) != VEGETAL or isinstance(a, unreal.InstancedFoliageActor):
                continue
            l = a.get_actor_location()
            ci, cj = cellule(*depuis_monde(l.x, l.y))
            if dans_carte(ci, cj) and eau_g[cj][ci]:
                cacher(a)
                n_c += 1
    if n_c:
        log("   %d plante(s) deja posee(s) dans le lit de la riviere / du lac : cachee(s)" % n_c)

    # ---- 5. modeles de plantes
    modeles = {}
    for cat, liste in MODELES.items():
        ok = []
        for chemin, (hmin, hmax) in liste:
            if not eal.does_asset_exist(chemin):
                continue
            m = eal.load_asset(chemin)
            if not isinstance(m, unreal.StaticMesh):
                continue
            b = m.get_bounding_box()
            h = b.max.z - b.min.z
            if h < 1:
                continue
            ok.append({"mesh": m, "h": h, "zmin": b.min.z, "lx": max(1.0, b.max.x - b.min.x), "cible": (hmin * 100, hmax * 100)})
        modeles[cat] = ok
    manquants = [c for c, l in modeles.items() if not l]
    log("   modeles trouves : %s%s" % (", ".join("%s %d" % (c, len(l)) for c, l in modeles.items() if l),
                                     (" ; AUCUN pour : " + ", ".join(manquants)) if manquants else ""))
    if not modeles.get("arbre_eau") and modeles.get("arbre"):
        modeles["arbre_eau"] = modeles["arbre"]

    # LOD automatiques (modeles qui n'en ont qu'un)
    if FAIRE_LOD:
        nb_lod = 0
        vus = set()
        for cat in ("arbre", "arbre_eau", "buisson", "haie", "pierre", "roseau", "fleur"):
            for info in modeles.get(cat, []):
                m = info["mesh"]
                p = m.get_path_name()
                if p in vus:
                    continue
                vus.add(p)
                try:
                    if unreal.EditorStaticMeshLibrary.get_lod_count(m) > 1:
                        continue
                    if unreal.EditorStaticMeshLibrary.get_number_verts(m, 0) < 800:
                        continue
                    opt = unreal.EditorScriptingMeshReductionOptions()
                    opt.set_editor_property("auto_compute_lod_screen_size", False)
                    reglages = []
                    for taille, pc in ((1.0, 1.0), (0.4, 0.5), (0.18, 0.25), (0.07, 0.1)):
                        r = unreal.EditorScriptingMeshReductionSettings()
                        r.set_editor_property("percent_triangles", pc)
                        r.set_editor_property("screen_size", taille)
                        reglages.append(r)
                    opt.set_editor_property("reduction_settings", reglages)
                    if unreal.EditorStaticMeshLibrary.set_lods(m, opt) > 1:
                        eal.save_loaded_asset(m)
                        if p not in etat["lod"]:
                            etat["lod"].append(p)
                        nb_lod += 1
                except Exception as e:
                    log("   LOD %s : %s" % (p, e))
        log("   LOD automatiques ajoutes sur %d modele(s)" % nb_lod)

    # ---- 6. groupes d'instances
    groupes = {}

    def ajouter(cat, info, X, Y, Z, yaw, sx, sy, sz):
        cle = (cat, info["mesh"].get_path_name())
        g = groupes.setdefault(cle, {"cat": cat, "mesh": info["mesh"], "t": []})
        g["t"].append(unreal.Transform(location=unreal.Vector(X, Y, Z), rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw),
                                       scale=unreal.Vector(sx, sy, sz)))

    rnd2 = random.Random(77500)
    for p in res["plantes"]:
        liste = modeles.get(p["modele"]) or modeles.get(p["cat"])
        if not liste:
            continue
        info = rnd2.choice(liste)
        s = rnd2.uniform(*info["cible"]) / info["h"] * p["ech"]
        X, Y = vers_monde(p["x"], p["y"])
        z = p["z"] if p["z"] is not None else z_de(p["x"], p["y"])
        enfonce = 0.15 * info["h"] * s if p["cat"] == "pierre" else 3.0
        if p["cat"] == "haie":
            r = math.radians(p["yaw"])          # sens de la haie dans le repere en m -> lacet Unreal
            yaw = math.degrees(math.atan2(ysign * math.sin(r), math.cos(r)))
            ajouter("haie", info, X, Y, z - info["zmin"] * s - 3.0, yaw, s * p["etire"], s * 0.85, s)
        else:
            ajouter(p["cat"], info, X, Y, z - info["zmin"] * s - enfonce, p["yaw"], s, s, s)

    # garde-corps des ponts
    if modeles.get("barriere"):
        info = modeles["barriere"][0]
        for pont in res["ponts"]:
            (cx, cy), (ux, uy), portee = pont["centre"], pont["dir"], pont["portee"]
            s = rnd2.uniform(*info["cible"]) / info["h"]
            lg = info["lx"] * s / 100.0
            nbp = max(1, int(math.ceil(portee / max(lg, 0.5))))
            for cote in (-1, 1):
                for k in range(nbp):
                    t = (k + 0.5 - nbp / 2) * lg
                    x = cx + ux * t - uy * (DEMI_LARGEUR_ROUTE_M + 0.2) * cote
                    y = cy + uy * t + ux * (DEMI_LARGEUR_ROUTE_M + 0.2) * cote
                    X, Y = vers_monde(x, y)
                    yaw = math.degrees(math.atan2(ysign * uy, ux))
                    ajouter("barriere", info, X, Y, z_de(x, y) + 3.0 - info["zmin"] * s, yaw, s, s, s)

    # anciennes plantes posees une par une -> instances (originaux caches, pas supprimes)
    n_anc = 0
    if REGROUPER_ANCIENNE_VEGETATION:
        for a in ell.get_all_level_actors():
            lab = label(a)
            if not lab.startswith(("CH_tree_", "CH_bush_", "CH_hedge_")) or TAG_CACHE in tags_de(a):
                continue
            if not isinstance(a, unreal.StaticMeshActor):
                continue
            c = a.static_mesh_component
            m = c.get_editor_property("static_mesh") if c else None
            if m is None:
                continue
            cat = "arbre" if lab.startswith("CH_tree_") else ("haie" if lab.startswith("CH_hedge_") else "buisson")
            cle = (cat, m.get_path_name())
            g = groupes.setdefault(cle, {"cat": cat, "mesh": m, "t": []})
            g["t"].append(a.get_actor_transform())
            cacher(a)
            n_anc += 1
        log("   %d ancienne(s) plante(s) CH_* regroupee(s) en instances (originaux caches)" % n_anc)

    total = 0
    for (cat, chemin), g in sorted(groupes.items()):
        cd, cf, ombres, coll = CATEGORIES[cat]
        a = ell.spawn_actor_from_class(unreal.ChellesVegetationActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        nom = chemin.split(".")[-1]
        a.set_actor_label("ChellesVeg_%s_%s" % (cat, nom))
        a.set_folder_path(DOSSIER_NIVEAU)
        a.tags = [unreal.Name(TAG)]
        a.set_editor_property("categorie", cat)
        n = a.remplir(g["mesh"], g["t"], float(cd), float(cf), ombres, coll)
        total += n
    log("7/9 %d plante(s)/objet(s) en %d groupe(s) d'instances" % (total, len(groupes)))

    # ---- 8. enregistrement
    etat["date"] = datetime.datetime.now().strftime("%d/%m/%Y %H:%M")
    json.dump(etat, open(etat_json, "w", encoding="utf-8"), indent=1)
    if ell.save_current_level():
        log("8/9 map Rachid enregistree.")
    else:
        log("8/9 ATTENTION : la map n'a pas pu etre enregistree (faites Ctrl+S dans Unreal).")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
    log("9/9 TERMINE. Ouvrez Unreal : dossier Chelles/VegetationEau dans le World Outliner. "
        "Annuler : ANNULER_VEGETATION_EAU.bat")


def _quitter_si_bat():
    """lance par le .bat : l'editeur se ferme tout seul a la fin"""
    import unreal as _u
    if os.environ.get("CHELLES_QUITTER") == "1":
        try:
            _u.SystemLibrary.quit_editor()
        except Exception:
            pass


try:
    import unreal as _unreal_present  # noqa: F401   (absent hors d'Unreal : rien ne se lance)
except ImportError:
    _unreal_present = None
if _unreal_present is not None:
    try:
        executer_dans_unreal()
    except Exception:
        msg = "ERREUR :\n" + traceback.format_exc()
        _unreal_present.log_error(msg)
        try:
            p = os.path.join(_unreal_present.SystemLibrary.get_project_directory(), "Tools", "VegetationEau", "vegetation_eau.log")
            open(p, "a", encoding="utf-8").write(msg + "\n")
        except Exception:
            pass
    _quitter_si_bat()
