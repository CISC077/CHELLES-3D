# -*- coding: utf-8 -*-
"""Calculs communs (sans Unreal) pour l'habillage des batiments de Rachid.

- empreintes des batiments simples (memes donnees OSM et memes regles que Tools/build_buildings.py)
- rectangle minimal autour de chaque empreinte (longueur, largeur, angle)
- tri des modeles de maisons / immeubles des packs
- correspondance de volume batiment simple <-> modele de pack
"""
import math
import re

# ------------------------------------------------------------------ reglages (modifiables)
TOLERANCE = 0.25        # 25 % d'ecart maximum sur la longueur et la largeur
TOLERANCE_HAUTEUR = 0.35  # 35 % sur la hauteur (la hauteur des batiments simples est souvent estimee : 9,2 m par defaut)
REMPLISSAGE_MIN = 0.78  # l'empreinte doit remplir au moins 78 % de son rectangle (sinon forme en L, en U...)
MARGE_COUVRE = 1.03     # le modele pose est 3 % plus grand que le volume simple pour bien le cacher
MARGE_HAUT = 1.02
PENALITE_REPETITION = 0.012  # pour varier : chaque reutilisation d'un meme modele le rend un peu moins prioritaire
COTE_MIN_CM = 300.0     # un modele de batiment fait au moins 3 m de cote et de haut
HAUT_MIN_CM = 300.0
COTE_MAX_CM = 25000.0

GROUND_H = 3.2
FLOOR_H = 3.0

# dossiers du Content qui ne contiennent pas de batiments a poser
DOSSIERS_EXCLUS = (
    "/Game/Chelles/", "/Game/Maps/", "/Game/Developers/", "/Game/Collections/", "/Game/FPS_Game/",
    "/Game/Personnages/", "/Game/Voitures/", "/Game/Vehicles/", "/Game/VehicleAdv/", "/Game/VehicleVarietyPack/",
    "/Game/VehicleVarietyVol2/", "/Game/DV_", "/Game/DriveableCar_basic/", "/Game/Bus_sity/", "/Game/Weapon/",
    "/Game/FPS_", "/Game/FPSAssets/", "/Game/Animations/", "/Game/AnimStarterPack/", "/Game/Animalia/",
    "/Game/GoodSky/", "/Game/Clouds_and_Skies/", "/Game/P_Snow/", "/Game/GrassLandscape/", "/Game/GrassSV_Vol2/",
    "/Game/grass_patches/", "/Game/Flowers_Plants_Part1/", "/Game/Nature_Pack/", "/Game/PN_Fern/",
    "/Game/Stylized_Landscape/", "/Game/Terrain/", "/Game/UI/", "/Game/ChellesMenu/", "/Game/Audio/", "/Game/Sounds/",
    "/Game/Fence_Pack_", "/Game/ModularFences/", "/Game/AdvancedRoads/", "/Game/DecalRoadsRVT/", "/Game/Remplacements/",
    "/Game/SuperheroFlight/", "/Game/SuperPowers/", "/Game/MagicModule/", "/Game/Effects/", "/Game/Jet_Packs/",
    "/Game/HorseStarterKit/", "/Game/IMAGES/", "/Game/Christmas_pack/",
)

# mots qui disent "c'est un morceau, pas un batiment entier"
MOTS_MORCEAU = (
    "wall", "mur_", "floor", "ground", "window", "fenetre", "door", "porte", "stair", "escalier", "corner",
    "trim", "column", "pillar", "pilier", "fence", "rail", "beam", "plank", "plane", "landscape", "terrain",
    "road", "street", "sidewalk", "trottoir", "rock", "cliff", "mountain", "tree", "bush", "grass", "cloud",
    "sky", "sphere", "frame", "glass", "interior", "_int_", "balcon", "awning", "sign", "pipe", "lamp", "car_",
    "vehicle", "bus", "bridge", "dock", "pier", "boat", "ship", "water", "decal", "ucx_", "proxy", "collision",
    "chimney", "cheminee", "gutter", "molding", "cornice", "ledge", "panel", "pane_", "shutter", "volet",
    "railing", "step", "curb", "pavement", "tile_", "brick_", "parapet", "antenna", "ac_unit", "vent", "duct",
    "furniture", "table", "chair", "bed", "sofa", "shelf", "crate", "box_", "barrel", "pallet", "debris",
    "garage_door", "gate", "portail", "arch", "roof_", "toit_", "_roof", "dome", "spire", "tent", "sky_",
)
# mots qui disent "c'est probablement un batiment entier"
MOTS_BATIMENT = (
    "house", "maison", "building", "bld", "bldg", "apartment", "appart", "immeuble", "home", "villa", "hotel",
    "shop", "store", "magasin", "tower", "tour", "cottage", "church", "eglise", "poste", "mairie", "supermarket",
    "market", "block", "residence", "office", "bureau", "cafe", "bar_", "restaurant", "school", "ecole",
    "station", "hangar", "barn", "warehouse", "entrepot", "pavillon", "khrush", "panel_house", "bat_",
    "batiment", "manor", "mansion", "cabin", "hut", "garage", "kiosk", "pharmacy", "bakery", "boulangerie",
)


def nettoyer(nom):
    return re.sub(r"[^a-z0-9_]", "_", nom.lower())


