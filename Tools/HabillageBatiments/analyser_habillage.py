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


def inventaire_packs():
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    flt = unreal.ARFilter(class_names=["StaticMesh"], package_paths=["/Game"], recursive_paths=True)
    tous = ar.get_assets(flt)
    log("1/4 %d modeles 3D (StaticMesh) dans le Content" % len(tous))
    candidats = []
    sans_taille = 0
    charges = 0
    debut = time.time()
    for ad in tous:
        paquet = str(ad.package_name)
        nom = str(ad.asset_name)
        if any(paquet.startswith(d) for d in hc.DOSSIERS_EXCLUS):
            continue
        taille = valeur_tag(ad, "ApproxSize")
        dims = None
        if taille:
            n = re.findall(r"[\d.]+", taille)
            if len(n) >= 3:
                dims = [float(v) for v in n[:3]]
        if dims is None:
            sans_taille += 1
            if charges > 6000 or time.time() - debut > 900:
                continue
            m = eal.load_asset(paquet)
            charges += 1
            if not isinstance(m, unreal.StaticMesh):
                continue
            b = m.get_bounding_box()
            dims = [b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z]
        nature = hc.nature_modele(paquet + "/", nom, dims[0], dims[1], dims[2])
        if nature:
            candidats.append((paquet, nom, nature))
    if sans_taille:
        log("   %d modeles sans taille enregistree (%d ouverts pour les mesurer)" % (sans_taille, charges))

    modeles = []
    for paquet, nom, nature in candidats:
        m = eal.load_asset(paquet)
        if not isinstance(m, unreal.StaticMesh):
            continue
        b = m.get_bounding_box()
        x, y, z = b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z
        if hc.nature_modele(paquet + "/", nom, x, y, z) is None:
            continue
        pack = paquet.split("/")[2] if paquet.count("/") >= 2 else "?"
        modeles.append({
            "chemin": paquet, "nom": nom, "pack": pack, "nature": nature,
            "x": x, "y": y, "z": z, "lo": max(x, y), "la": min(x, y), "axe_long": "X" if x >= y else "Y",
            "centre": [(b.max.x + b.min.x) / 2, (b.max.y + b.min.y) / 2], "bas": b.min.z,
        })
    log("   %d modeles de maisons / immeubles retenus" % len(modeles))
    return modeles


# ------------------------------------------------------------------ 2. batiments simples de Rachid
def recalage_tuiles(acteurs):
    """Decalage entre le repere des donnees OSM et la carte, tuile par tuile."""
    man_path = os.path.join(OUT, "manifest.json")
    if not os.path.exists(man_path):
        log("   manifest.json introuvable : batiments OSM ignores")
        return {}, {}
    man = json.load(open(man_path, encoding="utf-8"))
    boites = {t["name"]: t["box_cm"] for t in man["tiles"] if t.get("buildings")}
    par_nom = {a.get_actor_label(): a for a in acteurs if a.get_actor_label() in boites}
    decal = {}
    for nom, bx in boites.items():
        a = par_nom.get(nom)
        if a is None:
            log("   tuile %s absente de la carte" % nom)
            continue
        o, e = a.get_actor_bounds(False)
        if max(abs(e.x - bx[3]), abs(e.y - bx[4]), abs(e.z - bx[5])) > 60:
            log("   tuile %s tournee ou redimensionnee : ignoree" % nom)
            continue
        decal[nom] = (o.x - bx[0], o.y + bx[1], o.z - bx[2])
    log("   %d tuiles de batiments recalees sur la carte" % len(decal))
    return decal, boites


