# -*- coding: utf-8 -*-
"""
CHELLES 3D - Remplacement du SUV par les voitures de POLICE (demande MERIDJA, 16/11/2026).

    python police_installer.py installer   (lance par INSTALLER_POLICE.bat)
    python police_installer.py remettre    (lance par REMETTRE_LE_SUV.bat)

installer :
  1. Sons : la circulation utilisait le moteur du pack SUV -> moteur du pack police (SonsVehicules.txt).
  2. Export du jeu : DefaultGame.ini cuisait le pack SUV -> cuit le pack police a la place.
  3. Verifie que plus rien ne se sert du pack SUV (carte Rachid, autres Blueprints, reglages).
  4. Range (ne supprime PAS) Content\\DriveableCar_basic et Plugins\\ChellesConduite dans _Quarantaine.
     Si quelque chose se sert encore du pack SUV, il n'est PAS deplace (voir rapport_suv.txt).
remettre : remet tout a sa place (le contraire de installer).
Journal : Tools\\Police\\quarantaine_suv.json (pour remettre) et rapport_suv.txt (a lire).
"""
import json
import os
import re
import shutil
import struct
import sys
import time

ICI = os.path.dirname(os.path.abspath(__file__))
PROJET = os.path.abspath(os.path.join(ICI, "..", ".."))
CONTENT = os.path.join(PROJET, "Content")
QUARANTAINE = os.path.join(PROJET, "_Quarantaine")
JOURNAL = os.path.join(ICI, "quarantaine_suv.json")
RAPPORT = os.path.join(ICI, "rapport_suv.txt")
SAUVEGARDE = os.path.join(ICI, "Sauvegarde_avant_police")

PACK_SUV = "DriveableCar_basic"
CHEMIN_SUV = "/Game/" + PACK_SUV
A_RANGER = [
    os.path.join("Content", PACK_SUV),
    os.path.join("Plugins", "ChellesConduite"),
]

# Remplacements de texte : (fichier, ancien, nouveau)
SONS = os.path.join(PROJET, "Plugins", "ChellesSonsVehicules", "SonsVehicules.txt")
INI_JEU = os.path.join(PROJET, "Config", "DefaultGame.ini")
REMPLACEMENTS = [
    (SONS, "Voiture.Moteur=/Game/DriveableCar_basic/Sound/cues/standard/standard_mix_Cue",
           "Voiture.Moteur=/Game/SportVehiclesPack/Sounds/Car/EngineCue_Car"),
    (SONS, "Voiture.Ralenti=150", "Voiture.Ralenti=800"),
    (SONS, "Voiture.RegimeMax=4500", "Voiture.RegimeMax=6000"),
    (INI_JEU, '+DirectoriesToAlwaysCook=(Path="/Game/DriveableCar_basic/Blueprints")',
              '+DirectoriesToAlwaysCook=(Path="/Game/SportVehiclesPack/Blueprints")'),
    (INI_JEU, '+DirectoriesToAlwaysCook=(Path="/Game/DriveableCar_basic/Sound")',
              '+DirectoriesToAlwaysCook=(Path="/Game/SportVehiclesPack/Sounds")'),
]

RE_ASCII = re.compile(rb"/Game/DriveableCar_basic[A-Za-z0-9_\-./]*", re.IGNORECASE)
RE_UTF16 = re.compile(rb"/\x00G\x00a\x00m\x00e\x00/\x00D\x00r\x00i\x00v\x00e\x00a\x00b\x00l\x00e\x00C\x00a\x00r\x00_\x00b\x00a\x00s\x00i\x00c\x00", re.IGNORECASE)
IGNORER_DOSSIERS = {"_quarantaine", "saved", "intermediate", "binaries", "deriveddatacache", ".git", ".vs"}


def dire(texte=""):
    print("  " + texte)


# ---------------------------------------------------------------- textes
def lire_texte(chemin):
    with open(chemin, "rb") as f:
        brut = f.read()
    bom = brut.startswith(b"\xef\xbb\xbf")
    texte = brut.decode("utf-8-sig", errors="replace")
    return texte, bom


