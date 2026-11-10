# -*- coding: utf-8 -*-
"""
NETTOYER LES ERREURS DE CHARGEMENT (Window > Developer Tools > Message Log > Load Errors)
Projet CHELLES 3D - Unreal Engine 4.27

UTILISATION
  Dans Unreal : File > Execute Python Script...  puis choisir ce fichier.

  1. Le script lit le journal d'Unreal (Saved/Logs) et retrouve chaque erreur
     "Failed to load ... Referenced by ...".
  2. Il prepare la liste de ce qu'il va faire (SIMULATION : rien n'est modifie).
  3. Une fenetre affiche le resume et demande Oui / Non.
       Non -> rien n'est modifie, la liste reste dans Tools/out/nettoyer_erreurs_chargement.log
       Oui -> copie de sauvegarde de chaque fichier touche, puis nettoyage.
  4. Fermer puis rouvrir Unreal : les erreurs traitees ne reviennent plus.

CE QUE FAIT LE SCRIPT
  Les fichiers cites en "Failed to load" N'EXISTENT DEJA PLUS : on ne peut pas les supprimer.
  Le script s'occupe donc des fichiers qui les reclament encore :
  - le fichier manquant existe ailleurs dans le projet sous le meme nom -> il est rebranche dessus ;
  - mesh (batiment, objet) utilise dans une carte -> materiaux manquants remplaces, le mesh est GARDE ;
  - mesh qu'aucune carte et aucun asset n'utilise -> SUPPRIME (copie gardee) ;
  - materiau / fonction de materiau -> texture manquante remplacee par une texture neutre ;
  - autre asset simple (son...) -> la reference vide est retiree en le reenregistrant.

SECURITE
  - Blueprints, personnage, cartes, structures (inventaire FPS_Game...) ne sont JAMAIS
    supprimes ni reenregistres. S'il existe un remplacant du meme nom, une redirection est
    ajoutee dans Config/DefaultEngine.ini (section [CoreRedirects], effet au prochain demarrage).
    Sinon ils sont listes "a faire a la main".
  - Rien n'est supprime dans /Game/FPS_Game, /Game/Maps, /Game/Personnages, /Game/Character.
  - Copie de chaque fichier avant modification : Saved/Sauvegardes/erreurs_chargement_<date>/
    (pour revenir en arriere : Unreal ferme, recopier le fichier a sa place dans Content).
"""
import glob
import os
import re
import shutil
import time
import traceback

import unreal

PROJ = os.path.abspath(unreal.SystemLibrary.get_project_directory())
CONTENT = os.path.join(PROJ, "Content")
LOGS = os.path.join(PROJ, "Saved", "Logs")
OUT = os.path.join(PROJ, "Tools", "out")
JOURNAL = os.path.join(OUT, "nettoyer_erreurs_chargement.log")
INI = os.path.join(PROJ, "Config", "DefaultEngine.ini")
SAUVEGARDE = os.path.join(PROJ, "Saved", "Sauvegardes",
                          "erreurs_chargement_" + time.strftime("%Y-%m-%d_%H-%M-%S"))

EAL = unreal.EditorAssetLibrary

# Jamais modifies ni reenregistres (seulement redirection ou liste "a la main")
CLASSES_PROTEGEES = {"Blueprint", "AnimBlueprint", "WidgetBlueprint", "World", "UserDefinedStruct",
                     "UserDefinedEnum", "BlueprintGeneratedClass", "LevelSequence"}
# Jamais supprimes
DOSSIERS_PROTEGES = ("/Game/FPS_Game/", "/Game/Maps/", "/Game/Personnages/", "/Game/Character/")

CLASSES_MESH = {"StaticMesh", "SkeletalMesh"}
CLASSES_MATERIAU = {"Material", "MaterialFunction", "MaterialInstanceConstant"}

