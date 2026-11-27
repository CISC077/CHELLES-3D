"""CHELLES 3D - LOD AUTOMATIQUES, version sans question (27/11/2026).

Lance par TOUT_INSTALLER_27-11.bat (Unreal ferme, mode commandlet). Fait comme ajouter_lod.py
(3 LOD : 50 %, 25 %, 10 % des triangles ; de pres rien ne change), mais cherche AUSSI les objets
que les plugins posent pendant la partie et que la carte ne reference pas : les 30 Bugatti garees
(carrosserie ~15 Mo chacune, sans LOD), les voitures Horo, les commerces, l'appartement, les props...

Pourquoi : test du 26/11, Zone 1 sud-ouest 13 img/s et Zone 2 sud 19 img/s, la carte graphique
limite (76 a 90 ms par image) pendant que les Bugatti garees apparaissent autour du joueur.

Retour en arriere : Tools/LOD/retirer_lod.py (meme journal Tools/LOD/journal_lod.json).
Les objets qui ont deja des LOD ne sont pas touches. Journal : Tools/out/lod.log
"""
import datetime, json, os, traceback
import unreal

CARTE = "/Game/Maps/Rachid"
DOSSIERS = ["/Game/Chelles", "/Game/DV_Car_Horo", "/Game/Props"]  # Personnages : les statues sont deja dans la carte
SEUIL_SOMMETS = 3000
NIVEAUX = [(1.00, 1.00), (0.50, 0.40), (0.25, 0.20), (0.10, 0.08)]
EXCLUS = ("/Game/Chelles/Neige", "SM_Neige", "Flocon", "/Engine/", "Pluie", "Flaque")

PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
JOURNAL_JSON = os.path.join(PROJET, "Tools", "LOD", "journal_lod.json")
FICHIER_LOG = os.path.join(PROJET, "Tools", "out", "lod.log")
SML = unreal.EditorStaticMeshLibrary
_lignes = []


def log(msg):
    print("[LOD] " + msg)
    _lignes.append(msg)


def ecrire_log():
    try:
        os.makedirs(os.path.dirname(FICHIER_LOG), exist_ok=True)
        with open(FICHIER_LOG, "a", encoding="utf-8") as f:
            f.write("\n===== %s (auto 27/11) =====\n%s\n" % (datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"), "\n".join(_lignes)))
    except Exception:
        pass


def charger_journal():
    try:
        with open(JOURNAL_JSON, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return []


def ecrire_journal(j):
    os.makedirs(os.path.dirname(JOURNAL_JSON), exist_ok=True)
    with open(JOURNAL_JSON, "w", encoding="utf-8") as f:
        json.dump(sorted(set(j)), f, indent=1, ensure_ascii=False)


def candidats():
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    reg.search_all_assets(True)
    chemins = set()
    # 1) ce que la carte utilise
    opts = unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=True, include_hard_package_references=True,
        include_searchable_names=False, include_soft_management_references=False,
        include_hard_management_references=False)
    vus, a_voir = set(), [CARTE]
    while a_voir:
        p = a_voir.pop()
        if p in vus:
            continue
        vus.add(p)
        try:
            deps = reg.get_dependencies(unreal.Name(p), opts) or []
        except Exception:
            deps = []
        for d in deps:
            d = str(d)
            if d.startswith("/Game/") and d not in vus:
                a_voir.append(d)
    for p in vus:
        try:
            for ad in reg.get_assets_by_package_name(unreal.Name(p)) or []:
                if str(ad.asset_class) == "StaticMesh":
                    chemins.add(str(ad.object_path))
        except Exception:
            pass
    log("Carte : %d paquets, %d Static Mesh" % (len(vus), len(chemins)))
    # 2) ce que les plugins posent pendant la partie (Bugatti, Horo, commerces...)
    for dossier in DOSSIERS:
        try:
            for ad in reg.get_assets_by_path(unreal.Name(dossier), True) or []:
                if str(ad.asset_class) == "StaticMesh":
                    chemins.add(str(ad.object_path))
        except Exception as e:
            log("  dossier %s illisible : %s" % (dossier, e))
    return sorted(c for c in chemins if not any(x.lower() in c.lower() for x in EXCLUS))


def options():
    o = unreal.EditorScriptingMeshReductionOptions()
    o.set_editor_property("auto_compute_lod_screen_size", False)
    reglages = []
    for part, ecran in NIVEAUX:
        r = unreal.EditorScriptingMeshReductionSettings()
        r.set_editor_property("percent_triangles", part)
        r.set_editor_property("screen_size", ecran)
        reglages.append(r)
    o.set_editor_property("reduction_settings", reglages)
    return o


def main():
    chemins = candidats()
    log("A examiner : %d Static Mesh" % len(chemins))
    retenus = []
    for ch in chemins:
        try:
            m = unreal.load_asset(ch)
            if not isinstance(m, unreal.StaticMesh) or SML.get_lod_count(m) > 1:
                continue
            n = SML.get_number_verts(m, 0)
            if n >= SEUIL_SOMMETS:
                retenus.append((ch, m, n))
        except Exception:
            pass
    retenus.sort(key=lambda x: -x[2])
    log("Sans LOD et lourds : %d objet(s), %d sommets" % (len(retenus), sum(n for _, _, n in retenus)))
    journal = charger_journal()
    opts = options()
    faits = 0
    for ch, m, n in retenus:
        try:
            if SML.set_lods(m, opts) > 1:
                unreal.EditorAssetLibrary.save_loaded_asset(m, False)
                journal.append(ch)
                faits += 1
                log("  OK %8d sommets  %s" % (n, ch))
            else:
                log("  pas de LOD cree : %s" % ch)
        except Exception as e:
            log("  ECHEC %s : %s" % (ch, e))
        if faits and faits % 10 == 0:
            ecrire_journal(journal)
    ecrire_journal(journal)
    log("TERMINE : LOD ajoutes a %d objet(s)." % faits)


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())
finally:
    ecrire_log()
