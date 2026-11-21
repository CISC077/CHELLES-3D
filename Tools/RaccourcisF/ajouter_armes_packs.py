# -*- coding: utf-8 -*-
"""CHELLES 3D - met TOUTES les armes des packs dans la roue des armes TAB (2026-11-21)

A lancer avec AJOUTER_LES_ARMES_DES_PACKS.bat (Unreal FERME) ou dans l'editeur :
    File > Execute Python Script... > Tools/RaccourcisF/ajouter_armes_packs.py
Il faut que les plugins ChellesRaccourcis et ChellesArmes soient compiles.

Comment une arme de pack devient "compatible" avec le jeu :
  le soldat prend une arme de base du kit FPS_Game de la meme famille (pistolet, fusil, pompe, sniper,
  lourde, epee, energie) : son tir, ses munitions, ses animations. Le plugin ChellesArmes la DESSINE
  avec le modele 3D du pack (colonne Maillage). Rien n'est modifie dans les packs ni dans les Blueprints.

Ce que fait le script :
  1. cherche les modeles d'armes (SkeletalMesh / StaticMesh) dans les dossiers de packs d'armes
     (les chargeurs, douilles, lunettes, squelettes... sont ecartes) ;
  2. devine la famille de chaque arme d'apres son nom et son dossier ;
  3. ecrit Tools/RaccourcisF/Armes_Packs.csv (modifiable a la main : Nom, Categorie, ligne a retirer...) ;
  4. refait DT_Armes = Armes.csv (armes du jeu) + Armes_Packs.csv. Copie de l'ancien : DT_Armes_avant_packs.
Relancer le .bat apres avoir modifie Armes_Packs.csv : il est relu tel quel (CHELLES_RESCANNER=1 pour tout rechercher).
Journal : Tools/RaccourcisF/packs.log
"""
import csv
import os
import re
import traceback

import unreal

ICI = os.path.dirname(os.path.abspath(__file__))
JOURNAL = os.path.join(ICI, "packs.log")
CSV_JEU = os.path.join(ICI, "Armes.csv")
CSV_PACKS = os.path.join(ICI, "Armes_Packs.csv")
CSV_COMPLET = os.path.join(ICI, "Armes_complet.csv")
TABLE = "/Game/Chelles/Donnees/DT_Armes"
COPIE = "/Game/Chelles/Donnees/DT_Armes_avant_packs"
MAX_ARMES = 240  # 20 pages de 12 cases

# Packs d'armes connus du projet + tout dossier de /Game dont le nom fait penser a des armes
PACKS = ["AAAFantasticWeapons", "FPS_Assault_Pack", "FPS_Weapon_Bundle", "FPSAssets", "InfinityBladeWeapons",
         "MilitaryWeapDark", "MilitaryWeapSilver", "Weapon", "Weapon_Pack", "Weapons"]
MOTS_PACK = re.compile(r"weap|gun|arme|sword|blade|rifle|pistol|military", re.I)
PAS_UN_PACK = {"fps_game", "chelles", "tps_game", "_quarantaine"}

# Morceaux d'arme ou objets qui ne sont pas une arme entiere
ECARTES = {
    "mag", "mags", "magazine", "shell", "shells", "bullet", "bullets", "ammo", "scope", "sight", "sights", "reddot",
    "holo", "silencer", "suppressor", "grip", "foregrip", "flashlight", "rail", "rails", "stock", "trigger", "bolt",
    "slide", "barrel", "muzzle", "casing", "case", "clip", "holster", "projectile", "arms", "arm", "hand", "hands",
    "glove", "gloves", "attachment", "attachments", "attach", "lod", "proxy", "collision", "cube", "plane", "box",
    "sphere", "crate", "table", "rack", "decal", "fx", "smoke", "flash", "part", "parts", "piece", "cartridge",
    "grenade", "sheath", "scabbard", "target", "dummy", "floor", "wall", "skeleton", "physics", "phys", "pa",
    "handguard", "bipod", "laserpointer", "optic", "optics", "acog", "eotech", "rocketammo", "missile", "arrow",
    "quiver", "pickup", "socket", "dot", "lens", "cylinder", "hammerpart", "spring", "pin", "screw",
}
DOSSIERS_ECARTES = re.compile(r"/(animations?|anims?|materials?|textures?|fx|particles?|characters?|mannequin|"
                              r"hands?|arms|attachments?|ammo|maps?|levels?|blueprints?|sounds?|audio|ui)/", re.I)