# mot dans le nom de l'emplacement de materiau -> materiau de remplacement (Starter Content)
SUBSTITUTS = [
    (("glass", "verre", "window", "vitre"), "/Game/StarterContent/Materials/M_Glass"),
    (("brick", "brique"), "/Game/StarterContent/Materials/M_Brick_Clay_New"),
    (("roof", "tile", "toit", "tuile"), "/Game/StarterContent/Materials/M_Brick_Clay_Old"),
    (("stone", "pierre", "rock", "slate", "limestone"), "/Game/StarterContent/Materials/M_Brick_Cut_Stone"),
    (("wood", "bois", "door", "porte", "counter", "table", "chair", "frame"), "/Game/StarterContent/Materials/M_Wood_Oak"),
    (("metal", "steel", "trim", "iron", "newspaper"), "/Game/StarterContent/Materials/M_Metal_Steel"),
    (("concrete", "beton", "drywall", "plaster", "platre", "wall", "mur"), "/Game/StarterContent/Materials/M_Concrete_Poured"),
    (("grass", "herbe"), "/Game/StarterContent/Materials/M_Ground_Grass"),
]
SUBSTITUT_PAR_DEFAUT = "/Game/StarterContent/Materials/M_Basic_Wall"
TEXTURES_NEUTRES = ["/Engine/EngineResources/Black", "/Engine/EngineResources/DefaultTexture",
                    "/Engine/EngineResources/WhiteSquareTexture"]

RE_ASSETLOG = re.compile(r"\[AssetLog\] (.+?\.u(?:asset|map)): Failed to load '(/Game/[^']+)'")
RE_LOADERR = re.compile(r"LoadErrors: Failed to load (/Game/[^ .]+)(?:\.\S+)? Referenced by (\S+)")


# ----------------------------------------------------------------------------- outils
def log(msg=""):
    unreal.log(str(msg))
    with open(JOURNAL, "a", encoding="utf-8") as f:
        f.write(str(msg) + "\n")


def nom_court(paquet):
    return paquet.rsplit("/", 1)[-1]


def fichier_vers_paquet(chemin):
    chemin = os.path.normpath(chemin)
    if chemin.lower().startswith(CONTENT.lower() + os.sep):
        rel = chemin[len(CONTENT) + 1:]
    else:
        i = chemin.lower().find(os.sep + "content" + os.sep)
        if i < 0:
            return None
        rel = chemin[i + 9:]
    return "/Game/" + os.path.splitext(rel)[0].replace(os.sep, "/")


def paquet_vers_fichier(paquet):
    rel = paquet[len("/Game/"):].replace("/", os.sep)
    for ext in (".uasset", ".umap"):
        f = os.path.join(CONTENT, rel + ext)
        if os.path.exists(f):
            return f
    return None


def sauvegarder_fichier(chemin):
    if not chemin or not os.path.exists(chemin):
        return
    if chemin.lower().startswith(PROJ.lower()):
        rel = os.path.relpath(chemin, PROJ)
    else:
        rel = os.path.basename(chemin)
    dest = os.path.join(SAUVEGARDE, rel)
    if not os.path.exists(dest):
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        shutil.copy2(chemin, dest)


def normaliser(nom):
    n = (nom or "").lower().strip()
    n = re.sub(r"[ ._-]?\d{1,3}$", "", n)  # "Glass_001", "Glass.001" -> "glass"
    return n


# ----------------------------------------------------------------------------- 1. journal
def analyser_journal(chemin):
    paires = {}        # paquet referent -> set(paquets manquants)
    par_expression = {}  # "MaterialExpressionTextureSample_1" -> paquet manquant
    sans_fichier = {}  # paquet manquant -> set(nom du referent) (lignes LoadErrors seules)
    try:
        with open(chemin, "r", encoding="utf-8", errors="ignore") as f:
            for ligne in f:
                m = RE_ASSETLOG.search(ligne)
                if m:
                    ref = fichier_vers_paquet(m.group(1))
                    if ref:
                        paires.setdefault(ref, set()).add(m.group(2))
                    continue
                m = RE_LOADERR.search(ligne)
                if m:
                    manquant, par = m.group(1), m.group(2)
                    sans_fichier.setdefault(manquant, set()).add(par)
                    if par.startswith("MaterialExpression"):
                        par_expression[par] = manquant
    except Exception as e:
        log("   (lecture impossible de %s : %s)" % (chemin, e))
    deja = set()
    for s in paires.values():
        deja |= s
    sans_fichier = {k: v for k, v in sans_fichier.items() if k not in deja}
    return paires, par_expression, sans_fichier


def lire_erreurs():
    journaux = [p for p in glob.glob(os.path.join(LOGS, "*.log"))]
    journaux.sort(key=os.path.getmtime, reverse=True)
    for p in journaux:
        paires, par_expr, sans_fichier = analyser_journal(p)
        if paires or sans_fichier:
            return p, paires, par_expr, sans_fichier
    return None, {}, {}, {}


