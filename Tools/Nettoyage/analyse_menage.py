# -*- coding: utf-8 -*-
"""
Menage du projet CHELLES 3D (Unreal 4.27) : quels packs de Content la carte Rachid n'utilise pas ?

LECTURE SEULE : ce script ne supprime, ne deplace et ne modifie AUCUN fichier du projet.
Il ecrit seulement :
  - Tools\\Nettoyage\\Rapport\\menage.txt          (le resume a lire)
  - Tools\\Nettoyage\\Rapport\\menage_propose.json (la liste proposee pour la quarantaine)

Ce qui est GARDE d'office :
  - tout ce que la carte Rachid utilise, directement ou indirectement ;
  - tout ce que citent la config (Config\\*.ini) et le code des plugins/Source
    (motoneige, cheval, pouvoirs, zombies, menu, musiques...) ;
  - la liste GARDER ci-dessous (personnages, voitures, armes, sons...) ;
  - les packs qui contiennent des personnages ou des vehicules (SkeletalMesh) : les passants, la
    circulation et les pouvoirs les cherchent dans tout le projet pendant le jeu. Ils sont listes a part.
"""
import json
import os
import re
import struct
import sys
import time
from collections import defaultdict, deque

ICI = os.path.dirname(os.path.abspath(__file__))
PROJET = os.path.abspath(os.path.join(ICI, "..", ".."))
CONTENT = os.path.join(PROJET, "Content")
SORTIE = os.path.join(ICI, "Rapport")

CARTE = "/Game/Maps/Rachid"

# Dossiers de Content gardes quoi qu'il arrive (premier niveau).
GARDER = {
    "Maps", "Chelles", "ChellesMenu", "FPS_Game", "UI", "Audio", "Sounds", "HumanVocalizations",
    "FPS_Assault_Pack", "FPS_Weapon_Bundle", "FPSAssets", "Weapon", "Personnages", "Voitures",
    "Miami", "Miami_beach_modular", "DV_Car_Horo", "DV_Snowmobile", "DriveableCar_basic", "FuturisticGirl",
    "Hijacker", "DecayedZombieB", "Bus_sity", "Animalia", "ActorCore_Sample_Motions", "AnimStarterPack",
    "Animations", "Building_Props", "PN_Fern", "Vehicles", "VehicleAdv", "VehicleVarietyPack",
    "VehicleVarietyVol2", "Bandit_Sport", "Female_Mannequin", "Action_Male_and_Female", "Sport_Female",
    "Christmas_Girl", "Christmas_pack", "Scanned3DPeoplePack", "HorseStarterKit", "SuperPowers",
    "SuperheroFlight", "DynamicCombatSystem", "GoodSky", "Clouds_and_Skies", "GrassLandscape", "P_Snow",
    "StarterContent", "IMAGES", "Developers", "Collections",
}

UNREAL_EXT = {".uasset", ".umap"}
RE_ASCII = re.compile(rb"/Game/[A-Za-z0-9_\-./ ()]+")
RE_UTF16 = re.compile(rb"/\x00G\x00a\x00m\x00e\x00/\x00(?:[A-Za-z0-9_\-./ ()]\x00)+")
RE_TEXTE = re.compile(r"/Game/[A-Za-z0-9_\-./ ()%]*")
MARQUEURS_PERSO = (b"SkeletalMesh\x00", b"WheeledVehicle")


def taille_entete(f, taille_fichier):
    """Lit TotalHeaderSize dans l'entete du package UE4."""
    try:
        f.seek(0)
        b = f.read(4096)
        tag, leg = struct.unpack_from("<Ii", b, 0)
        if tag != 0x9E2A83C1:
            return None
        o = 8
        if leg != -4:
            o += 4
        o += 8
        n = struct.unpack_from("<i", b, o)[0]
        o += 4 + n * 20
        h = struct.unpack_from("<i", b, o)[0]
        if 0 < h <= taille_fichier:
            return h
    except Exception:
        pass
    return None


def lire_paquet(chemin, taille):
    """Renvoie (references /Game, contient un personnage ou vehicule ?)."""
    with open(chemin, "rb") as f:
        h = taille_entete(f, taille)
        f.seek(0)
        data = f.read(h if h else min(taille, 16 * 1024 * 1024))
    refs = set()
    for m in RE_ASCII.finditer(data):
        refs.add(m.group().decode("ascii", "ignore"))
    for m in RE_UTF16.finditer(data):
        refs.add(m.group().decode("utf-16-le", "ignore"))
    propres = set()
    for r in refs:
        r = r.strip().rstrip("/").split(".")[0]
        if r.startswith("/Game/") and len(r) > 6:
            propres.add(r)
    perso = any(m in data for m in MARQUEURS_PERSO)
    return propres, perso


