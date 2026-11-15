# -*- coding: utf-8 -*-
"""ETAPE 1 - ANALYSE (lecture seule) : quels modeles de maisons / immeubles des packs ont le meme volume
que les batiments simples de la carte /Game/Maps/Rachid ?

Lancer : double-clic sur 1_ANALYSER.bat (Unreal FERME)
     ou, Unreal ouvert sur Rachid : File > Execute Python Script... > Tools/HabillageBatiments/analyser_habillage.py

Ce script NE MODIFIE RIEN (ni la carte, ni les packs). Il ecrit seulement :
  Tools/HabillageBatiments/Rapport/rapport_habillage.txt  (le resume a lire)
  Tools/HabillageBatiments/Rapport/modeles_trouves.txt    (tous les batiments trouves dans les packs, avec leurs tailles)
  Tools/HabillageBatiments/Rapport/correspondances.json   (ce que l'etape 2 posera)

Batiments simples pris en compte :
  - ceux des tuiles Chelles_Tile_* (construits par chelles_complet.py a partir d'OpenStreetMap) ;
  - les cubes et les objets "Batiment..." poses a la main (au moins 3 m de cote, sans objet attache).
On saute les batiments ou un vrai modele est deja pose, et les formes trop irregulieres (en L, en U...).
"""
import importlib
import json
import math
import os
import random
import re
import sys
import time
import traceback

import unreal

CARTE = "/Game/Maps/Rachid"
TAG_AJOUTE = "ChellesHabillage"
GRAINE = 77500

ell = unreal.EditorLevelLibrary
eal = unreal.EditorAssetLibrary
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ICI = os.path.join(PROJET, "Tools", "HabillageBatiments")
OUT = os.path.join(PROJET, "Tools", "out")
RAPPORT = os.path.join(ICI, "Rapport")
JOURNAL = os.path.join(ICI, "habillage.log")

if ICI not in sys.path:
    sys.path.insert(0, ICI)
import habillage_commun as hc  # noqa: E402
importlib.reload(hc)

TRAVERSABLE_IDS = {111032114, 111031897, 111022267, 111003037, 111023866, 111029096}
NOMS_CUBE = ("cube", "shape_cube", "1m_cube", "sm_cube", "box", "batiment")
BP_MESURES = []  # Blueprints poses un instant pour les mesurer


def log(msg):
    unreal.log("[Habillage] " + str(msg))
    try:
        os.makedirs(ICI, exist_ok=True)
        with open(JOURNAL, "a", encoding="utf-8") as f:
            f.write(time.strftime("%H:%M:%S ") + str(msg) + "\n")
    except Exception:
        pass


def a_tag(a, tag):
    try:
        return any(str(t) == tag for t in a.get_editor_property("tags"))
    except Exception:
        return False


# ------------------------------------------------------------------ 1. modeles des packs
def valeur_tag(ad, nom):
    for f in (lambda: ad.get_tag_value(nom),
              lambda: unreal.AssetRegistryHelpers.get_tag_value(ad, nom)):
        try:
            r = f()
        except Exception:
            continue
        if isinstance(r, str) and r:
            return r
        if isinstance(r, (tuple, list)):
            for v in r:
                if isinstance(v, str) and v:
                    return v
    return None


def bornes_visibles(acteur):
    """(min, max) des modeles visibles d'un acteur (Static Mesh, instances, personnages) ou None."""
    mn = mx = None
    for classe in (unreal.StaticMeshComponent, unreal.SkeletalMeshComponent):
        for c in acteur.get_components_by_class(classe):
            try:
                if isinstance(c, unreal.StaticMeshComponent) and c.get_editor_property("static_mesh") is None:
                    continue
                if not c.get_editor_property("visible") or c.get_editor_property("hidden_in_game"):
                    continue
                o, e, _r = unreal.SystemLibrary.get_component_bounds(c)
            except Exception:
                continue
            lo = (o.x - e.x, o.y - e.y, o.z - e.z)
            hi = (o.x + e.x, o.y + e.y, o.z + e.z)
            mn = lo if mn is None else tuple(min(p, q) for p, q in zip(mn, lo))
            mx = hi if mx is None else tuple(max(p, q) for p, q in zip(mx, hi))
    if mn is None:
        return None
    return mn, mx