# ----------------------------------------------------------------------------- 2. registre
class Index(object):
    def __init__(self):
        self.reg = unreal.AssetRegistryHelpers.get_asset_registry()
        self.par_nom = {}
        self.classe = {}
        for a in self.reg.get_assets_by_path("/Game", True):
            pk, cl = str(a.package_name), str(a.asset_class)
            self.classe.setdefault(pk, cl)
            if cl != "ObjectRedirector":
                self.par_nom.setdefault(str(a.asset_name).lower(), []).append(pk)
        self.opts = unreal.AssetRegistryDependencyOptions(
            include_soft_package_references=True, include_hard_package_references=True,
            include_searchable_names=False, include_soft_management_references=False,
            include_hard_management_references=False)

    def existe(self, paquet):
        return paquet in self.classe

    def dependances(self, paquet):
        return set(str(d) for d in (self.reg.get_dependencies(paquet, self.opts) or []))

    def utilise_par(self, paquet):
        return sorted(set(str(r) for r in (self.reg.get_referencers(paquet, self.opts) or []))
                      - {paquet})

    def remplacant(self, manquant, familles=None):
        """Un asset existant du meme nom (le plus ressemblant), ou None."""
        cands = [p for p in self.par_nom.get(nom_court(manquant).lower(), []) if p != manquant]
        if familles:
            cands = [p for p in cands if self.classe.get(p) in familles]
        if not cands:
            return None
        a = manquant.lower().split("/")

        def score(p):
            b = p.lower().split("/")
            n = 0
            while n < min(len(a), len(b)) and a[-1 - n] == b[-1 - n]:
                n += 1
            return (n, p.startswith("/Game/FPS_Game/"), -len(p))
        return max(cands, key=score)


def famille_attendue(manquant):
    n = nom_court(manquant).lower()
    if n.startswith(("m_", "mi_", "mm_", "inst_", "wm_", "mat_")) or n.endswith(("_mat", "_inst")):
        return CLASSES_MATERIAU - {"MaterialFunction"}
    if n.startswith(("t_", "tex_")) and not n.endswith("_mat"):
        return {"Texture2D", "TextureCube", "TextureRenderTarget2D"}
    return None


# ----------------------------------------------------------------------------- 3. plan
def preparer_plan(idx, paires, sans_fichier):
    plan = []  # dicts
    for ref, manquants in sorted(paires.items()):
        e = {"ref": ref, "manquants": sorted(manquants), "classe": idx.classe.get(ref, "?"),
             "action": None, "remplacants": {}, "detail": ""}
        plan.append(e)
        if not idx.existe(ref):
            e["action"] = "rien"
            e["detail"] = "ce fichier n'existe plus lui non plus (erreur deja disparue)"
            continue
        deps = idx.dependances(ref)
        reste = [m for m in manquants if m in deps and not idx.existe(m)]
        cascade = [m for m in manquants if idx.existe(m)]
        if not reste:
            e["action"] = "rien"
            e["detail"] = ("le fichier cite existe bien : erreur en cascade, rien a supprimer"
                           if cascade else "deja repare (sera confirme au prochain demarrage)")
            continue
        e["manquants"] = reste
        for m in reste:
            r = idx.remplacant(m, famille_attendue(m))
            if r:
                e["remplacants"][m] = r
        cl = e["classe"]
        protege_modif = cl in CLASSES_PROTEGEES
        protege_suppr = protege_modif or ref.startswith(DOSSIERS_PROTEGES)

        if protege_modif:
            e["action"] = "rediriger" if e["remplacants"] else "a_la_main"
            if len(e["remplacants"]) < len(reste):
                e["detail"] = consigne_main(cl)
        elif cl in CLASSES_MESH:
            utilisateurs = idx.utilise_par(ref)
            e["utilise_par"] = utilisateurs
            if not utilisateurs and not protege_suppr:
                e["action"] = "supprimer"
                e["detail"] = "aucune carte ni aucun asset ne l'utilise"
            else:
                e["action"] = "reparer_mesh"
                e["detail"] = "utilise par %d fichier(s) : on le garde" % len(utilisateurs)
        elif cl in CLASSES_MATERIAU:
            e["action"] = "reparer_materiau"
        else:
            e["action"] = "rediriger" if len(e["remplacants"]) == len(reste) else "reenregistrer"

    # erreurs sans fichier referent identifie (ex. structure de l'inventaire)
    for m, pars in sorted(sans_fichier.items()):
        e = {"ref": None, "manquants": [m], "classe": "?", "remplacants": {},
             "action": "rien", "detail": "cite par : %s" % ", ".join(sorted(pars))}
        if idx.existe(m):
            e["detail"] += " -> le fichier existe bien : erreur en cascade d'une autre erreur, ne rien supprimer"
        else:
            r = idx.remplacant(m, famille_attendue(m))
            if r:
                e["action"] = "rediriger"
                e["remplacants"][m] = r
            else:
                e["action"] = "a_la_main"
        plan.append(e)
    return plan


