"""SCRIPT A LANCER DANS UNREAL 4.27 : File > Execute Python Script... > ce fichier.

AJOUTE DES LOD aux objets lourds (Static Mesh) de la carte Maps/Rachid.
Un LOD = une version plus simple du meme objet, affichee automatiquement quand il est loin :
de pres rien ne change, de loin la carte graphique dessine beaucoup moins de triangles.

Comment ca marche :
  - le script cherche les Static Mesh dont la carte Rachid a besoin (batiments, props, voitures...) ;
  - il garde ceux qui ont plus de SEUIL_SOMMETS sommets et AUCUN LOD (les LOD deja faits ne sont pas touches) ;
  - il affiche combien il en a trouve et DEMANDE Oui/Non ;
  - si Oui : il cree 3 LOD (50 %, 25 %, 10 % des triangles) pour chacun, puis enregistre.

C'est reversible : la liste des objets modifies est notee dans Tools/LOD/journal_lod.json,
et retirer_lod.py enleve ces LOD (l'objet d'origine, le LOD 0, n'est jamais modifie).
Duree : de quelques minutes a une heure selon le nombre d'objets. On peut annuler (bouton Cancel) :
ce qui est deja fait reste fait et note dans le journal.
Journal : Tools/out/lod.log
"""
import datetime, json, os, traceback
import unreal

# ----------------------------------------------------------------------------------------------- reglages
CARTE = "/Game/Maps/Rachid"
SEUIL_SOMMETS = 3000          # en dessous, l'objet est deja leger : on ne touche pas
NIVEAUX = [                   # (part des triangles gardee, taille a l'ecran a partir de laquelle ce LOD s'affiche)
    (1.00, 1.00),             # LOD 0 = l'objet d'origine
    (0.50, 0.40),
    (0.25, 0.20),
    (0.10, 0.08),
]
# Objets a ne jamais toucher (morceaux de texte dans le chemin) : les flocons de neige sont un maillage special.
EXCLUS = ("/Game/Chelles/Neige", "SM_Neige", "Flocon", "/Engine/")

PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DOSSIER_OUTIL = os.path.join(PROJET, "Tools", "LOD")
JOURNAL_JSON = os.path.join(DOSSIER_OUTIL, "journal_lod.json")
FICHIER_LOG = os.path.join(PROJET, "Tools", "out", "lod.log")
_lignes = []
SML = unreal.EditorStaticMeshLibrary


def log(msg):
    unreal.log("[LOD] " + msg)
    _lignes.append(msg)


def ecrire_log():
    try:
        os.makedirs(os.path.dirname(FICHIER_LOG), exist_ok=True)
        with open(FICHIER_LOG, "a", encoding="utf-8") as f:
            f.write("\n===== %s =====\n%s\n" % (datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"), "\n".join(_lignes)))
    except Exception:
        pass


def paquets_de_la_carte():
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    reg.search_all_assets(True)
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
    return reg, vus


def candidats():
    reg, paquets = paquets_de_la_carte()
    log("Paquets utilises par %s : %d" % (CARTE, len(paquets)))
    chemins = set()
    for p in paquets:
        try:
            for ad in reg.get_assets_by_package_name(unreal.Name(p)) or []:
                if str(ad.asset_class) == "StaticMesh":
                    chemins.add(str(ad.object_path))
        except Exception:
            pass
    return sorted(c for c in chemins if not any(x.lower() in c.lower() for x in EXCLUS))


def analyser(chemins):
    retenus, illisibles = [], 0
    with unreal.ScopedSlowTask(len(chemins), "Analyse des objets de la carte...") as t:
        t.make_dialog(True)
        for ch in chemins:
            if t.should_cancel():
                return None, illisibles
            t.enter_progress_frame(1, ch)
            try:
                m = unreal.load_asset(ch)
                if not isinstance(m, unreal.StaticMesh):
                    continue
                if SML.get_lod_count(m) > 1:
                    continue
                n = SML.get_number_verts(m, 0)
                if n >= SEUIL_SOMMETS:
                    retenus.append((ch, m, n))
            except Exception:
                illisibles += 1
    retenus.sort(key=lambda x: -x[2])
    return retenus, illisibles


def charger_journal():
    try:
        with open(JOURNAL_JSON, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return []


def ecrire_journal(j):
    os.makedirs(DOSSIER_OUTIL, exist_ok=True)
    with open(JOURNAL_JSON, "w", encoding="utf-8") as f:
        json.dump(sorted(set(j)), f, indent=1, ensure_ascii=False)


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


def appliquer(retenus):
    journal = charger_journal()
    opts = options()
    faits = 0
    with unreal.ScopedSlowTask(len(retenus), "Creation des LOD (peut etre long)...") as t:
        t.make_dialog(True)
        for ch, m, n in retenus:
            if t.should_cancel():
                log("Arret demande : %d objet(s) deja traites (ils gardent leurs LOD)." % faits)
                break
            t.enter_progress_frame(1, "%s (%d sommets)" % (ch, n))
            try:
                if SML.set_lods(m, opts) > 1:
                    unreal.EditorAssetLibrary.save_loaded_asset(m, False)
                    journal.append(ch)
                    faits += 1
                else:
                    log("  Pas de LOD cree pour %s" % ch)
            except Exception as e:
                log("  ECHEC %s : %s" % (ch, e))
            if faits and faits % 20 == 0:
                ecrire_journal(journal)
    ecrire_journal(journal)
    return faits


def main():
    log("Recherche des objets de plus de %d sommets sans LOD..." % SEUIL_SOMMETS)
    chemins = candidats()
    log("Static Mesh utilises par la carte : %d" % len(chemins))
    retenus, illisibles = analyser(chemins)
    if retenus is None:
        log("Analyse annulee.")
        return
    total = sum(n for _, _, n in retenus)
    log("A traiter : %d objet(s), %d sommets au total (%d illisible(s))" % (len(retenus), total, illisibles))
    for ch, m, n in retenus[:40]:
        log("  %8d sommets  %s" % (n, ch))
    if len(retenus) > 40:
        log("  ... et %d autre(s)." % (len(retenus) - 40))
    if not retenus:
        unreal.EditorDialog.show_message("LOD", "Tous les objets lourds ont deja des LOD : rien a faire.", unreal.AppMsgType.OK)
        return
    q = ("%d objet(s) lourds sans LOD trouves dans la carte.\n\n"
         "Leur ajouter 3 LOD (50 %%, 25 %%, 10 %% des triangles) ?\n"
         "De pres rien ne change ; de loin le jeu dessine beaucoup moins de triangles.\n"
         "retirer_lod.py enleve ces LOD si besoin.\n\n"
         "La liste est dans Window > Developer Tools > Output Log et dans Tools/out/lod.log." % len(retenus))
    if unreal.EditorDialog.show_message("Ajouter des LOD", q, unreal.AppMsgType.YES_NO) != unreal.AppReturnType.YES:
        log("Annule par l'utilisateur : rien n'a ete modifie.")
        return
    faits = appliquer(retenus)
    log("Termine : LOD ajoutes a %d objet(s). Lance Play pour verifier." % faits)
    unreal.EditorDialog.show_message("LOD", "LOD ajoutes a %d objet(s).\nLance Play pour verifier." % faits, unreal.AppMsgType.OK)


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())
    unreal.log_error(traceback.format_exc())
finally:
    ecrire_log()
