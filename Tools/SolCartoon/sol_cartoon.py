"""SCRIPT A LANCER DANS UNREAL 4.27, carte Maps/Rachid ouverte : File > Execute Python Script... > ce fichier.

SOL LOW POLY STYLE CARTOON + SOL ET HERBE PLUS LEGERS (plus de FPS).

Ce que fait le script :
  1. STYLE : il cree un materiau de sol /Game/Chelles/SolCartoon/M_SolCartoon (+ MI_SolCartoon pour regler
     les couleurs) : herbe vert vif avec grandes taches plus claires, terre orangee dans les pentes,
     peu de reflets, et des FACETTES "low poly" (chaque triangle du sol est eclaire a plat).
     Il le pose sur le Landscape et sur la dalle Chelles_Tile_Ground.
  2. LANDSCAPE PLUS LEGER : le Landscape passe plus vite a ses versions simplifiees (LOD) quand on s'eloigne.
  3. HERBE / FOLIAGE PLUS LEGERS : l'herbe et les petites plantes disparaissent plus pres (80 m au lieu de
     l'infini) et ne font plus d'ombre ; les arbres disparaissent a 300 m ; l'herbe automatique du Landscape
     (Landscape Grass) est 2 fois moins dense.
  Le script n'efface AUCUNE plante et ne touche pas aux routes, batiments, trottoirs.

Il affiche d'abord ce qu'il a trouve et DEMANDE Oui/Non avant de modifier.
C'est reversible : tout ce qui est change est note dans Tools/SolCartoon/journal_sol_cartoon.json ;
retirer_sol_cartoon.py remet exactement les valeurs d'avant. Une copie de Rachid.umap est aussi faite
dans Tools/SolCartoon/sauvegarde/ avant la premiere modification.
Journal texte : Tools/out/sol_cartoon.log

Pour changer les couleurs apres coup : Content Browser > Chelles > SolCartoon > double-clic sur MI_SolCartoon.
"""
import datetime, json, os, shutil, traceback
import unreal

# ----------------------------------------------------------------------------------------------- reglages
CARTE = "/Game/Maps/Rachid"
DOSSIER_ASSETS = "/Game/Chelles/SolCartoon"

FAIRE_STYLE_CARTOON = True     # materiau de sol cartoon sur le Landscape + Chelles_Tile_Ground
FAIRE_LANDSCAPE_LEGER = True   # LOD du Landscape plus agressifs
FAIRE_FOLIAGE_LEGER = True     # herbe/plantes peintes : distance d'affichage + ombres
FAIRE_HERBE_AUTO_LEGERE = True # Landscape Grass Types : densite + distance + ombres

# Couleurs (R, V, B entre 0 et 1) : modifiables ensuite dans MI_SolCartoon
HERBE_FONCEE = (0.10, 0.42, 0.06)
HERBE_CLAIRE = (0.32, 0.70, 0.12)
TERRE_PENTE = (0.55, 0.33, 0.13)
FACETTES_LOW_POLY = 1.0        # 1 = facettes bien visibles, 0 = sol lisse
TAILLE_TACHES_M = 25.0         # taille des grandes taches d'herbe claire, en metres

# Landscape (valeurs Unreal par defaut : 1.0 / 1.75 / 2.0). Le script ne fait que "simplifier plus tot".
LANDSCAPE_LOD0_SCREEN_SIZE = 2.0     # plus grand = moins de morceaux en pleine definition
LANDSCAPE_LOD0_DISTRIBUTION = 1.25   # plus petit = la pleine definition reste plus pres du joueur
LANDSCAPE_LOD_DISTRIBUTION = 1.5     # plus petit = les versions simplifiees arrivent plus vite

# Foliage peint (outil Foliage). Distances en centimetres.
HAUTEUR_PETITE_PLANTE_CM = 300       # en dessous : herbe, fleurs, buissons ; au-dessus : arbres
CULL_PETITES_CM = 8000               # 80 m
CULL_GRANDES_CM = 30000              # 300 m
OMBRES_PETITES = False               # les petites plantes ne projettent plus d'ombre

# Landscape Grass Types (herbe generee automatiquement par le materiau du Landscape)
FACTEUR_DENSITE_HERBE = 0.5
CULL_HERBE_AUTO_CM = 6000            # 60 m

