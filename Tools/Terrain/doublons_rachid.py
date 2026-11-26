# CHELLES 3D - Agent Terrain (25/11/2026)
# Repere et retire les ACTEURS EN DOUBLE de la carte Rachid : meme objet, au meme endroit,
# meme rotation, meme taille (copies collees deux fois, imports en double...). Ils coutent
# du temps d'affichage pour rien et provoquent des scintillements (z-fighting).
# - une copie de Rachid.umap (+ _BuiltData) est faite AVANT dans _Sauvegardes\avant_doublons_<date>
# - liste des acteurs retires : Tools\Terrain\Rapport\doublons_retires_<date>.json
# - rapport des fichiers en double dans Content (meme nom + meme taille) : Tools\Terrain\Rapport\doublons_fichiers.txt
#   (rapport seulement : aucun fichier n'est supprime)
# Variable CHELLES_DOUBLONS : SIMULATION (rien n'est retire) ou SUPPRIMER (par defaut SIMULATION).
# Lance par OPTIMISER_TERRAIN.bat (Unreal ferme), ou dans l'editeur : Fichier > Executer un script Python.
# Lignes du journal : [Terrain]
import os, json, shutil, datetime
import unreal

CARTE = "/Game/Maps/Rachid"
MODE = os.environ.get("CHELLES_DOUBLONS", "SIMULATION").upper()
PROJET = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
CONTENT = os.path.join(PROJET, "Content")
RAPPORT = os.path.join(PROJET, "Tools", "Terrain", "Rapport")
DATE = datetime.datetime.now().strftime("%Y-%m-%d_%Hh%M")

# Rien de ce que posent les autres outils et qui DOIT rester superpose (fil vegetation / riviere / lac)
TAGS_PROTEGES = {"ChellesVegEau", "ChellesVegEauCache", "ChellesZombie", "ChellesDepart", "ChellesGarder"}
PREFIXES_PROTEGES = ("ChellesVeg_", "Chelles_Riviere", "Chelles_Lac", "CH_tree_", "CH_bush_", "CH_hedge_",
                     "Stylized_Lac", "Stylized_PierreLac")
MOTS_CLASSES_IGNOREES = ("PlayerStart", "Volume", "Navigation", "NavMesh", "WorldSettings", "Brush", "Landscape",
                         "Foliage", "GameMode", "Info", "Sky", "Fog", "PostProcess", "Camera", "Spawner", "Manager",
                         "Note", "Reflection", "Atmosphere", "SphereReflection", "BoxReflection", "LevelSequence")


def log(t):
    unreal.log("[Terrain] " + t)


def charger_monde():
    try:
        monde = unreal.EditorLevelLibrary.get_editor_world()
    except Exception:
        monde = None
    if monde is not None and monde.get_path_name().split(".")[0] == CARTE:
        return monde
    log("ouverture de %s (1 a 3 minutes)..." % CARTE)
    return unreal.EditorLoadingAndSavingUtils.load_map(CARTE)


def acteurs_du_monde(monde):
    try:
        liste = unreal.EditorLevelLibrary.get_all_level_actors()
        if liste:
            return list(liste)
    except Exception:
        pass
    return list(unreal.GameplayStatics.get_all_actors_of_class(monde, unreal.Actor))


def protege(a):
    try:
        tags = set(str(t) for t in a.tags)
    except Exception:
        tags = set()
    if tags & TAGS_PROTEGES:
        return True
    nom = a.get_actor_label() if hasattr(a, "get_actor_label") else a.get_name()
    if nom.startswith(PREFIXES_PROTEGES):
        return True
    try:
        if a.get_editor_property("hidden"):
            return True  # cache en jeu par un autre outil : on n'y touche pas
    except Exception:
        pass
    classe = a.get_class().get_name()
    return any(m in classe for m in MOTS_CLASSES_IGNOREES)


def base_label(a):
    nom = a.get_actor_label()
    return nom.rstrip("0123456789_ ")


def cle(a):
    t = a.get_actor_transform()
    p, r, s = t.translation, t.rotation.rotator(), t.scale3d
    pos = (round(p.x), round(p.y), round(p.z))           # 1 cm pres
    rot = (round(r.roll * 2) % 720, round(r.pitch * 2) % 720, round(r.yaw * 2) % 720)  # 0,5 degre pres
    ech = (round(s.x, 2), round(s.y, 2), round(s.z, 2))
    classe = a.get_class().get_path_name()
    if isinstance(a, unreal.StaticMeshActor):
        comp = a.static_mesh_component
        mesh = comp.static_mesh if comp else None
        if mesh is None:
            return None
        try:
            mats = tuple(m.get_path_name() if m else "" for m in comp.get_materials())
        except Exception:
            mats = ()
        return ("SM", mesh.get_path_name(), mats, pos, rot, ech)
    if isinstance(a, (unreal.PointLight, unreal.SpotLight, unreal.RectLight)):
        return ("LUMIERE", classe, pos, rot)
    if classe.startswith("/Game/"):
        # Blueprint du projet : meme classe, meme nom de base, meme endroit
        return ("BP", classe, base_label(a), pos, rot, ech)
    return None


