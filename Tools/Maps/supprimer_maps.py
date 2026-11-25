# CHELLES 3D - Agent Terrain (25/11/2026)
# Supprime toutes les cartes (.umap) du projet SAUF Rachid et ce dont Rachid a besoin.
#
# Gardees :
#  - toute carte dont le nom commence par "Rachid" (Rachid, Rachid_Campagne, sauvegardes auto de Rachid...)
#  - toute carte dont Rachid depend, directement ou non (sous-niveaux, niveaux streames...) : registre d'assets
#  - toute carte citee dans Config\*.ini (GameDefaultMap, EditorStartupMap, ServerDefaultMap, TransitionMap...)
#  - toute carte citee dans les plugins (sources .cpp/.h, reglages .txt/.ini) ou dans le .uproject
#  - les cartes de \Engine (pas dans le projet) et tout ce qui est dans _Sauvegardes
# Supprimees : les autres .umap de Content, Plugins\*\Content, _Quarantaine et Saved (+ leur _BuiltData.uasset).
#
# 2 etapes (SUPPRIMER_LES_MAPS.bat) :
#  CHELLES_MAPS=LISTE     : dans Unreal (UE4Editor-Cmd), calcule la liste -> Tools\Maps\Rapport\maps_a_supprimer.txt/.json
#  CHELLES_MAPS=SUPPRIMER : Python seul (sans Unreal) : copie chaque fichier dans _Sauvegardes\avant_maps_<date>\
#                           (meme chemin) : il quitte le projet. Videz ce dossier plus tard pour gagner la place. Remettre : RESTAURER_MAPS.bat
# Lignes du journal : [Maps]
import os, re, sys, json, shutil, datetime

MODE = os.environ.get("CHELLES_MAPS", "LISTE").upper()
ICI = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else ""
PROJET = os.environ.get("CHELLES_PROJET") or os.path.abspath(os.path.join(ICI, "..", ".."))
RAPPORT = os.path.join(PROJET, "Tools", "Maps", "Rapport")
LISTE_JSON = os.path.join(RAPPORT, "maps_a_supprimer.json")
LISTE_TXT = os.path.join(RAPPORT, "maps_a_supprimer.txt")
CARTE = "/Game/Maps/Rachid"
DOSSIERS_IGNORES = {"_sauvegardes", "binaries", "intermediate", "deriveddatacache", ".git", "tools"}


def log(t):
    try:
        import unreal
        unreal.log("[Maps] " + t)
    except Exception:
        print("[Maps] " + t)
        sys.stdout.flush()


def gardee_par_nom(nom):
    return nom.lower().startswith("rachid")


def toutes_les_cartes():
    """Tous les .umap du projet : (chemin complet, chemin relatif au projet, nom de paquet Unreal ou None)."""
    cartes = []
    for racine, dossiers, fichiers in os.walk(PROJET):
        rel = os.path.relpath(racine, PROJET)
        haut = rel.split(os.sep)[0].lower()
        if rel == ".":
            dossiers[:] = [d for d in dossiers if d.lower() not in DOSSIERS_IGNORES and not d.lower().startswith("chelles 3d_backup")]
        dossiers[:] = [d for d in dossiers if d.lower() not in ("binaries", "intermediate")]
        for f in fichiers:
            if not f.lower().endswith(".umap"):
                continue
            chemin = os.path.join(racine, f)
            relf = os.path.relpath(chemin, PROJET)
            morceaux = relf.split(os.sep)
            paquet = None
            if morceaux[0].lower() == "content":
                paquet = "/Game/" + "/".join(morceaux[1:])[:-5]
            elif morceaux[0].lower() == "plugins":
                # Plugins\<...>\<Plugin>\Content\... -> /<Plugin>/...
                bas = [m.lower() for m in morceaux]
                if "content" in bas:
                    i = bas.index("content")
                    paquet = "/" + morceaux[i - 1] + "/" + "/".join(morceaux[i + 1:])[:-5]
            cartes.append((chemin, relf, paquet))
    return cartes


def textes_a_verifier():
    """Config/*.ini, .uproject, sources et reglages des plugins : une carte citee ici est gardee."""
    textes = []
    for f in os.listdir(PROJET):
        if f.lower().endswith(".uproject"):
            textes.append(os.path.join(PROJET, f))
    cfg = os.path.join(PROJET, "Config")
    if os.path.isdir(cfg):
        for f in os.listdir(cfg):
            if f.lower().endswith(".ini"):
                textes.append(os.path.join(cfg, f))
    plug = os.path.join(PROJET, "Plugins")
    for racine, dossiers, fichiers in os.walk(plug):
        dossiers[:] = [d for d in dossiers if d.lower() not in ("binaries", "intermediate", "content", "resources")]
        for f in fichiers:
            if f.lower().endswith((".cpp", ".h", ".txt", ".ini", ".cs", ".uplugin", ".json")):
                textes.append(os.path.join(racine, f))
    contenu = []
    for t in textes:
        try:
            with open(t, "r", encoding="utf-8", errors="ignore") as h:
                contenu.append((os.path.relpath(t, PROJET), h.read()))
        except Exception:
            pass
    return contenu