def modele_depuis_bornes(chemin, nom, nature, mn, mx, ox=0.0, oy=0.0, oz=0.0, blueprint=False):
    x, y, z = mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2]
    return {
        "chemin": chemin, "nom": nom, "pack": hc.pack_de(chemin), "nature": nature, "blueprint": blueprint,
        "x": x, "y": y, "z": z, "lo": max(x, y), "la": min(x, y), "axe_long": "X" if x >= y else "Y",
        "centre": [(mx[0] + mn[0]) / 2 - ox, (mx[1] + mn[1]) / 2 - oy], "bas": mn[2] - oz,
    }


def inventaire_packs():
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    flt = unreal.ARFilter(class_names=["StaticMesh"], package_paths=["/Game"], recursive_paths=True)
    tous = ar.get_assets(flt)
    log("1/4 %d modeles 3D (StaticMesh) dans le Content" % len(tous))
    modeles = []
    for ad in tous:
        paquet = str(ad.package_name)
        nom = str(ad.asset_name)
        if hc.nature_modele(paquet, nom, 1e4, 1e4, 1e4) is None:  # tri sur le nom avant d'ouvrir le modele
            continue
        m = eal.load_asset(paquet)
        if not isinstance(m, unreal.StaticMesh):
            continue
        b = m.get_bounding_box()
        mn, mx = (b.min.x, b.min.y, b.min.z), (b.max.x, b.max.y, b.max.z)
        nature = hc.nature_modele(paquet, nom, mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2])
        if nature:
            modeles.append(modele_depuis_bornes(paquet, nom, nature, mn, mx))
    log("   %d modeles 3D de maisons / immeubles retenus" % len(modeles))

    # Blueprints de batiments entiers : poses un instant tres loin sous la carte pour les mesurer
    flt = unreal.ARFilter(class_names=["Blueprint"], package_paths=["/Game"], recursive_paths=True)
    n_bp = 0
    ici = unreal.Vector(0.0, 0.0, -500000.0)
    for ad in ar.get_assets(flt):
        paquet = str(ad.package_name)
        if not hc.blueprint_candidat(paquet):
            continue
        try:
            classe = eal.load_blueprint_class(paquet)
            a = ell.spawn_actor_from_class(classe, ici, unreal.Rotator(0.0, 0.0, 0.0)) if classe else None
        except Exception:
            a = None
        if a is None:
            continue
        BP_MESURES.append(1)
        try:
            bornes = bornes_visibles(a)
        finally:
            ell.destroy_actor(a)
        if bornes is None:
            continue
        mn, mx = bornes
        nom = str(ad.asset_name)
        nature = hc.nature_modele(paquet, nom, mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2], blueprint=True)
        if nature:
            modeles.append(modele_depuis_bornes(paquet, nom, nature, mn, mx, ici.x, ici.y, ici.z, blueprint=True))
            n_bp += 1
    log("   %d Blueprints d'immeubles retenus" % n_bp)
    return modeles


# ------------------------------------------------------------------ 2. batiments simples de Rachid
def correspondance_axes(m, bx):
    """Comment le repere des donnees (FBX, cm) devient le repere du modele importe : local[i] = signe[i] * fbx[perm[i]]."""
    import itertools
    b = m.get_bounding_box()
    lc = [(b.max.x + b.min.x) / 2, (b.max.y + b.min.y) / 2, (b.max.z + b.min.z) / 2]
    le = [(b.max.x - b.min.x) / 2, (b.max.y - b.min.y) / 2, (b.max.z - b.min.z) / 2]
    meilleur = None
    for perm in itertools.permutations(range(3)):
        for signes in itertools.product((1, -1), repeat=3):
            err = sum(abs(le[i] - bx[3 + perm[i]]) + abs(lc[i] - signes[i] * bx[perm[i]]) for i in range(3))
            if meilleur is None or err < meilleur[0]:
                meilleur = (err, perm, signes)
    return meilleur