# ----------------------------------------------------------------------------------------------- fichiers
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DOSSIER_OUTIL = os.path.join(PROJET, "Tools", "SolCartoon")
JOURNAL_JSON = os.path.join(DOSSIER_OUTIL, "journal_sol_cartoon.json")
FICHIER_LOG = os.path.join(PROJET, "Tools", "out", "sol_cartoon.log")
_lignes = []
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
ELL = unreal.EditorLevelLibrary


def log(msg):
    unreal.log("[SolCartoon] " + msg)
    _lignes.append(msg)


def ecrire_log():
    try:
        os.makedirs(os.path.dirname(FICHIER_LOG), exist_ok=True)
        with open(FICHIER_LOG, "a", encoding="utf-8") as f:
            f.write("\n===== %s =====\n%s\n" % (datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"), "\n".join(_lignes)))
    except Exception:
        pass


def charger_journal():
    try:
        with open(JOURNAL_JSON, "r", encoding="utf-8") as f:
            j = json.load(f)
    except Exception:
        j = {}
    for k in ("landscapes", "tuiles_sol", "foliage", "herbe_auto"):
        j.setdefault(k, {})
    return j


def ecrire_journal(j):
    os.makedirs(DOSSIER_OUTIL, exist_ok=True)
    with open(JOURNAL_JSON, "w", encoding="utf-8") as f:
        json.dump(j, f, indent=1, ensure_ascii=False)


def chemin(obj):
    return obj.get_path_name() if obj else None


# ----------------------------------------------------------------------------------------------- recherche
def tous_les_acteurs():
    return ELL.get_all_level_actors()


def trouver_landscapes(acteurs):
    return [a for a in acteurs if isinstance(a, unreal.LandscapeProxy)]


def trouver_tuiles_sol(acteurs):
    res = []
    for a in acteurs:
        try:
            if a.get_actor_label().startswith("Chelles_Tile_Ground") and isinstance(a, unreal.StaticMeshActor):
                res.append(a)
        except Exception:
            pass
    return res


def trouver_foliage(acteurs):
    comps = []
    for a in acteurs:
        if isinstance(a, unreal.InstancedFoliageActor):
            for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent) or []:
                if c.static_mesh and c.get_instance_count() > 0:
                    comps.append(c)
    return comps


def hauteur_mesh(m):
    try:
        b = m.get_bounding_box()
        return b.max.z - b.min.z
    except Exception:
        return 0.0


def trouver_herbe_auto():
    """Landscape Grass Types utilises par la carte (en suivant les dependances de Rachid)."""
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    opts = unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=True, include_hard_package_references=True,
        include_searchable_names=False, include_soft_management_references=False,
        include_hard_management_references=False)
    vus, a_voir, res = set(), [CARTE], []
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
                if str(ad.asset_class) == "LandscapeGrassType":
                    res.append(str(ad.object_path))
        except Exception:
            pass
    return sorted(set(res))


# ----------------------------------------------------------------------------------------------- materiau
def _expr(mat, classe, x, y):
    return MEL.create_material_expression(mat, classe, x, y)