def cartes_dont_rachid_depend():
    """Fermeture des dependances (dures + souples) de Rachid et des cartes du Config, via le registre d'assets."""
    import unreal
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    log("Lecture du registre d'assets (quelques minutes)...")
    ar.search_all_assets(True)
    opts = unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=True, include_hard_package_references=True,
        include_searchable_names=False, include_soft_management_references=False,
        include_hard_management_references=False)
    vus = set()
    a_voir = [CARTE]
    while a_voir:
        p = a_voir.pop()
        if p in vus:
            continue
        vus.add(p)
        try:
            deps = ar.get_dependencies(p, opts) or []
        except Exception:
            deps = []
        for d in deps:
            d = str(d)
            if d.startswith("/Script/") or d.startswith("/Engine/") or d in vus:
                continue
            a_voir.append(d)
    log("Rachid utilise %d paquets (directement ou non)." % len(vus))
    return vus


def faire_liste():
    os.makedirs(RAPPORT, exist_ok=True)
    cartes = toutes_les_cartes()
    log("%d carte(s) .umap trouvee(s) dans le projet." % len(cartes))
    try:
        dependances = cartes_dont_rachid_depend()
    except Exception as e:
        log("ARRET : registre d'assets illisible (%s). Rien n'est supprime." % e)
        return 1
    textes = textes_a_verifier()
    gardees, supprimees = [], []
    for chemin, rel, paquet in cartes:
        nom = os.path.splitext(os.path.basename(chemin))[0]
        raison = None
        if gardee_par_nom(nom):
            raison = "nom Rachid"
        elif paquet and paquet in dependances:
            raison = "utilisee par Rachid"
        else:
            for fichier, texte in textes:
                if (paquet and paquet in texte) or ('"' + nom + '"' in texte) or ("/" + nom + "." in texte) \
                        or (fichier.lower().endswith(".ini") and re.search(r"Map\s*=\s*[^\r\n]*" + re.escape(nom), texte)):
                    raison = "citee dans " + fichier
                    break
        if raison:
            gardees.append({"fichier": rel, "raison": raison})
            continue
        fichiers = [rel]
        built = os.path.join(os.path.dirname(chemin), nom + "_BuiltData.uasset")
        if os.path.isfile(built):
            fichiers.append(os.path.relpath(built, PROJET))
        taille = sum(os.path.getsize(os.path.join(PROJET, f)) for f in fichiers)
        supprimees.append({"carte": rel, "paquet": paquet, "fichiers": fichiers, "octets": taille})
    total = sum(s["octets"] for s in supprimees)
    with open(LISTE_JSON, "w", encoding="utf-8") as h:
        json.dump({"date": datetime.datetime.now().isoformat(), "gardees": gardees, "a_supprimer": supprimees}, h, ensure_ascii=False, indent=1)
    with open(LISTE_TXT, "w", encoding="utf-8") as h:
        h.write("CARTES GARDEES (%d)\n" % len(gardees))
        for g in gardees:
            h.write("  %s   <- %s\n" % (g["fichier"], g["raison"]))
        h.write("\nCARTES A SUPPRIMER (%d, %.1f Mo)\n" % (len(supprimees), total / 1048576.0))
        for s in supprimees:
            h.write("  %s   (%.1f Mo)\n" % (s["carte"], s["octets"] / 1048576.0))
    for g in gardees:
        log("GARDEE  %s  (%s)" % (g["fichier"], g["raison"]))
    log("A SUPPRIMER : %d carte(s), %.1f Mo. Liste : Tools\\Maps\\Rapport\\maps_a_supprimer.txt" % (len(supprimees), total / 1048576.0))
    if not any(g["raison"] == "nom Rachid" and g["fichier"].lower().endswith(os.path.join("maps", "rachid.umap")) for g in gardees):
        log("ATTENTION : Content\\Maps\\Rachid.umap introuvable. Par securite, rien ne sera supprime.")
        os.remove(LISTE_JSON)
        return 1
    return 0


def supprimer():
    if not os.path.isfile(LISTE_JSON):
        log("Pas de liste : rien a faire.")
        return 1
    with open(LISTE_JSON, "r", encoding="utf-8") as h:
        donnees = json.load(h)
    date = datetime.datetime.now().strftime("%Y-%m-%d_%Hh%M")
    sauvegarde = os.path.join(PROJET, "_Sauvegardes", "avant_maps_" + date)
    os.makedirs(sauvegarde, exist_ok=True)
    faits, erreurs = 0, 0
    for s in donnees["a_supprimer"]:
        for rel in s["fichiers"]:
            src = os.path.join(PROJET, rel)
            if not os.path.isfile(src):
                continue
            if gardee_par_nom(os.path.splitext(os.path.basename(src))[0]):
                continue  # double securite
            dst = os.path.join(sauvegarde, rel)
            try:
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                taille = os.path.getsize(src)
                shutil.move(src, dst)  # meme disque : instantane, le fichier quitte le projet
                if not os.path.isfile(dst) or os.path.getsize(dst) != taille:
                    raise IOError("copie incomplete")
                faits += 1
            except Exception as e:
                erreurs += 1
                log("ERREUR sur %s : %s (fichier laisse en place)" % (rel, e))
    shutil.copy2(LISTE_JSON, os.path.join(sauvegarde, "maps_supprimees.json"))
    os.rename(LISTE_JSON, os.path.join(RAPPORT, "maps_supprimees_" + date + ".json"))
    log("%d fichier(s) supprime(s), %d erreur(s). Copie : _Sauvegardes\\avant_maps_%s" % (faits, erreurs, date))
    return 1 if erreurs else 0


if __name__ == "__main__" or True:
    code = supprimer() if MODE == "SUPPRIMER" else faire_liste()
    if MODE == "SUPPRIMER":
        sys.exit(code)