def consigne_main(classe):
    if classe == "World":
        return "carte protegee : l'ouvrir puis File > Save Current (la reference vide disparait)"
    return "protege : l'ouvrir, cliquer Compiler puis Enregistrer (la reference vide disparait)"


LIBELLES = {
    "rien": "RIEN A FAIRE",
    "supprimer": "SUPPRIMER",
    "reparer_mesh": "REPARER (materiaux)",
    "reparer_materiau": "REPARER (textures)",
    "reenregistrer": "REENREGISTRER",
    "rediriger": "REDIRIGER (DefaultEngine.ini)",
    "a_la_main": "A FAIRE A LA MAIN",
}


def afficher_plan(plan):
    for e in plan:
        titre = e["ref"] or "(referent inconnu)"
        log("- %s  [%s]  ->  %s" % (titre, e["classe"], LIBELLES[e["action"]]))
        if e.get("detail"):
            log("     %s" % e["detail"])
        for m in e["manquants"]:
            r = e["remplacants"].get(m)
            log("     manque : %s%s" % (m, ("   => remplace par " + r) if r else ""))
        for u in (e.get("utilise_par") or [])[:5]:
            log("     utilise dans : %s" % u)


# ----------------------------------------------------------------------------- 4. actions
def substitut_par_mots(nom):
    n = (nom or "").lower()
    for mots, chemin in SUBSTITUTS:
        if any(m in n for m in mots):
            if EAL.does_asset_exist(chemin):
                return chemin
    return SUBSTITUT_PAR_DEFAUT if EAL.does_asset_exist(SUBSTITUT_PAR_DEFAUT) else None


def charger_materiau(chemin):
    if not chemin:
        return None
    o = EAL.load_asset(chemin)
    return o if isinstance(o, unreal.MaterialInterface) else None


def reparer_mesh(e):
    obj = EAL.load_asset(e["ref"])
    if obj is None:
        log("     ECHEC : impossible de charger %s" % e["ref"])
        return False
    manquants = {normaliser(nom_court(m)): m for m in e["manquants"]}
    if isinstance(obj, unreal.StaticMesh):
        prop = "static_materials"
    elif isinstance(obj, unreal.SkeletalMesh):
        prop = "materials"
    else:
        log("     ECHEC : type inattendu %s" % type(obj).__name__)
        return False
    emplacements = obj.get_editor_property(prop)
    change = 0
    for i, s in enumerate(emplacements):
        if s.get_editor_property("material_interface") is not None:
            continue
        nom = str(s.get_editor_property("material_slot_name"))
        m = manquants.get(normaliser(nom))
        mat, origine = None, ""
        if m and m in e["remplacants"]:
            mat, origine = charger_materiau(e["remplacants"][m]), "meme nom"
        if mat is None:
            mat, origine = charger_materiau(substitut_par_mots(nom + " " + (nom_court(m) if m else ""))), "ressemblant"
        if mat is not None:
            s.set_editor_property("material_interface", mat)
            change += 1
            log("     emplacement %d (%s) -> %s (%s)" % (i, nom, mat.get_name(), origine))
        else:
            log("     emplacement %d (%s) -> materiau par defaut" % (i, nom))
    if change:
        # une seule mise a jour du mesh (et pas une par emplacement : evite le blocage sur les gros meshes)
        obj.set_editor_property(prop, emplacements)
    return EAL.save_loaded_asset(obj, False)


def texture_neutre():
    for p in TEXTURES_NEUTRES:
        t = EAL.load_asset(p)
        if isinstance(t, unreal.Texture):
            return t
    return None