def lire_redirections():
    red = []
    try:
        txt = open(os.path.join(PROJET, "Config", "DefaultEngine.ini"), encoding="utf-8", errors="ignore").read()
    except Exception:
        return red
    for m in re.finditer(r'PackageRedirects=\(OldName="([^"]+)",NewName="([^"]+)"([^)]*)\)', txt):
        red.append((m.group(1), m.group(2), "MatchSubstring=true" in m.group(3)))
    return red


def chemins_cites_par_le_code():
    """Chemins /Game ecrits dans Config, Plugins et Source (code C++, .txt, .ini, .py)."""
    trouves = set()
    for base in ("Config", "Plugins", "Source"):
        racine = os.path.join(PROJET, base)
        for r, dirs, fs in os.walk(racine):
            dirs[:] = [d for d in dirs if d not in ("Binaries", "Intermediate", "Content")]
            for nom in fs:
                if os.path.splitext(nom)[1].lower() not in (".cpp", ".h", ".ini", ".txt", ".py", ".cs"):
                    continue
                try:
                    txt = open(os.path.join(r, nom), encoding="utf-8", errors="ignore").read()
                except Exception:
                    continue
                for m in RE_TEXTE.finditer(txt):
                    c = m.group().split("%")[0].split(".")[0].rstrip("/")
                    if c.count("/") >= 2:      # ignore "/Game" tout seul
                        trouves.add(c)
    return trouves


def mo(octets):
    return round(octets / 1024.0 / 1024.0, 1)