def _scalaire(mat, nom, valeur, x, y):
    e = _expr(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", nom)
    e.set_editor_property("default_value", float(valeur))
    return e


def _couleur(mat, nom, rgb, x, y):
    e = _expr(mat, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", nom)
    e.set_editor_property("default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return e


def _lier(a, sortie, b, entree):
    if not MEL.connect_material_expressions(a, sortie, b, entree):
        raise RuntimeError("liaison impossible %s.%s -> %s.%s" % (a.get_name(), sortie, b.get_name(), entree))


def construire_materiau():
    nom_mat, nom_mi = "M_SolCartoon", "MI_SolCartoon"
    ch_mat, ch_mi = DOSSIER_ASSETS + "/" + nom_mat, DOSSIER_ASSETS + "/" + nom_mi
    tools = unreal.AssetToolsHelpers.get_asset_tools()

    if EAL.does_asset_exist(ch_mat):
        mat = EAL.load_asset(ch_mat)
        log("Materiau deja present : %s (reutilise)" % ch_mat)
    else:
        mat = tools.create_asset(nom_mat, DOSSIER_ASSETS, unreal.Material, unreal.MaterialFactoryNew())
        # normale en espace monde : necessaire pour les facettes low poly
        mat.set_editor_property("tangent_space_normal", False)

        # --- facettes : normale d'un triangle = produit vectoriel des derivees de la position monde
        wp = _expr(mat, unreal.MaterialExpressionWorldPosition, -1900, 400)
        ddx = _expr(mat, unreal.MaterialExpressionDDX, -1650, 340)
        ddy = _expr(mat, unreal.MaterialExpressionDDY, -1650, 460)
        _lier(wp, "", ddx, "")
        _lier(wp, "", ddy, "")
        croix = _expr(mat, unreal.MaterialExpressionCrossProduct, -1450, 400)
        _lier(ddy, "", croix, "A")
        _lier(ddx, "", croix, "B")
        nfac = _expr(mat, unreal.MaterialExpressionNormalize, -1250, 400)
        _lier(croix, "", nfac, "")
        # toujours tournee vers le ciel : n * signe(n.z)
        nz = _expr(mat, unreal.MaterialExpressionComponentMask, -1050, 500)
        nz.set_editor_property("r", False); nz.set_editor_property("g", False)
        nz.set_editor_property("b", True); nz.set_editor_property("a", False)
        _lier(nfac, "", nz, "")
        signe = _expr(mat, unreal.MaterialExpressionSign, -900, 500)
        _lier(nz, "", signe, "")
        nhaut = _expr(mat, unreal.MaterialExpressionMultiply, -750, 400)
        _lier(nfac, "", nhaut, "A")
        _lier(signe, "", nhaut, "B")
        # melange sol lisse / facettes
        nlisse = _expr(mat, unreal.MaterialExpressionVertexNormalWS, -750, 250)
        p_fac = _scalaire(mat, "FacettesLowPoly", FACETTES_LOW_POLY, -750, 560)
        nmix = _expr(mat, unreal.MaterialExpressionLinearInterpolate, -550, 350)
        _lier(nlisse, "", nmix, "A")
        _lier(nhaut, "", nmix, "B")
        _lier(p_fac, "", nmix, "Alpha")
        nfin = _expr(mat, unreal.MaterialExpressionNormalize, -380, 350)
        _lier(nmix, "", nfin, "")
        MEL.connect_material_property(nfin, "", unreal.MaterialProperty.MP_NORMAL)

        # --- grandes taches d'herbe claire (bruit tres basse frequence)
        p_ech = _scalaire(mat, "EchelleTaches", 1.0 / (TAILLE_TACHES_M * 100.0), -1650, -300)
        wp2 = _expr(mat, unreal.MaterialExpressionWorldPosition, -1650, -420)
        pos = _expr(mat, unreal.MaterialExpressionMultiply, -1450, -360)
        _lier(wp2, "", pos, "A")
        _lier(p_ech, "", pos, "B")
        bruit = _expr(mat, unreal.MaterialExpressionNoise, -1250, -360)
        for k, v in (("scale", 1.0), ("levels", 2), ("output_min", 0.0), ("output_max", 1.0), ("quality", 1)):
            try:
                bruit.set_editor_property(k, v)
            except Exception:
                pass
        _lier(pos, "", bruit, "Position")
        c_fonce = _couleur(mat, "HerbeFoncee", HERBE_FONCEE, -1050, -560)
        c_clair = _couleur(mat, "HerbeClaire", HERBE_CLAIRE, -1050, -420)
        herbe = _expr(mat, unreal.MaterialExpressionLinearInterpolate, -800, -460)
        _lier(c_fonce, "", herbe, "A")
        _lier(c_clair, "", herbe, "B")
        _lier(bruit, "", herbe, "Alpha")

        # --- terre dans les pentes : alpha = sature((n.z - seuil) * durete)
        nz2 = _expr(mat, unreal.MaterialExpressionComponentMask, -380, 100)
        nz2.set_editor_property("r", False); nz2.set_editor_property("g", False)
        nz2.set_editor_property("b", True); nz2.set_editor_property("a", False)
        _lier(nfin, "", nz2, "")
        p_seuil = _scalaire(mat, "SeuilPente", 0.82, -380, 200)
        moins = _expr(mat, unreal.MaterialExpressionSubtract, -200, 120)
        _lier(nz2, "", moins, "A")
        _lier(p_seuil, "", moins, "B")
        p_dur = _scalaire(mat, "DuretePente", 10.0, -200, 240)
        fois = _expr(mat, unreal.MaterialExpressionMultiply, -50, 140)
        _lier(moins, "", fois, "A")
        _lier(p_dur, "", fois, "B")
        sat = _expr(mat, unreal.MaterialExpressionSaturate, 80, 140)
        _lier(fois, "", sat, "")
        c_terre = _couleur(mat, "TerrePente", TERRE_PENTE, -380, -200)
        couleur = _expr(mat, unreal.MaterialExpressionLinearInterpolate, 250, -200)
        _lier(c_terre, "", couleur, "A")
        _lier(herbe, "", couleur, "B")
        _lier(sat, "", couleur, "Alpha")
        MEL.connect_material_property(couleur, "", unreal.MaterialProperty.MP_BASE_COLOR)

        # --- peu de reflets (rendu "peint")
        MEL.connect_material_property(_scalaire(mat, "Rugosite", 0.9, 250, 50), "", unreal.MaterialProperty.MP_ROUGHNESS)
        MEL.connect_material_property(_scalaire(mat, "Speculaire", 0.15, 250, 150), "", unreal.MaterialProperty.MP_SPECULAR)

        MEL.recompile_material(mat)
        EAL.save_loaded_asset(mat, False)
        log("Materiau cree : %s" % ch_mat)

    if EAL.does_asset_exist(ch_mi):
        mi = EAL.load_asset(ch_mi)
    else:
        mi = tools.create_asset(nom_mi, DOSSIER_ASSETS, unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, mat)
        EAL.save_loaded_asset(mi, False)
        log("Instance creee : %s (pour regler les couleurs)" % ch_mi)
    return mi


# ----------------------------------------------------------------------------------------------- actions
def sauvegarder_umap():
    src = os.path.join(PROJET, "Content", *CARTE.replace("/Game/", "").split("/")) + ".umap"
    dst_dir = os.path.join(DOSSIER_OUTIL, "sauvegarde")
    dst = os.path.join(dst_dir, os.path.basename(src))
    if os.path.exists(src) and not os.path.exists(dst):
        os.makedirs(dst_dir, exist_ok=True)
        shutil.copy2(src, dst)
        log("Copie de securite : %s" % dst)


def appliquer_style(journal, landscapes, tuiles):
    mi = construire_materiau()
    for l in landscapes:
        k = chemin(l)
        if k not in journal["landscapes"]:
            journal["landscapes"][k] = {}
        jl = journal["landscapes"][k]
        if "landscape_material" not in jl:
            jl["landscape_material"] = chemin(l.get_editor_property("landscape_material"))
        l.set_editor_property("landscape_material", mi)
        log("  Landscape %s : materiau -> MI_SolCartoon" % l.get_actor_label())
    for a in tuiles:
        comp = a.static_mesh_component
        k = chemin(a)
        if k not in journal["tuiles_sol"]:
            journal["tuiles_sol"][k] = [chemin(m) for m in (comp.get_editor_property("override_materials") or [])]
        n = max(1, comp.get_num_materials())
        for i in range(n):
            comp.set_material(i, mi)
        log("  %s : materiau -> MI_SolCartoon (%d emplacement(s))" % (a.get_actor_label(), n))


def appliquer_landscape_leger(journal, landscapes):
    cibles = (("lod0_screen_size", LANDSCAPE_LOD0_SCREEN_SIZE, max),
              ("lod0_distribution_setting", LANDSCAPE_LOD0_DISTRIBUTION, min),
              ("lod_distribution_setting", LANDSCAPE_LOD_DISTRIBUTION, min))
    for l in landscapes:
        jl = journal["landscapes"].setdefault(chemin(l), {})
        for prop, val, choix in cibles:
            try:
                avant = float(l.get_editor_property(prop))
            except Exception:
                continue
            jl.setdefault(prop, avant)
            l.set_editor_property(prop, float(choix(avant, val)))
        log("  Landscape %s : LOD plus agressifs" % l.get_actor_label())


def appliquer_foliage(journal, comps):
    n_petit = n_grand = 0
    for c in comps:
        h = hauteur_mesh(c.static_mesh)
        petit = h < HAUTEUR_PETITE_PLANTE_CM
        cible = CULL_PETITES_CM if petit else CULL_GRANDES_CM
        start = int(c.get_editor_property("instance_start_cull_distance"))
        end = int(c.get_editor_property("instance_end_cull_distance"))
        k = chemin(c)
        if k not in journal["foliage"]:
            journal["foliage"][k] = {"start": start, "end": end, "cast_shadow": bool(c.get_editor_property("cast_shadow"))}
        new_end = cible if end <= 0 else min(end, cible)
        new_start = min(start, int(new_end * 0.75)) if start > 0 else int(new_end * 0.75)
        c.modify()
        c.set_cull_distances(new_start, new_end)
        c.set_editor_property("instance_start_cull_distance", new_start)
        c.set_editor_property("instance_end_cull_distance", new_end)
        if petit and not OMBRES_PETITES:
            c.set_cast_shadow(False)
            n_petit += 1
        elif not petit:
            n_grand += 1
    log("  Foliage : %d groupe(s) de petites plantes (%.0f m, sans ombre), %d groupe(s) d'arbres (%.0f m)" % (
        n_petit, CULL_PETITES_CM / 100.0, n_grand, CULL_GRANDES_CM / 100.0))


def _pp_lire(v, prop):
    pp = v.get_editor_property(prop)
    try:
        return pp, pp.get_editor_property("default")
    except Exception:
        return None, pp


def _pp_ecrire(v, prop, valeur):
    pp = v.get_editor_property(prop)
    try:
        pp.set_editor_property("default", valeur)
        v.set_editor_property(prop, pp)
    except Exception:
        v.set_editor_property(prop, valeur)


def appliquer_herbe_auto(journal, chemins):
    faits = 0
    for ch in chemins:
        gt = EAL.load_asset(ch)
        if gt is None:
            continue
        vars_ = list(gt.get_editor_property("grass_varieties") or [])
        if ch not in journal["herbe_auto"]:
            orig = []
            for v in vars_:
                orig.append({"density": float(_pp_lire(v, "grass_density")[1]),
                             "start": int(_pp_lire(v, "start_cull_distance")[1]),
                             "end": int(_pp_lire(v, "end_cull_distance")[1]),
                             "shadow": bool(v.get_editor_property("cast_dynamic_shadow"))})
            journal["herbe_auto"][ch] = orig
        orig = journal["herbe_auto"][ch]
        for i, v in enumerate(vars_):
            o = orig[i] if i < len(orig) else None
            if o is None:
                continue
            _pp_ecrire(v, "grass_density", float(o["density"]) * FACTEUR_DENSITE_HERBE)
            end = CULL_HERBE_AUTO_CM if o["end"] <= 0 else min(o["end"], CULL_HERBE_AUTO_CM)
            start = min(o["start"], int(end * 0.75)) if o["start"] > 0 else int(end * 0.75)
            _pp_ecrire(v, "end_cull_distance", int(end))
            _pp_ecrire(v, "start_cull_distance", int(start))
            v.set_editor_property("cast_dynamic_shadow", False)
        gt.set_editor_property("grass_varieties", vars_)
        EAL.save_loaded_asset(gt, False)
        faits += 1
    if faits:
        try:
            unreal.SystemLibrary.execute_console_command(ELL.get_editor_world(), "grass.FlushCache")
        except Exception:
            pass
    log("  Herbe automatique : %d type(s) allege(s) (densite x%.1f, %.0f m, sans ombre)" % (
        faits, FACTEUR_DENSITE_HERBE, CULL_HERBE_AUTO_CM / 100.0))


# ----------------------------------------------------------------------------------------------- main
def main():
    monde = ELL.get_editor_world()
    nom = monde.get_name() if monde else "?"
    if nom != CARTE.split("/")[-1]:
        q = "La carte ouverte est '%s', pas 'Rachid'.\nContinuer quand meme sur cette carte ?" % nom
        if unreal.EditorDialog.show_message("Sol cartoon", q, unreal.AppMsgType.YES_NO) != unreal.AppReturnType.YES:
            log("Annule : mauvaise carte ouverte (%s)." % nom)
            return

    acteurs = tous_les_acteurs()
    landscapes = trouver_landscapes(acteurs)
    tuiles = trouver_tuiles_sol(acteurs)
    foliage = trouver_foliage(acteurs) if FAIRE_FOLIAGE_LEGER else []
    herbe = trouver_herbe_auto() if FAIRE_HERBE_AUTO_LEGERE else []

    log("Carte : %s" % nom)
    for l in landscapes:
        log("  Landscape %s : materiau=%s LOD0=%.2f/%.2f autres=%.2f" % (
            l.get_actor_label(), chemin(l.get_editor_property("landscape_material")),
            l.get_editor_property("lod0_screen_size"), l.get_editor_property("lod0_distribution_setting"),
            l.get_editor_property("lod_distribution_setting")))
    for a in tuiles:
        log("  Dalle de sol : %s" % a.get_actor_label())
    total_inst = 0
    for c in foliage:
        total_inst += c.get_instance_count()
        log("  Foliage %6d x %s (h=%.0f cm, disparait a %d cm, ombre=%s)" % (
            c.get_instance_count(), chemin(c.static_mesh), hauteur_mesh(c.static_mesh),
            c.get_editor_property("instance_end_cull_distance"), c.get_editor_property("cast_shadow")))
    for h in herbe:
        log("  Herbe automatique : %s" % h)

    resume = ("%d Landscape, %d dalle(s) de sol, %d groupe(s) de foliage (%d plantes), %d type(s) d'herbe auto." % (
        len(landscapes), len(tuiles), len(foliage), total_inst, len(herbe)))
    log(resume)
    if not (landscapes or tuiles or foliage or herbe):
        unreal.EditorDialog.show_message("Sol cartoon", "Rien trouve a modifier dans cette carte.", unreal.AppMsgType.OK)
        return

    q = ("Trouve : " + resume + "\n\nAppliquer ?\n"
         + ("- sol style cartoon low poly (vert vif, facettes)\n" if FAIRE_STYLE_CARTOON else "")
         + ("- Landscape simplifie plus tot au loin\n" if FAIRE_LANDSCAPE_LEGER else "")
         + ("- herbe/plantes masquees au-dela de %.0f m, sans ombre ; arbres au-dela de %.0f m\n"
            % (CULL_PETITES_CM / 100.0, CULL_GRANDES_CM / 100.0) if FAIRE_FOLIAGE_LEGER else "")
         + ("- herbe automatique 2 fois moins dense\n" if FAIRE_HERBE_AUTO_LEGERE else "")
         + "\nAucune plante n'est effacee. retirer_sol_cartoon.py remet tout comme avant.\n"
         + "Detail : Window > Developer Tools > Output Log, ou Tools/out/sol_cartoon.log")
    if unreal.EditorDialog.show_message("Sol cartoon + allegement", q, unreal.AppMsgType.YES_NO) != unreal.AppReturnType.YES:
        log("Annule par l'utilisateur : rien n'a ete modifie.")
        return

    sauvegarder_umap()
    journal = charger_journal()
    journal["carte"] = nom
    journal["date"] = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    try:
        if FAIRE_STYLE_CARTOON:
            appliquer_style(journal, landscapes, tuiles)
        if FAIRE_LANDSCAPE_LEGER:
            appliquer_landscape_leger(journal, landscapes)
        if FAIRE_FOLIAGE_LEGER:
            appliquer_foliage(journal, foliage)
        if FAIRE_HERBE_AUTO_LEGERE:
            appliquer_herbe_auto(journal, herbe)
    finally:
        ecrire_journal(journal)      # meme en cas d'erreur, on garde de quoi revenir en arriere
    ELL.save_all_dirty_levels()
    log("Termine et enregistre.")
    unreal.EditorDialog.show_message(
        "Sol cartoon", "C'est fait !\nLance Play pour voir le resultat.\n\n"
        "Couleurs : Content Browser > Chelles > SolCartoon > MI_SolCartoon.\n"
        "Pour revenir en arriere : retirer_sol_cartoon.py", unreal.AppMsgType.OK)


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())
    unreal.log_error(traceback.format_exc())
    try:
        unreal.EditorDialog.show_message("Sol cartoon", "Erreur : voir Tools/out/sol_cartoon.log", unreal.AppMsgType.OK)
    except Exception:
        pass
finally:
    ecrire_log()