def tuiles_de_la_carte(acteurs, boites):
    """Acteurs de la carte qui affichent une tuile de batiments (trouves par leur modele, pas par leur nom)."""
    res = []
    for a in acteurs:
        if not isinstance(a, unreal.StaticMeshActor):
            continue
        try:
            m = a.static_mesh_component.get_editor_property("static_mesh")
        except Exception:
            continue
        if m is None or m.get_name() not in boites:
            continue
        err, perm, signes = correspondance_axes(m, boites[m.get_name()])
        t = a.get_actor_transform()
        r = a.get_actor_rotation()
        s = a.get_actor_scale3d()
        log("   tuile %-26s acteur \"%s\" pos (%.0f, %.0f, %.0f) rot (%.0f, %.0f, %.0f) echelle (%.2f, %.2f, %.2f)%s" % (
            m.get_name(), a.get_actor_label(), t.translation.x, t.translation.y, t.translation.z,
            r.roll, r.pitch, r.yaw, s.x, s.y, s.z, "" if err < 150 else "  -> modele different des donnees, ignoree"))
        if err < 150:
            res.append((m.get_name(), a, perm, signes))
    return res


def cibles_osm(acteurs):
    osm = os.path.join(OUT, "osm.json")
    man_path = os.path.join(OUT, "manifest.json")
    if not (os.path.exists(osm) and os.path.exists(man_path)):
        log("   osm.json / manifest.json introuvables : batiments des tuiles ignores")
        return []
    man = json.load(open(man_path, encoding="utf-8"))
    boites = {t["name"]: t["box_cm"] for t in man["tiles"] if t.get("buildings")}
    tuiles = tuiles_de_la_carte(acteurs, boites)
    presentes = {n for n, _a, _p, _s in tuiles}
    for n in sorted(boites):
        if n not in presentes:
            log("   tuile %s : pas dans la carte" % n)
    if not tuiles:
        return []
    # chaque batiment OSM -> sa tuile (meme regle que build_buildings.py : boite de la tuile)
    batiments = hc.batiments_osm(json.load(open(osm, encoding="utf-8")))
    par_tuile = {}
    for b in batiments:
        fx, fy = b["cx"] * 100, b["cy"] * 100
        if b["id"] in TRAVERSABLE_IDS:
            noms = ["Chelles_Tile_Traversable"]
        else:
            noms = [n for n, bx in boites.items() if n != "Chelles_Tile_Traversable"
                    and abs(fx - bx[0]) <= bx[3] + 1 and abs(fy - bx[1]) <= bx[4] + 1]
        if noms:
            n = min(noms, key=lambda k: math.hypot(fx - boites[k][0], fy - boites[k][1]))
            par_tuile.setdefault(n, []).append(b)

    cibles = []
    for nom, a, perm, signes in tuiles:
        t = a.get_actor_transform()

        def monde(fx, fy, fz):
            f = (fx, fy, fz)
            return t.transform_location(unreal.Vector(*[signes[i] * f[perm[i]] for i in range(3)]))

        for b in par_tuile.get(nom, []):
            cx, cy = b["cx"] * 100, b["cy"] * 100
            ux, uy = math.cos(b["angle"]), math.sin(b["angle"])
            p0 = monde(cx, cy, 0.0)
            pu = monde(cx + ux * 100, cy + uy * 100, 0.0)
            pv = monde(cx - uy * 100, cy + ux * 100, 0.0)
            pz = monde(cx, cy, 100.0)
            du = (pu.x - p0.x, pu.y - p0.y, pu.z - p0.z)
            su = math.sqrt(sum(v * v for v in du)) / 100
            sv = math.sqrt((pv.x - p0.x) ** 2 + (pv.y - p0.y) ** 2 + (pv.z - p0.z) ** 2) / 100
            sz = math.sqrt((pz.x - p0.x) ** 2 + (pz.y - p0.y) ** 2 + (pz.z - p0.z) ** 2) / 100
            if abs(du[2]) > 5 or abs(pz.z - p0.z) < 90 * sz:
                continue  # tuile penchee : on ne sait pas poser droit
            cibles.append({
                "source": "osm", "id": b["id"], "tuile": nom,
                "x": p0.x, "y": p0.y, "sol": p0.z, "yaw": math.degrees(math.atan2(du[1], du[0])),
                "longueur": b["longueur"] * 100 * su, "largeur": b["largeur"] * 100 * sv,
                "hauteur": b["hauteur"] * 100 * sz, "remplissage": b["remplissage"], "infos": b["tags"],
            })
    log("   %d tuile(s) de batiments trouvee(s) dans la carte" % len(tuiles))
    return cibles