FAMILLES = [
    ("Melee", {"sword", "blade", "katana", "axe", "knife", "dagger", "mace", "hammer", "club", "spear", "scythe",
               "machete", "saber", "sabre", "staff", "halberd", "bat", "claymore", "longsword", "greatsword", "cleaver"}),
    ("Pistolet", {"pistol", "handgun", "revolver", "glock", "deagle", "deserteagle", "colt", "1911", "m1911", "m9",
                  "beretta", "p226", "usp", "magnum", "luger", "makarov", "p250", "fiveseven"}),
    ("Pompe", {"shotgun", "pump", "spas", "spas12", "benelli", "m870", "870", "aa12", "saiga", "m1014", "doublebarrel"}),
    ("Precision", {"sniper", "awp", "barrett", "m82", "m24", "svd", "dragunov", "l96", "m40", "kar98", "kar98k",
                   "mosin", "intervention", "m107", "marksman", "dmr"}),
    ("Lourde", {"rpg", "rpg7", "launcher", "rocket", "bazooka", "minigun", "lmg", "m249", "pkm", "m60", "gatling",
                "mg42", "javelin", "stinger", "m32", "gl"}),
    ("Energie", {"laser", "plasma", "blaster", "ray", "raygun", "scifi", "phaser", "railgun", "energy"}),
]

LIGNES = []


def log(texte):
    LIGNES.append(texte)
    unreal.log("[ArmesPacks] " + texte)
    with open(JOURNAL, "w", encoding="utf-8") as f:
        f.write("\n".join(LIGNES) + "\n")


def mots(texte):
    """'SK_AK47_Dark' -> ['sk', 'ak47', 'dark'] ; 'WeaponPistolAuto' -> ['weapon', 'pistol', 'auto']"""
    texte = re.sub(r"([a-z])([A-Z])", r"\1 \2", texte)
    return [m.lower() for m in re.split(r"[^A-Za-z0-9]+", texte) if m]


def famille(nom, chemin):
    tous = set(mots(nom)) | set(mots(chemin))
    colle = nom.lower()
    for fam, cles in FAMILLES:
        if tous & cles or any(len(c) > 4 and c in colle for c in cles):
            return fam
    if "infinityblade" in chemin.lower().replace("_", ""):
        return "Melee"
    return "Fusil"


def nom_affiche(nom):
    n = re.sub(r"^(SKM|SK|SM|S|BP|Mesh)_", "", nom, flags=re.I)
    n = re.sub(r"^(Weap|Weapon)_?", "", n, flags=re.I)
    n = n.replace("_", " ").strip() or nom
    return n.upper()[:20]


def dossiers_de_packs(registre):
    trouves = []
    for d in registre.get_sub_paths("/Game", False):
        d = str(d)
        court = d.rsplit("/", 1)[-1]
        if court.lower() in PAS_UN_PACK:
            continue
        if court in PACKS or MOTS_PACK.search(court):
            trouves.append(d)
    return sorted(set(trouves))


def chercher():
    registre = unreal.AssetRegistryHelpers.get_asset_registry()
    registre.search_all_assets(True)
    armes = {}  # nom simplifie -> (priorite, ligne)
    for dossier in dossiers_de_packs(registre):
        n_avant = len(armes)
        for a in registre.get_assets_by_path(dossier, True):
            classe = str(a.asset_class)
            if classe not in ("SkeletalMesh", "StaticMesh"):
                continue
            nom = str(a.asset_name)
            chemin = str(a.object_path)
            paquet = str(a.package_path) + "/"
            if DOSSIERS_ECARTES.search(paquet):
                continue
            m = set(mots(nom))
            if m & ECARTES or nom.lower().startswith(("phys", "pa_")) or len(nom) < 3:
                continue
            simple = re.sub(r"^(skm|sk|sm|s)_", "", nom.lower())
            priorite = 0 if classe == "SkeletalMesh" else 1  # SkeletalMesh prefere (meme arme en double)
            cle = dossier + "|" + simple
            if cle in armes and armes[cle][0] <= priorite:
                continue
            fam = famille(nom, paquet)
            armes[cle] = (priorite, {
                "Nom": nom_affiche(nom), "Maillage": chemin, "Categorie": fam, "Pack": dossier.rsplit("/", 1)[-1]})
        log("%s : %d arme(s)." % (dossier, len(armes) - n_avant))
    liste = [v[1] for v in armes.values()]
    liste.sort(key=lambda l: (l["Categorie"], l["Pack"], l["Nom"]))
    if len(liste) > MAX_ARMES:
        log("%d armes trouvees : seules les %d premieres sont gardees (retirez des lignes dans Armes_Packs.csv)." % (len(liste), MAX_ARMES))
        liste = liste[:MAX_ARMES]
    return liste