def ecrire_texte(chemin, texte, bom):
    with open(chemin, "wb") as f:
        if bom:
            f.write(b"\xef\xbb\xbf")
        f.write(texte.encode("utf-8"))


def remplacer_textes(inverse=False):
    os.makedirs(SAUVEGARDE, exist_ok=True)
    for chemin in sorted({r[0] for r in REMPLACEMENTS}):
        if not os.path.isfile(chemin):
            dire("(absent, ignore) " + os.path.relpath(chemin, PROJET))
            continue
        texte, bom = lire_texte(chemin)
        avant = texte
        for fichier, ancien, nouveau in REMPLACEMENTS:
            if fichier != chemin:
                continue
            a, n = (nouveau, ancien) if inverse else (ancien, nouveau)
            texte = texte.replace(a, n)
        if texte != avant:
            copie = os.path.join(SAUVEGARDE, os.path.basename(chemin) + time.strftime(".%Y%m%d_%H%M%S"))
            shutil.copy2(chemin, copie)
            ecrire_texte(chemin, texte, bom)
            dire("Modifie : " + os.path.relpath(chemin, PROJET) + "  (copie : " + os.path.relpath(copie, PROJET) + ")")
        else:
            dire("Deja a jour : " + os.path.relpath(chemin, PROJET))


# ---------------------------------------------------------------- references
def taille_entete(f, taille_fichier):
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


def chercher_references():
    """Renvoie (assets qui citent le pack SUV, reglages qui le citent, code qui le cite)."""
    assets, reglages, code = [], [], []
    pack = os.path.normcase(os.path.join(CONTENT, PACK_SUV))
    conduite = os.path.normcase(os.path.join(PROJET, "Plugins", "ChellesConduite"))
    outils = os.path.normcase(ICI)
    n = 0
    for racine, dossiers, fichiers in os.walk(PROJET):
        dossiers[:] = [d for d in dossiers if d.lower() not in IGNORER_DOSSIERS]
        nr = os.path.normcase(racine)
        if nr.startswith(pack) or nr.startswith(conduite) or nr.startswith(outils):
            continue
        for nom in fichiers:
            chemin = os.path.join(racine, nom)
            ext = os.path.splitext(nom)[1].lower()
            rel = os.path.relpath(chemin, PROJET)
            try:
                if ext in (".uasset", ".umap"):
                    n += 1
                    if n % 5000 == 0:
                        dire("... %d fichiers Unreal lus" % n)
                    taille = os.path.getsize(chemin)
                    with open(chemin, "rb") as f:
                        h = taille_entete(f, taille)
                        f.seek(0)
                        data = f.read(h if h else min(taille, 16 * 1024 * 1024))
                    if RE_ASCII.search(data) or RE_UTF16.search(data):
                        assets.append(rel)
                elif ext in (".ini", ".txt", ".json"):
                    with open(chemin, "r", encoding="utf-8", errors="ignore") as f:
                        if CHEMIN_SUV.lower() in f.read().lower():
                            reglages.append(rel)
                elif ext in (".cpp", ".h", ".py", ".cs"):
                    with open(chemin, "r", encoding="utf-8", errors="ignore") as f:
                        if CHEMIN_SUV.lower() in f.read().lower():
                            code.append(rel)
            except OSError:
                pass
    # Redirecteurs du pack SUV vers lui-meme : ignores. Les fichiers de journal ne comptent pas.
    reglages = [r for r in reglages if not r.lower().startswith(("tools" + os.sep))]
    return assets, reglages, code


# ---------------------------------------------------------------- deplacements
def deplacer(src, dst):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.move(src, dst)