def nature_modele(chemin, nom, x, y, z):
    """'batiment', 'probable' ou None selon le nom et la taille du modele (cm)."""
    if any(chemin.startswith(d) for d in DOSSIERS_EXCLUS):
        return None
    lo, la = max(x, y), min(x, y)
    if la < COTE_MIN_CM or z < HAUT_MIN_CM or lo > COTE_MAX_CM or z > COTE_MAX_CM:
        return None
    n = "_" + nettoyer(nom) + "_"
    if any(m in n for m in MOTS_MORCEAU) and not any(m in n for m in ("house", "building", "maison", "immeuble")):
        return None
    if la < 0.08 * lo:  # tres plat : un mur, une facade
        return None
    if any(m in n for m in MOTS_BATIMENT):
        return "batiment"
    if la >= 600 and z >= 500:
        return "probable"
    return None


# ------------------------------------------------------------------ geometrie
def _hull(points):
    pts = sorted(set(points))
    if len(pts) <= 2:
        return pts

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def aire(p):
    return abs(sum(p[i][0] * p[(i + 1) % len(p)][1] - p[(i + 1) % len(p)][0] * p[i][1] for i in range(len(p)))) / 2


def rectangle_minimal(p):
    """Rectangle d'aire minimale autour du polygone p (m) -> (cx, cy, longueur, largeur, angle_rad du grand cote)."""
    h = _hull(p)
    best = None
    for i in range(len(h)):
        a, b = h[i], h[(i + 1) % len(h)]
        ang = math.atan2(b[1] - a[1], b[0] - a[0])
        c, s = math.cos(ang), math.sin(ang)
        us = [x * c + y * s for x, y in h]
        vs = [-x * s + y * c for x, y in h]
        w, l = max(us) - min(us), max(vs) - min(vs)
        if best is None or w * l < best[0]:
            uc, vc = (max(us) + min(us)) / 2, (max(vs) + min(vs)) / 2
            cx, cy = uc * c - vc * s, uc * s + vc * c
            if w >= l:
                best = (w * l, cx, cy, w, l, ang)
            else:
                best = (w * l, cx, cy, l, w, ang + math.pi / 2)
    _, cx, cy, lo, la, ang = best
    return cx, cy, lo, la, ang


def _nettoyer_poly(pts):
    p = []
    for q in (pts[:-1] if pts[0] == pts[-1] else pts):
        if not p or math.hypot(q[0] - p[-1][0], q[1] - p[-1][1]) > 0.05:
            p.append(q)
    return p


def _hauteur_osm(tags, defaut=8.0):
    try:
        if "height" in tags:
            return float(str(tags["height"]).split()[0].replace(",", "."))
        if "building:levels" in tags:
            return float(tags["building:levels"]) * 3.0 + 1
    except ValueError:
        pass
    return defaut


def _is_rect(p):
    if len(p) != 4:
        return False
    for i in range(4):
        a = (p[i][0] - p[i - 1][0], p[i][1] - p[i - 1][1])
        b = (p[(i + 1) % 4][0] - p[i][0], p[(i + 1) % 4][1] - p[i][1])
        la, lb = math.hypot(*a), math.hypot(*b)
        if la < 1e-6 or lb < 1e-6 or abs(a[0] * b[0] + a[1] * b[1]) / (la * lb) > 0.2:
            return False
    return True


def batiments_osm(data, centre=(48.8795, 2.5912)):
    """Batiments simples tels que build_buildings.py les a construits (repere FBX, metres)."""
    lat0, lon0 = centre
    m_lat = 111320.0
    m_lon = 111320.0 * math.cos(math.radians(lat0))
    res = []
    for el in data.get("elements", []):
        if el.get("type") != "way" or "geometry" not in el:
            continue
        pts = [((g["lon"] - lon0) * m_lon, (g["lat"] - lat0) * m_lat) for g in el["geometry"]]
        p = _nettoyer_poly(pts)
        if len(p) < 3 or aire(p) < 4:
            continue
        h = _hauteur_osm(el.get("tags", {}))
        floors = max(1, round((h - GROUND_H) / FLOOR_H))
        h = GROUND_H + floors * FLOOR_H
        cx, cy, lo, la, ang = rectangle_minimal(p)
        toit = 0.0
        if _is_rect(p):
            toit = max(1.5, min(4.5, la * 0.3))
        res.append({
            "id": el["id"], "cx": cx, "cy": cy, "longueur": lo, "largeur": la, "angle": ang,
            "hauteur_murs": h, "hauteur": h + toit, "toit_pointu": toit > 0,
            "remplissage": aire(p) / max(lo * la, 1e-6), "aire": aire(p),
            "tags": {k: v for k, v in el.get("tags", {}).items() if k in ("building", "addr:street", "addr:housenumber", "name")},
        })
    return res


# ------------------------------------------------------------------ correspondance
def meilleur_modele(cible, modeles, usages, tolerance=None, tolerance_h=None):
    """cible : dict longueur/largeur/hauteur (cm). modeles : liste de dict (lo, la, z, ...).
    Rend (modele, ecart, (sL, sW, sH)) ou None."""
    lim = math.log(1.0 + (TOLERANCE if tolerance is None else tolerance))
    lim_h = math.log(1.0 + (TOLERANCE_HAUTEUR if tolerance_h is None else tolerance_h))
    best = None
    L, W, H = cible["longueur"], cible["largeur"], cible["hauteur"]
    for m in modeles:
        sl, sw, sh = L / m["lo"], W / m["la"], H / m["z"]
        e_sol = max(abs(math.log(sl)), abs(math.log(sw)))
        e_h = abs(math.log(sh))
        if e_sol > lim or e_h > lim_h:
            continue
        e = max(e_sol, 0.6 * e_h)
        score = e + PENALITE_REPETITION * usages.get(m["chemin"], 0)
        if best is None or score < best[0]:
            best = (score, m, e, (sl, sw, sh))
    if best is None:
        return None
    return best[1], best[2], best[3]