def reparer_materiau(e, par_expression):
    obj = EAL.load_asset(e["ref"])
    if obj is None:
        log("     ECHEC : impossible de charger %s" % e["ref"])
        return False
    racine = obj.get_path_name()
    neutre = None
    for expr in unreal.ObjectIterator(unreal.MaterialExpressionTextureSample):
        if not expr.get_path_name().startswith(racine + ":"):
            continue
        if expr.get_editor_property("texture") is not None:
            continue
        m = par_expression.get(expr.get_name())
        tex = None
        if m and m in e["remplacants"]:
            tex = EAL.load_asset(e["remplacants"][m])
            if not isinstance(tex, unreal.Texture):
                tex = None
        if tex is None:
            neutre = neutre or texture_neutre()
            tex = neutre
            try:
                expr.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
            except Exception:
                pass
        if tex is not None:
            expr.set_editor_property("texture", tex)
            log("     %s -> %s" % (expr.get_name(), tex.get_name()))
    if isinstance(obj, unreal.MaterialInstanceConstant) and obj.get_editor_property("parent") is None:
        parent = charger_materiau(substitut_par_mots(obj.get_name()))
        if parent:
            unreal.MaterialEditingLibrary.set_material_instance_parent(obj, parent)
            log("     parent manquant -> %s" % parent.get_name())
    try:
        if isinstance(obj, unreal.MaterialFunction):
            unreal.MaterialEditingLibrary.update_material_function(obj, None)
        elif isinstance(obj, unreal.Material):
            unreal.MaterialEditingLibrary.recompile_material(obj)
    except Exception as ex:
        log("     (recompilation : %s)" % ex)
    return EAL.save_loaded_asset(obj, False)


def reenregistrer(e):
    obj = EAL.load_asset(e["ref"])
    if obj is None:
        log("     ECHEC : impossible de charger %s" % e["ref"])
        return False
    return EAL.save_loaded_asset(obj, False)


def supprimer(e):
    return EAL.delete_asset(e["ref"])


def ecrire_redirections(redirections):
    if not redirections:
        return 0
    sauvegarder_fichier(INI)
    with open(INI, "r", encoding="utf-8", errors="ignore") as f:
        texte = f.read()
    nouvelles = []
    for ancien, nouveau in sorted(redirections.items()):
        if ('OldName="%s"' % ancien) in texte:
            continue
        nouvelles.append('+PackageRedirects=(OldName="%s",NewName="%s")' % (ancien, nouveau))
    if not nouvelles:
        return 0
    bloc = "\n".join(nouvelles)
    if re.search(r"^\[CoreRedirects\]\s*$", texte, re.M):
        texte = re.sub(r"^(\[CoreRedirects\]\s*)$", lambda m: m.group(1) + "\n" + bloc, texte, count=1, flags=re.M)
    else:
        texte = texte.rstrip("\r\n") + "\n\n[CoreRedirects]\n" + bloc + "\n"
    with open(INI, "w", encoding="utf-8", newline="\r\n") as f:
        f.write(texte.replace("\r\n", "\n"))
    return len(nouvelles)