def rapport_fichiers():
    """Fichiers .uasset de meme nom ET meme taille dans des dossiers differents (copies via l'Explorateur...)."""
    groupes = {}
    for racine, dossiers, fichiers in os.walk(CONTENT):
        dossiers[:] = [d for d in dossiers if d not in ("Collections", "Developers")]
        for f in fichiers:
            if not f.endswith((".uasset", ".umap")):
                continue
            chemin = os.path.join(racine, f)
            try:
                taille = os.path.getsize(chemin)
            except OSError:
                continue
            if taille < 200 * 1024:
                continue  # petits fichiers : sans interet
            groupes.setdefault((f.lower(), taille), []).append(os.path.relpath(chemin, CONTENT))
    doubles = [(k, v) for k, v in groupes.items() if len(v) > 1]
    doubles.sort(key=lambda kv: kv[0][1] * (len(kv[1]) - 1), reverse=True)
    perdu = sum(k[1] * (len(v) - 1) for k, v in doubles)
    os.makedirs(RAPPORT, exist_ok=True)
    with open(os.path.join(RAPPORT, "doublons_fichiers.txt"), "w", encoding="utf-8") as sortie:
        sortie.write("CHELLES 3D - fichiers en double dans Content (meme nom + meme taille), %s\n" % DATE)
        sortie.write("RAPPORT SEULEMENT : rien n'a ete supprime. Place perdue estimee : %.1f Go\n\n" % (perdu / 1e9))
        for (nom, taille), chemins in doubles[:400]:
            sortie.write("%s  (%.1f Mo x %d)\n" % (nom, taille / 1e6, len(chemins)))
            for c in chemins:
                sortie.write("    %s\n" % c)
    log("fichiers en double dans Content : %d groupes, %.1f Go perdus (rapport doublons_fichiers.txt)" % (len(doubles), perdu / 1e9))


def sauvegarder_carte():
    dest = os.path.join(PROJET, "_Sauvegardes", "avant_doublons_" + DATE)
    os.makedirs(dest, exist_ok=True)
    for nom in ("Rachid.umap", "Rachid_BuiltData.uasset"):
        src = os.path.join(CONTENT, "Maps", nom)
        if os.path.isfile(src):
            shutil.copy2(src, os.path.join(dest, nom))
    log("copie de securite : " + dest)
    return dest


def principal():
    log("mode : " + MODE)
    try:
        rapport_fichiers()
    except Exception as e:
        log("rapport des fichiers impossible : %s" % e)

    monde = charger_monde()
    if monde is None:
        log("ERREUR : impossible d'ouvrir Rachid. Ouvre Rachid dans Unreal puis Fichier > Executer un script Python > ce fichier.")
        return
    acteurs = acteurs_du_monde(monde)
    log("%d acteurs dans Rachid" % len(acteurs))

    vus = {}
    doublons = []
    for a in sorted(acteurs, key=lambda x: x.get_name()):
        try:
            if protege(a):
                continue
            k = cle(a)
        except Exception:
            continue
        if k is None:
            continue
        if k in vus:
            doublons.append((a, vus[k]))
        else:
            vus[k] = a

    par_type = {}
    journal = []
    for a, garde in doublons:
        t = a.get_actor_transform().translation
        type_ = cle(a)[0]
        par_type[type_] = par_type.get(type_, 0) + 1
        journal.append({"retire": a.get_actor_label(), "garde": garde.get_actor_label(),
                        "classe": a.get_class().get_path_name(), "position": [t.x, t.y, t.z]})
    log("%d doublon(s) trouve(s) : %s" % (len(doublons), ", ".join("%s %d" % kv for kv in par_type.items()) or "aucun"))
    os.makedirs(RAPPORT, exist_ok=True)
    with open(os.path.join(RAPPORT, "doublons_%s_%s.json" % ("retires" if MODE == "SUPPRIMER" else "simulation", DATE)), "w", encoding="utf-8") as f:
        json.dump(journal, f, ensure_ascii=False, indent=1)

    if MODE != "SUPPRIMER" or not doublons:
        log("TERMINE (rien n'a ete retire).")
        return

    sauvegarder_carte()
    retires = 0
    for a, _ in doublons:
        try:
            if unreal.EditorLevelLibrary.destroy_actor(a):
                retires += 1
                continue
        except Exception:
            pass
        try:
            a.destroy_actor()
            retires += 1
        except Exception as e:
            log("pas retire %s : %s" % (a.get_name(), e))
    ok = unreal.EditorLoadingAndSavingUtils.save_map(monde, CARTE)
    log("TERMINE : %d doublon(s) retire(s), Rachid %s." % (retires, "enregistree" if ok else "PAS enregistree (erreur)"))


principal()