def cibles_acteurs(acteurs):
    res = []
    for a in acteurs:
        if not isinstance(a, unreal.StaticMeshActor) or a_tag(a, TAG_AJOUTE):
            continue
        lbl = a.get_actor_label()
        if lbl.startswith(("Chelles_", "CH_")) or lbl == "Floor":
            continue
        try:
            m = a.static_mesh_component.get_editor_property("static_mesh")
        except Exception:
            m = None
        if m is None:
            continue
        nom = m.get_name().lower()
        if not (nom.startswith(NOMS_CUBE) or lbl.lower().startswith("batiment")):
            continue
        try:
            if a.get_attached_actors():
                continue
        except Exception:
            pass
        r = a.get_actor_rotation()
        if abs(r.pitch) > 1 or abs(r.roll) > 1:
            continue
        b = m.get_bounding_box()
        s = a.get_actor_scale3d()
        dx, dy, dz = (b.max.x - b.min.x) * abs(s.x), (b.max.y - b.min.y) * abs(s.y), (b.max.z - b.min.z) * abs(s.z)
        if min(dx, dy) < 300 or dz < 300 or max(dx, dy) > 25000:
            continue
        centre = a.get_actor_transform().transform_location(
            unreal.Vector((b.max.x + b.min.x) / 2, (b.max.y + b.min.y) / 2, (b.max.z + b.min.z) / 2))
        o, e = a.get_actor_bounds(False)
        yaw = r.yaw if dx >= dy else r.yaw + 90
        res.append({
            "source": "acteur", "id": lbl, "x": centre.x, "y": centre.y, "sol": o.z - e.z, "yaw": yaw,
            "longueur": max(dx, dy), "largeur": min(dx, dy), "hauteur": dz, "remplissage": 1.0,
            "infos": {"modele": m.get_path_name()},
        })
    return res


def vrais_batiments(acteurs, a_ignorer):
    """Boites 2D des grands modeles deja poses (vrais batiments des packs, Blueprints...)."""
    res = []
    for a in acteurs:
        lbl = a.get_actor_label()
        if lbl.startswith(("Chelles_", "CH_")) or lbl in a_ignorer or a_tag(a, TAG_AJOUTE):
            continue
        if isinstance(a, (unreal.Pawn, unreal.Volume, unreal.Light, unreal.LandscapeProxy, unreal.Brush)):
            continue
        t = (a.get_class().get_name() + " " + lbl).lower()
        if any(k in t for k in ("sky", "cloud", "fog", "atmos", "snow", "neige", "post", "landscape", "navmesh",
                                "water", "spawner", "volume", "trigger", "zone", "foliage", "road", "route",
                                "trottoir", "sidewalk")):
            continue
        bornes = bornes_visibles(a)
        if bornes is None:
            continue
        mn, mx = bornes
        if mx[0] - mn[0] < 300 or mx[1] - mn[1] < 300 or mx[2] - mn[2] < 300:
            continue
        if mx[0] - mn[0] > 60000 or mx[1] - mn[1] > 60000:
            continue
        res.append((mn[0], mn[1], mx[0], mx[1], lbl))
    return res


def boite_cible(c):
    a = math.radians(c["yaw"])
    hx = abs(math.cos(a)) * c["longueur"] / 2 + abs(math.sin(a)) * c["largeur"] / 2
    hy = abs(math.sin(a)) * c["longueur"] / 2 + abs(math.cos(a)) * c["largeur"] / 2
    return c["x"] - hx, c["y"] - hy, c["x"] + hx, c["y"] + hy