def main():
    debut = time.time()
    if not os.path.isdir(CONTENT):
        print("Dossier Content introuvable :", CONTENT)
        return 1
    redirections = lire_redirections()

    print("1/4 Inventaire du dossier Content...")
    fichiers = []
    for racine, dirs, files in os.walk(CONTENT):
        for nom in files:
            p = os.path.join(racine, nom)
            try:
                t = os.path.getsize(p)
            except OSError:
                continue
            rel = os.path.relpath(p, CONTENT).replace("\\", "/")
            fichiers.append((rel, t, os.path.splitext(nom)[1].lower()))
    print("   %d fichiers" % len(fichiers))
    paquets = {}
    for rel, t, ext in fichiers:
        if ext in UNREAL_EXT:
            paquets["/Game/" + rel[: -len(ext)]] = (rel, t, ext)
    paquets_min = {k.lower(): k for k in paquets}

    def resoudre(ref):
        for old, new, sub in redirections:
            if sub and old.rstrip("/") in ref:
                ref = ref.replace(old.rstrip("/"), new.rstrip("/"), 1)
            elif ref == old:
                ref = new
        if ref in paquets:
            return ref
        return paquets_min.get(ref.lower())

    print("2/4 Lecture des references (%d assets)..." % len(paquets))
    deps, perso = {}, set()
    for i, (pk, (rel, t, ext)) in enumerate(paquets.items()):
        if i % 2000 == 0:
            print("   %d / %d" % (i, len(paquets)))
        try:
            refs, est_perso = lire_paquet(os.path.join(CONTENT, rel), t)
        except Exception:
            refs, est_perso = set(), False
        if est_perso:
            perso.add(pk)
        deps[pk] = {c for c in (resoudre(r) for r in refs) if c and c != pk}

    print("3/4 Recherche de ce que le jeu utilise...")
    cites = chemins_cites_par_le_code()
    racines = set()
    for c in [CARTE] + sorted(cites):
        cible = resoudre(c)
        if cible:
            racines.add(cible)
        else:   # un dossier : on garde tout ce qu'il contient
            pref = c.lower().rstrip("/") + "/"
            racines.update(k for k in paquets if k.lower().startswith(pref))
    for pk, (rel, t, ext) in paquets.items():
        if rel.split("/")[0] in GARDER:
            racines.add(pk)
    utilises = set()
    file_ = deque(racines)
    while file_:
        p = file_.popleft()
        if p in utilises:
            continue
        utilises.add(p)
        if p + "_BuiltData" in paquets:
            file_.append(p + "_BuiltData")
        file_.extend(deps.get(p, ()))

    print("4/4 Ecriture du rapport...")
    par_dossier = defaultdict(lambda: {"octets": 0, "fichiers": 0, "assets": 0, "utilises": 0, "perso": 0})
    for rel, t, ext in fichiers:
        top = rel.split("/")[0] if "/" in rel else None
        if top is None:
            continue
        d = par_dossier[top]
        d["octets"] += t
        d["fichiers"] += 1
        if ext in UNREAL_EXT:
            pk = "/Game/" + rel[: -len(ext)]
            d["assets"] += 1
            d["utilises"] += pk in utilises
            d["perso"] += pk in perso
    proposes, avec_perso, partiels = [], [], []
    for top, d in par_dossier.items():
        if top in GARDER or d["assets"] == 0:
            continue
        ligne = {"dossier": top, "taille_mo": mo(d["octets"]), "assets": d["assets"],
                 "utilises": d["utilises"], "personnages_ou_vehicules": d["perso"]}
        if d["utilises"] == 0 and d["perso"] == 0:
            proposes.append(ligne)
        elif d["utilises"] == 0:
            avec_perso.append(ligne)
        else:
            partiels.append(ligne)
    for l in (proposes, avec_perso, partiels):
        l.sort(key=lambda x: -x["taille_mo"])

    # fichiers a la racine de Content inutilises
    racine_inutile = [(rel, t) for pk, (rel, t, ext) in paquets.items()
                      if "/" not in rel and pk not in utilises and pk not in perso]
    total_content = sum(t for _, t, _ in fichiers)
    gain = sum(x["taille_mo"] for x in proposes) + mo(sum(t for _, t in racine_inutile))

    os.makedirs(SORTIE, exist_ok=True)
    with open(os.path.join(SORTIE, "menage_propose.json"), "w", encoding="utf-8") as f:
        json.dump({"dossiers": [x["dossier"] for x in proposes],
                   "fichiers": sorted(rel for rel, _ in racine_inutile)}, f, ensure_ascii=False, indent=1)
    with open(os.path.join(SORTIE, "menage_detail.json"), "w", encoding="utf-8") as f:
        json.dump({"date": time.strftime("%Y-%m-%d %H:%M:%S"), "chemins_cites_par_le_code": sorted(cites),
                   "proposes": proposes, "inutilises_avec_personnages": avec_perso,
                   "en_partie_utilises": partiels,
                   "fichiers_racine": [{"fichier": r, "taille_mo": mo(t)} for r, t in sorted(racine_inutile)]},
                  f, ensure_ascii=False, indent=1)

    L = ["MENAGE CHELLES 3D - %s  (rien n'a ete modifie)" % time.strftime("%Y-%m-%d %H:%M"),
         "Content : %d fichiers, %.1f Go" % (len(fichiers), total_content / 1024.0 ** 3),
         "Assets utilises par le jeu : %d sur %d" % (len(utilises), len(paquets)),
         "Gain possible avec la liste A : %.1f Go" % (gain / 1024.0),
         "",
         "A) PACKS QUE LE JEU N'UTILISE PAS (proposes pour la quarantaine) : %d" % len(proposes)]
    L += ["   %8.0f Mo  %s" % (x["taille_mo"], x["dossier"]) for x in proposes]
    L += ["   %8.0f Mo  %d fichier(s) a la racine de Content" % (mo(sum(t for _, t in racine_inutile)),
                                                             len(racine_inutile)), "",
          "B) PACKS NON UTILISES PAR LA CARTE MAIS AVEC PERSONNAGES/VEHICULES (a decider ensemble) : %d" % len(avec_perso)]
    L += ["   %8.0f Mo  %s  (%d perso/vehicule)" % (x["taille_mo"], x["dossier"], x["personnages_ou_vehicules"])
          for x in avec_perso]
    L += ["", "C) PACKS UTILISES PAR LE JEU (gardes) : %d" % len(partiels)]
    L += ["   %8.0f Mo  %s  (%d/%d assets utilises)" % (x["taille_mo"], x["dossier"], x["utilises"], x["assets"])
          for x in partiels[:30]]
    L += ["", "Duree : %d s" % (time.time() - debut)]
    with open(os.path.join(SORTIE, "menage.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")
    print("\n".join(L))
    print("\nRapport : Tools\\Nettoyage\\Rapport\\menage.txt")
    return 0


if __name__ == "__main__":
    sys.exit(main())