def installer():
    print()
    dire("=== 1. Sons et export du jeu : pack SUV -> pack police ===")
    remplacer_textes()

    print()
    dire("=== 2. Qui se sert encore du pack SUV ? (quelques minutes) ===")
    assets, reglages, code = chercher_references()
    lignes = ["CHELLES 3D - verification avant de ranger le SUV (" + time.strftime("%d/%m/%Y %H:%M") + ")", ""]
    lignes.append("Fichiers Unreal (cartes, Blueprints) qui citent " + CHEMIN_SUV + " : %d" % len(assets))
    lignes += ["   " + a for a in assets]
    lignes.append("")
    lignes.append("Reglages (.ini / .txt) qui le citent : %d" % len(reglages))
    lignes += ["   " + r for r in reglages]
    lignes.append("")
    lignes.append("Code C++ / scripts qui le citent (sans danger si le reglage .txt le remplace) : %d" % len(code))
    lignes += ["   " + c for c in code]
    with open(RAPPORT, "w", encoding="utf-8") as f:
        f.write("\n".join(lignes) + "\n")
    for l in lignes[2:]:
        dire(l)

    bloque_content = bool(assets or reglages)
    if bloque_content:
        print()
        dire("ATTENTION : le pack SUV sert encore (liste ci-dessus, aussi dans Tools\\Police\\rapport_suv.txt).")
        dire("Le dossier Content\\DriveableCar_basic n'est PAS deplace. Envoie rapport_suv.txt a Claude.")

    a_faire = []
    for rel in A_RANGER:
        if bloque_content and rel.startswith("Content"):
            continue
        src = os.path.join(PROJET, rel)
        if os.path.isdir(src):
            a_faire.append(rel)
    if not a_faire:
        print()
        dire("Rien a ranger (deja fait ?).")
        return 0

    print()
    dire("=== 3. Ranger dans _Quarantaine (rien n'est supprime, REMETTRE_LE_SUV.bat remet tout) ===")
    for rel in a_faire:
        dire("   " + rel)
    try:
        rep = input("\n  Tape OUI puis Entree pour ranger ces dossiers (autre chose = on garde) : ").strip().upper()
    except EOFError:
        rep = ""
    if rep != "OUI":
        dire("D'accord, rien n'est range. Le jeu marchera quand meme avec la POLICE.")
        return 0

    journal = []
    if os.path.isfile(JOURNAL):
        with open(JOURNAL, "r", encoding="utf-8") as f:
            journal = json.load(f)
    for rel in a_faire:
        src = os.path.join(PROJET, rel)
        dst = os.path.join(QUARANTAINE, rel)
        if os.path.exists(dst):
            dst = dst + time.strftime("_%Y%m%d_%H%M%S")
        try:
            deplacer(src, dst)
            journal.append({"depuis": rel, "vers": os.path.relpath(dst, PROJET), "date": time.strftime("%Y-%m-%d %H:%M:%S")})
            dire("Range : " + rel + "  ->  " + os.path.relpath(dst, PROJET))
        except Exception as e:
            dire("ECHEC pour " + rel + " : " + str(e) + " (un fichier est peut-etre ouvert)")
    with open(JOURNAL, "w", encoding="utf-8") as f:
        json.dump(journal, f, indent=2, ensure_ascii=False)
    return 0


def remettre():
    print()
    if not os.path.isfile(JOURNAL):
        dire("Aucun journal : rien n'a ete range.")
    else:
        with open(JOURNAL, "r", encoding="utf-8") as f:
            journal = json.load(f)
        reste = []
        for e in reversed(journal):
            src = os.path.join(PROJET, e["vers"])
            dst = os.path.join(PROJET, e["depuis"])
            if not os.path.isdir(src):
                dire("Introuvable : " + e["vers"])
                continue
            if os.path.exists(dst):
                dire("Deja present, laisse en quarantaine : " + e["depuis"])
                reste.append(e)
                continue
            deplacer(src, dst)
            dire("Remis : " + e["depuis"])
        with open(JOURNAL, "w", encoding="utf-8") as f:
            json.dump(list(reversed(reste)), f, indent=2, ensure_ascii=False)
    print()
    dire("Sons et export du jeu remis comme avant :")
    remplacer_textes(inverse=True)
    print()
    dire("Le pack SUV et le plugin ChellesConduite sont revenus.")
    dire("La roue TAB affiche toujours POLICE : demande a Claude pour remettre la case SUV.")
    return 0


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "installer"
    sys.exit(remettre() if mode == "remettre" else installer())