def boites_sprites(acteurs):
    """Boites 3D (min, max, nom) des Sprites (images Paper2D) poses dans la carte."""
    classes = [getattr(unreal, n) for n in ("PaperSpriteComponent", "PaperFlipbookComponent",
                                             "PaperGroupedSpriteComponent", "PaperTileMapComponent") if hasattr(unreal, n)]
    res = []
    for a in acteurs:
        comps = []
        for k in classes:
            try:
                comps += list(a.get_components_by_class(k))
            except Exception:
                pass
        if not comps and "sprite" not in a.get_class().get_name().lower():
            continue
        if not comps:
            try:
                o, e = a.get_actor_bounds(False)
                res.append(((o.x - e.x, o.y - e.y, o.z - e.z), (o.x + e.x, o.y + e.y, o.z + e.z), a.get_actor_label()))
            except Exception:
                pass
            continue
        for c in comps:
            try:
                o, e, _r = unreal.SystemLibrary.get_component_bounds(c)
            except Exception:
                continue
            res.append(((o.x - e.x, o.y - e.y, o.z - e.z), (o.x + e.x, o.y + e.y, o.z + e.z), a.get_actor_label()))
    return res


def touche_sprite(c, sprites, marge=150.0):
    """Nom du Sprite colle sur (ou tout pres de) ce batiment, sinon None."""
    x0, y0, x1, y1 = boite_cible(c)
    z0, z1 = c["sol"] - marge, c["sol"] + c["hauteur"] + marge
    for mn, mx, nom in sprites:
        if (mn[0] <= x1 + marge and mx[0] >= x0 - marge and mn[1] <= y1 + marge and mx[1] >= y0 - marge
                and mn[2] <= z1 and mx[2] >= z0):
            return nom
    return None


def deja_habille(c, boites):
    x0, y0, x1, y1 = boite_cible(c)
    surf = max((x1 - x0) * (y1 - y0), 1.0)
    for bx0, by0, bx1, by1, lbl in boites:
        w = min(x1, bx1) - max(x0, bx0)
        h = min(y1, by1) - max(y0, by0)
        if w > 0 and h > 0 and w * h / surf >= 0.5:
            return lbl
    return None


# ------------------------------------------------------------------ 3. correspondances
def decouper(c, modeles, usages):
    """Un modele pour tout le batiment ; sinon, pour un batiment long (barre, rangee de maisons),
    plusieurs modeles cote a cote dans la longueur. Rend [(morceau, (modele, ecart, echelles))] ou None."""
    r = hc.meilleur_modele(c, modeles, usages)
    if r is not None:
        return [(c, r)]
    if c["longueur"] < 1.5 * c["largeur"]:
        return None
    a = math.radians(c["yaw"])
    for k in range(2, hc.MORCEAUX_MAX + 1):
        lg = c["longueur"] / k
        if lg < 0.6 * c["largeur"]:
            break
        essai, u = [], dict(usages)
        for i in range(k):
            d = (i + 0.5) * lg - c["longueur"] / 2
            m = dict(c, longueur=lg, x=c["x"] + d * math.cos(a), y=c["y"] + d * math.sin(a),
                     id="%s/%d" % (c["id"], i + 1))
            if lg < m["largeur"]:  # le morceau est plus large que long : on le tourne
                m["longueur"], m["largeur"], m["yaw"] = m["largeur"], lg, c["yaw"] + 90.0
            r = hc.meilleur_modele(m, modeles, u)
            if r is None:
                break
            u[r[0]["chemin"]] = u.get(r[0]["chemin"], 0) + 1
            essai.append((m, r))
        if len(essai) == k:
            return essai
    return None


def placement(c, m, s, retourne):
    sl, sw, sh = s
    if m["axe_long"] == "X":
        sx, sy = sl * hc.MARGE_COUVRE, sw * hc.MARGE_COUVRE
        yaw = c["yaw"]
    else:
        sx, sy = sw * hc.MARGE_COUVRE, sl * hc.MARGE_COUVRE
        yaw = c["yaw"] - 90.0
    if retourne:
        yaw += 180.0
    sz = sh * hc.MARGE_HAUT
    yaw = (yaw + 180.0) % 360.0 - 180.0
    cx, cy = m["centre"][0] * sx, m["centre"][1] * sy
    a = math.radians(yaw)
    x = c["x"] - (cx * math.cos(a) - cy * math.sin(a))
    y = c["y"] - (cx * math.sin(a) + cy * math.cos(a))
    z = c["sol"] - m["bas"] * sz
    return [x, y, z], yaw, [sx, sy, sz]