def cibles_osm(acteurs):
    osm = os.path.join(OUT, "osm.json")
    if not os.path.exists(osm):
        log("   osm.json introuvable : batiments OSM ignores")
        return []
    decal, boites = recalage_tuiles(acteurs)
    if not decal:
        return []
    cibles = []
    for b in hc.batiments_osm(json.load(open(osm, encoding="utf-8"))):
        fx, fy = b["cx"] * 100, b["cy"] * 100
        if b["id"] in TRAVERSABLE_IDS:
            noms = ["Chelles_Tile_Traversable"]
        else:
            noms = [n for n, bx in boites.items() if n != "Chelles_Tile_Traversable"
                    and abs(fx - bx[0]) <= bx[3] + 1 and abs(fy - bx[1]) <= bx[4] + 1]
        noms = [n for n in noms if n in decal]
        if not noms:
            continue
        n = min(noms, key=lambda k: math.hypot(fx - boites[k][0], fy - boites[k][1]))
        dx, dy, dz = decal[n]
        cibles.append({
            "source": "osm", "id": b["id"], "tuile": n,
            "x": fx + dx, "y": -fy + dy, "sol": dz, "yaw": -math.degrees(b["angle"]),
            "longueur": b["longueur"] * 100, "largeur": b["largeur"] * 100, "hauteur": b["hauteur"] * 100,
            "remplissage": b["remplissage"], "infos": b["tags"],
        })
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
    """Boites 2D des grands objets deja poses (vrais batiments des packs, Blueprints...)."""
    res = []
    for a in acteurs:
        lbl = a.get_actor_label()
        if lbl.startswith(("Chelles_", "CH_")) or lbl in a_ignorer or a_tag(a, TAG_AJOUTE):
            continue
        if isinstance(a, (unreal.Pawn, unreal.Volume, unreal.Light, unreal.LandscapeProxy, unreal.Brush)):
            continue
        t = (a.get_class().get_name() + " " + lbl).lower()
        if any(k in t for k in ("sky", "cloud", "fog", "atmos", "snow", "neige", "post", "landscape", "navmesh", "water")):
            continue
        try:
            o, e = a.get_actor_bounds(False)
        except Exception:
            continue
        if e.x < 150 or e.y < 150 or e.z < 150 or e.x > 30000 or e.y > 30000:
            continue
        res.append((o.x - e.x, o.y - e.y, o.x + e.x, o.y + e.y, lbl))
    return res


def boite_cible(c):
    a = math.radians(c["yaw"])
    hx = abs(math.cos(a)) * c["longueur"] / 2 + abs(math.sin(a)) * c["largeur"] / 2
    hy = abs(math.sin(a)) * c["longueur"] / 2 + abs(math.cos(a)) * c["largeur"] / 2
    return c["x"] - hx, c["y"] - hy, c["x"] + hx, c["y"] + hy


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
    boites = vrais_batiments(acteurs, {c["id"] for c in cibles if c["source"] == "acteur"} | {"Floor"})
    rnd = random.Random(GRAINE)
    usages, poses = {}, []
    irreguliers, deja, sans = [], [], []
    for c in sorted(cibles, key=lambda c: -c["longueur"] * c["largeur"]):
        if c["remplissage"] < hc.REMPLISSAGE_MIN:
            irreguliers.append(c)
            continue
        d = deja_habille(c, boites)
        if d:
            c["deja"] = d
            deja.append(c)
            continue
        r = hc.meilleur_modele(c, modeles, usages)
        if r is None:
            sans.append(c)
            continue
        m, ecart, s = r
        usages[m["chemin"]] = usages.get(m["chemin"], 0) + 1
        loc, yaw, echelle = placement(c, m, s, rnd.random() < 0.5)
        poses.append({
            "source": c["source"], "id": c["id"], "modele": m["chemin"], "pack": m["pack"],
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
                    "" if m["nature"] == "batiment" else "(a verifier) ",
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
        w("   -> habilles par un modele de meme volume : %d\n" % len(poses))
        w("   -> aucun modele assez proche (ecart > %d %%) : %d\n" % (round(hc.TOLERANCE * 100), len(sans)))
        w("   -> forme trop irreguliere (en L, en U...)  : %d\n" % len(irreguliers))
        w("   -> deja un vrai batiment pose dessus        : %d\n" % len(deja))
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
        if deja:
            w("\nDEJA HABILLES (on n'y touche pas)\n")
            for c in deja:
                w("   %-14s sous %s\n" % (c["id"], c["deja"]))
        if sans:
            w("\nSANS MODELE ASSEZ PROCHE (les 40 plus grands)\n")
            for c in sans[:40]:
                w("   %-14s %5.1f x %4.1f x %4.1f m\n" % (c["id"], c["longueur"] / 100, c["largeur"] / 100, c["hauteur"] / 100))
    log("TERMINE : %d batiment(s) peuvent etre habilles sur %d. Rapport : Tools\\HabillageBatiments\\Rapport\\rapport_habillage.txt"
        % (len(poses), len(cibles)))


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