# ----------------------------------------------------------------------------- principal
def run():
    os.makedirs(OUT, exist_ok=True)
    open(JOURNAL, "w").close()
    log("NETTOYAGE DES ERREURS DE CHARGEMENT - %s" % time.strftime("%d/%m/%Y %H:%M"))

    chemin, paires, par_expr, sans_fichier = lire_erreurs()
    if not chemin:
        unreal.EditorDialog.show_message("Erreurs de chargement",
                                         "Aucune erreur 'Failed to load' trouvee dans Saved/Logs.",
                                         unreal.AppMsgType.OK)
        log("Aucune erreur trouvee dans %s" % LOGS)
        return
    nb = sum(len(v) for v in paires.values()) + len(sans_fichier)
    log("1/4 journal lu : %s" % chemin)
    log("    %d erreur(s) distincte(s), %d fichier(s) en cause" % (nb, len(paires)))

    log("2/4 lecture du registre d'assets (1 a 2 minutes)...")
    with unreal.ScopedSlowTask(1, "Lecture du registre d'assets...") as t:
        t.make_dialog(False)
        t.enter_progress_frame(1)
        idx = Index()
    plan = preparer_plan(idx, paires, sans_fichier)

    log("")
    log("3/4 SIMULATION (rien n'est encore modifie) :")
    afficher_plan(plan)

    compte = {}
    for e in plan:
        compte[e["action"]] = compte.get(e["action"], 0) + 1
    a_faire = [e for e in plan if e["action"] not in ("rien", "a_la_main")]
    resume = ["%d erreur(s) trouvee(s) dans le journal d'Unreal.", ""]
    resume[0] = resume[0] % nb
    for cle in ("supprimer", "reparer_mesh", "reparer_materiau", "reenregistrer", "rediriger", "a_la_main", "rien"):
        if compte.get(cle):
            noms = [nom_court(e["ref"]) if e["ref"] else nom_court(e["manquants"][0]) for e in plan if e["action"] == cle]
            resume.append("%s : %s" % (LIBELLES[cle], ", ".join(noms)))
    resume += ["", "Le detail est dans Tools/out/nettoyer_erreurs_chargement.log",
               "Une copie de chaque fichier touche sera faite avant.", ""]

    if not a_faire:
        resume.append("Rien a modifier automatiquement.")
        unreal.EditorDialog.show_message("Erreurs de chargement", "\n".join(resume), unreal.AppMsgType.OK)
        log("Rien a modifier automatiquement.")
        return

    resume.append("Appliquer maintenant ?  (Non = ne rien modifier)")
    rep = unreal.EditorDialog.show_message("Erreurs de chargement - confirmation", "\n".join(resume),
                                           unreal.AppMsgType.YES_NO)
    if rep != unreal.AppReturnType.YES:
        log("")
        log("ANNULE par l'utilisateur : rien n'a ete modifie.")
        return

    log("")
    log("4/4 APPLICATION (copies dans %s)" % SAUVEGARDE)
    redirections = {}
    ok, echecs = 0, []
    with unreal.ScopedSlowTask(len(a_faire), "Nettoyage des erreurs de chargement...") as t:
        t.make_dialog(True)
        for e in a_faire:
            if t.should_cancel():
                log("   arrete par l'utilisateur")
                break
            t.enter_progress_frame(1, nom_court(e["ref"] or e["manquants"][0]))
            log("- %s : %s" % (e["ref"] or e["manquants"][0], LIBELLES[e["action"]]))
            try:
                if e["action"] == "rediriger":
                    redirections.update(e["remplacants"])
                    ok += 1
                    continue
                sauvegarder_fichier(paquet_vers_fichier(e["ref"]))
                if e["action"] == "supprimer":
                    r = supprimer(e)
                elif e["action"] == "reparer_mesh":
                    r = reparer_mesh(e)
                elif e["action"] == "reparer_materiau":
                    r = reparer_materiau(e, par_expr)
                else:
                    r = reenregistrer(e)
                if r:
                    ok += 1
                    log("     OK")
                else:
                    echecs.append(e["ref"])
                    log("     ECHEC (voir Output Log)")
            except Exception:
                echecs.append(e["ref"])
                log("     ERREUR :\n" + traceback.format_exc())

    n_redir = ecrire_redirections(redirections)
    if n_redir:
        log("")
        log("%d redirection(s) ajoutee(s) dans Config/DefaultEngine.ini [CoreRedirects] :" % n_redir)
        for a, n in sorted(redirections.items()):
            log("   %s  ->  %s" % (a, n))

    main = [e for e in plan if e["action"] == "a_la_main"]
    if main:
        log("")
        log("A FAIRE A LA MAIN (proteges, aucun remplacant trouve) :")
        for e in main:
            log("   %s : %s" % (e["ref"] or nom_court(e["manquants"][0]), consigne_main(e["classe"]) if e["ref"] else e["detail"]))

    fin = ["%d action(s) reussie(s), %d echec(s)." % (ok, len(echecs))]
    if n_redir:
        fin.append("%d redirection(s) ajoutee(s) dans DefaultEngine.ini." % n_redir)
    if main:
        fin.append("%d element(s) a faire a la main (voir le journal)." % len(main))
    fin += ["", "Fermez puis rouvrez Unreal pour verifier Load Errors.",
            "Copies de securite : " + SAUVEGARDE]
    log("")
    log("TERMINE. " + " ".join(fin[:3]))
    unreal.EditorDialog.show_message("Erreurs de chargement - termine", "\n".join(fin), unreal.AppMsgType.OK)


try:
    run()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())
    unreal.EditorDialog.show_message("Erreurs de chargement", "Erreur : voir Tools/out/nettoyer_erreurs_chargement.log",
                                     unreal.AppMsgType.OK)