def ecrire_csv_packs(liste):
    with open(CSV_PACKS, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Pack", "Nom", "Categorie", "Maillage"])
        for l in liste:
            w.writerow([l["Pack"], l["Nom"], l["Categorie"], l["Maillage"]])
    log("Armes_Packs.csv ecrit : %d armes." % len(liste))


def lire_csv_packs():
    with open(CSV_PACKS, encoding="utf-8-sig", newline="") as f:
        return [l for l in csv.DictReader(f) if (l.get("Maillage") or "").strip()]


def ecrire_complet(packs):
    with open(CSV_JEU, encoding="utf-8-sig", newline="") as f:
        lecteur = csv.reader(f)
        entete = next(lecteur)
        jeu = [r for r in lecteur if r]
    if "Maillage" not in entete:
        entete.append("Maillage")
        jeu = [r + [""] for r in jeu]
    col = {nom: i for i, nom in enumerate(entete)}
    noms = set(r[0] for r in jeu)
    lignes = list(jeu)
    for l in packs:
        base = "Pack_" + re.sub(r"[^A-Za-z0-9]", "", l["Pack"])[:12] + "_" + re.sub(r"[^A-Za-z0-9]", "", l["Nom"])[:20]
        nom_ligne, i = base, 2
        while nom_ligne in noms:
            nom_ligne, i = "%s_%d" % (base, i), i + 1
        noms.add(nom_ligne)
        r = [""] * len(entete)
        r[0] = nom_ligne
        r[col["Nom"]] = l["Nom"]
        r[col["ClasseArme"]] = ""  # vide = arme de base de la famille (choisie par la roue)
        r[col["Arsenal"]] = ""
        r[col["Icone"]] = ""
        r[col["Couleur"]] = "(R=0.000000,G=0.000000,B=0.000000,A=0.000000)"
        r[col["Categorie"]] = (l.get("Categorie") or "Fusil").strip()
        r[col["bDonner"]] = "True"
        r[col["Maillage"]] = l["Maillage"].strip()
        lignes.append(r)
    with open(CSV_COMPLET, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, quoting=csv.QUOTE_MINIMAL)
        w.writerow(entete)
        w.writerows(lignes)
    return len(jeu), len(packs)


def remplir_table():
    structure = unreal.load_object(None, "/Script/ChellesRaccourcis.ChellesArmeLigne")
    if structure is None:
        log("ERREUR : structure ChellesArmeLigne introuvable. Compilez d'abord Plugins/ChellesRaccourcis.")
        return
    lib = unreal.EditorAssetLibrary
    if lib.does_asset_exist(TABLE):
        if not lib.does_asset_exist(COPIE):
            lib.duplicate_asset(TABLE, COPIE)
            log("Copie de l'ancien tableau : " + COPIE)
        table = lib.load_asset(TABLE)
    else:
        usine = unreal.DataTableFactory()
        usine.set_editor_property("struct", structure)
        table = unreal.AssetToolsHelpers.get_asset_tools().create_asset("DT_Armes", "/Game/Chelles/Donnees", unreal.DataTable, usine)
    if table is None:
        log("ERREUR : DT_Armes impossible a ouvrir/creer.")
        return
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(table, CSV_COMPLET):
        log("ERREUR : lecture de Armes_complet.csv impossible.")
        return
    lib.save_loaded_asset(table)
    log("DT_Armes rempli : %d lignes." % len(unreal.DataTableFunctionLibrary.get_data_table_row_names(table)))


try:
    log("Debut.")
    if os.path.exists(CSV_PACKS) and os.environ.get("CHELLES_RESCANNER") != "1":
        packs = lire_csv_packs()
        log("Armes_Packs.csv relu (%d armes). Pour tout rechercher a nouveau : supprimez-le ou CHELLES_RESCANNER=1." % len(packs))
    else:
        packs = chercher()
        ecrire_csv_packs(packs)
    nb_jeu, nb_packs = ecrire_complet(packs)
    log("Roue : %d armes du jeu + %d armes des packs." % (nb_jeu, nb_packs))
    for fam in sorted(set(l["Categorie"] for l in packs)):
        log("  %s : %d" % (fam, sum(1 for l in packs if l["Categorie"] == fam)))
    remplir_table()
    log("Fin.")
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