def main():
    open(JOURNAL, "w").close()
    log("---- ANALYSE (rien n'est modifie) ----")
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        log("chargement de %s ..." % CARTE)
        ell.load_level(CARTE)
    acteurs = ell.get_all_level_actors()

    modeles = inventaire_packs()

    log("2/4 batiments simples de Rachid ...")
    cibles = cibles_osm(acteurs)
    n_osm = len(cibles)
    cibles += cibles_acteurs(acteurs)
    log("   %d batiments des tuiles OSM + %d cubes / batiments poses a la main" % (n_osm, len(cibles) - n_osm))

    log("3/4 calcul des correspondances de volume ...")
    sprites = boites_sprites(acteurs)
    log("   %d Sprites trouves : les batiments qui en portent ne seront PAS touches" % len(sprites))
    avec_sprites = []
    boites = vrais_batiments(acteurs, {c["id"] for c in cibles if c["source"] == "acteur"} | {"Floor"})
    boites_cubes = [boite_cible(c) + ("cube " + c["id"],) for c in cibles if c["source"] == "acteur"]
    rnd = random.Random(GRAINE)
    usages, poses = {}, []
    irreguliers, deja, sans, en_rangee = [], [], [], []
    for c in sorted(cibles, key=lambda c: -c["longueur"] * c["largeur"]):
        if c["remplissage"] < hc.REMPLISSAGE_MIN:
            irreguliers.append(c)
            continue
        sp = touche_sprite(c, sprites)
        if sp:
            c["sprite"] = sp
            avec_sprites.append(c)
            continue
        d = deja_habille(c, boites)
        if not d and c["source"] == "osm":
            d = deja_habille(c, boites_cubes)
        if d:
            c["deja"] = d
            deja.append(c)
            continue
        morceaux = decouper(c, modeles, usages)
        if morceaux is None:
            sans.append(c)
            continue
        if len(morceaux) > 1:
            en_rangee.append((c, len(morceaux)))
        for c, (m, ecart, s) in morceaux:
          usages[m["chemin"]] = usages.get(m["chemin"], 0) + 1
          loc, yaw, echelle = placement(c, m, s, rnd.random() < 0.5)
          poses.append({
              "source": c["source"], "id": c["id"], "modele": m["chemin"], "pack": m["pack"],
              "blueprint": m.get("blueprint", False),
              "position": loc, "yaw": yaw, "echelle": echelle, "ecart_pourcent": round(ecart * 100, 1),
              "simple_m": [round(c["longueur"] / 100, 1), round(c["largeur"] / 100, 1), round(c["hauteur"] / 100, 1)],
              "modele_m": [round(m["lo"] / 100, 1), round(m["la"] / 100, 1), round(m["z"] / 100, 1)],
              "infos": c.get("infos", {}),
          })

    log("4/4 ecriture du rapport ...")
    os.makedirs(RAPPORT, exist_ok=True)
    json.dump({"carte": CARTE, "date": time.strftime("%Y-%m-%d %H:%M"), "poses": poses},
              open(os.path.join(RAPPORT, "correspondances.json"), "w", encoding="utf-8"), indent=1, ensure_ascii=False)

    par_pack = {}
    for m in modeles:
        par_pack.setdefault(m["pack"], []).append(m)
    with open(os.path.join(RAPPORT, "modeles_trouves.txt"), "w", encoding="utf-8") as f:
        f.write("MAISONS ET IMMEUBLES TROUVES DANS LES PACKS (longueur x largeur x hauteur, en metres)\n\n")
        for pack in sorted(par_pack):
            f.write("== %s (%d)\n" % (pack, len(par_pack[pack])))
            for m in sorted(par_pack[pack], key=lambda m: m["nom"]):
                f.write("   %-45s %6.1f x %5.1f x %5.1f  %s%s\n" % (
                    m["nom"], m["lo"] / 100, m["la"] / 100, m["z"] / 100,
                    "(Blueprint) " if m.get("blueprint") else ("" if m["nature"] == "autorise" else "(nouveau pack) "),
                    "utilise %d fois" % usages[m["chemin"]] if m["chemin"] in usages else ""))
            f.write("\n")

    with open(os.path.join(RAPPORT, "rapport_habillage.txt"), "w", encoding="utf-8") as f:
        w = f.write
        w("HABILLAGE DES BATIMENTS DE RACHID - rapport du %s\n" % time.strftime("%d/%m/%Y %H:%M"))
        w("(rien n'a ete modifie : c'est seulement le calcul)\n\n")
        w("Modeles de maisons / immeubles trouves dans les packs : %d (detail : modeles_trouves.txt)\n" % len(modeles))
        for pack in sorted(par_pack, key=lambda p: -len(par_pack[p])):
            w("   %-32s %4d modele(s)\n" % (pack, len(par_pack[pack])))
        w("\nBatiments simples dans Rachid : %d\n" % len(cibles))
        w("   -> habilles par un modele de meme volume : %d (%d modeles poses)\n" % (len(cibles) - len(sans) - len(irreguliers) - len(deja) - len(avec_sprites), len(poses)))
        w("      (dont %d batiments longs habilles par plusieurs modeles cote a cote)\n" % len(en_rangee))
        w("   -> aucun modele assez proche (ecart > %d %%) : %d\n" % (round(hc.TOLERANCE * 100), len(sans)))
        w("   -> forme trop irreguliere (en L, en U...)  : %d\n" % len(irreguliers))
        w("   -> deja un vrai batiment pose dessus        : %d\n" % len(deja))
        w("   -> porte des Sprites (on n'y touche pas)    : %d\n" % len(avec_sprites))
        w("\nModeles les plus utilises :\n")
        for ch, n in sorted(usages.items(), key=lambda kv: -kv[1])[:25]:
            w("   %4d x %s\n" % (n, ch))
        w("\nDETAIL (longueur x largeur x hauteur en m : batiment simple -> modele du pack, ecart)\n")
        for p in poses:
            i = p["infos"]
            adr = " ".join(str(i[k]) for k in ("addr:housenumber", "addr:street", "name") if k in i)
            w("   %-14s %5.1f x %4.1f x %4.1f -> %5.1f x %4.1f x %4.1f  %4.1f %%  %s %s\n" % (
                p["id"], p["simple_m"][0], p["simple_m"][1], p["simple_m"][2],
                p["modele_m"][0], p["modele_m"][1], p["modele_m"][2], p["ecart_pourcent"],
                p["modele"].rsplit("/", 1)[-1], ("(" + adr + ")") if adr else ""))
        if avec_sprites:
            w("\nBATIMENTS AVEC SPRITES (on n'y touche pas)\n")
            for c in avec_sprites:
                w("   %-14s Sprite %s\n" % (c["id"], c["sprite"]))
        if deja:
            w("\nDEJA HABILLES (on n'y touche pas)\n")
            for c in deja:
                w("   %-14s sous %s\n" % (c["id"], c["deja"]))
        if sans:
            w("\nSANS MODELE ASSEZ PROCHE (les 40 plus grands)\n")
            for c in sans[:40]:
                w("   %-14s %5.1f x %4.1f x %4.1f m\n" % (c["id"], c["longueur"] / 100, c["largeur"] / 100, c["hauteur"] / 100))
    log("TERMINE : %d batiment(s) peuvent etre habilles sur %d (%d modeles a poser). Rapport : Tools\\HabillageBatiments\\Rapport\\rapport_habillage.txt"
        % (len(cibles) - len(sans) - len(irreguliers) - len(deja) - len(avec_sprites), len(cibles), len(poses)))


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    if BP_MESURES:
        # des Blueprints ont ete poses puis enleves pour les mesurer : on ferme SANS enregistrer
        # (sinon Unreal demanderait d'enregistrer Rachid alors que rien n'a change)
        os._exit(0)
    unreal.SystemLibrary.quit_editor()
