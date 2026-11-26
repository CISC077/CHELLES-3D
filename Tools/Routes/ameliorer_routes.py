r"""ROUTES + FLEURS (26/11/2026, agent Terrain) - carte Rachid, Unreal 4.27.
Lance par Tools\Routes\AMELIORER_ROUTES.bat (Unreal ferme) ou dans l editeur : File > Execute Python Script.
 1. LISTE des fleurs de la carte (Tools\Routes\fleurs_a_supprimer.txt) : RIEN n est supprime ici.
 2. Sauvegarde de Rachid.umap dans Tools\Routes\Sauvegardes\asphalte_<date>\.
 3. Croisements, ronds-points et raccords : la plaque et tous les bouts de route qui la touchent a la MEME hauteur
    (raccord en douceur sur 15 m, la route ne descend jamais sous le sol).
 4. Toutes les chaussees, ronds-points et plaques de croisement en MI_AsphaltRoad (demande de Rachid).
Journal : Tools\Routes\ameliorer_routes.log
"""
# >>> SAUVEGARDE AUTO CHELLES 3D (pose par Tools\Sauvegarde\brancher_sauvegarde_auto.py, ne pas modifier)
try:
    import os as _sa_os, sys as _sa_sys
    _sa_d = _sa_os.path.dirname(_sa_os.path.abspath(globals().get('__file__') or '.'))
    _sa_m = None
    while _sa_d:
        if _sa_os.path.isfile(_sa_os.path.join(_sa_d, 'Tools', 'Sauvegarde', 'sauvegarde_auto.py')):
            _sa_m = _sa_os.path.join(_sa_d, 'Tools', 'Sauvegarde')
            break
        _sa_p = _sa_os.path.dirname(_sa_d)
        _sa_d = None if _sa_p == _sa_d else _sa_p
    if _sa_m is None:
        import unreal as _sa_u
        _sa_m = _sa_os.path.join(_sa_u.Paths.convert_relative_path_to_full(_sa_u.Paths.project_dir()), 'Tools', 'Sauvegarde')
    if _sa_m not in _sa_sys.path:
        _sa_sys.path.insert(0, _sa_m)
    import sauvegarde_auto as _sa_mod
    _sa_mod.avant(globals().get('__file__') or 'script')
except Exception as _sa_e:
    print('[SauvegardeAuto] non faite : %s' % _sa_e)
# <<< SAUVEGARDE AUTO CHELLES 3D
import datetime, json, math, os, re, shutil, traceback
import unreal

# ----------------------------------------------------------------------------------------------- reglages
CARTE = "/Game/Maps/Rachid"
NOM_ASPHALTE = "MI_AsphaltRoad"          # demande de Rachid (26/11) : routes, ronds-points et croisements
TAG = "ChellesRoutePRG"
RACCORD_M = 15.0                         # longueur sur laquelle le bout d'une route rejoint la hauteur du carrefour
CONTACT_M = 4.0                          # un bout de route a moins de (rayon plaque + 4 m) touche le carrefour
CONTACT_BOUCLE_M = 9.0                   # un bout de route a moins de 9 m d'un rond-point le touche
MOTS_CHAUSSEE = ("road", "asphalt", "street", "route", "tarmac", "granular", "intersection", "lane")
MOTS_PAS_CHAUSSEE = ("side", "curb", "kerb", "walk", "trott", "line", "mark", "lamp", "light", "pole", "grass",
                     "border", "dirt", "concrete_tile", "paving", "brick")
NOMS_VAR_MATERIAU = ("Road Material", "RoadMaterial", "Road_Material", "Asphalt Material", "AsphaltMaterial",
                     "Material Road", "Road Mat", "RoadMat", "Material")
# fleurs : mots du nom du modele / des materiaux / des textures
MOTS_FLEUR = ("flower", "fleur", "daisy", "dandelion", "tulip", "sunflower", "poppy", "lily", "marigold", "buttercup",
              "daffodil", "bloom", "blossom", "petal", "camomile", "chamomile", "lavender", "rose_", "_rose", "wildflower")
MOTS_JAUNE = ("yellow", "jaune", "gold", "dandelion", "sunflower", "buttercup", "daffodil", "marigold", "camomile", "chamomile")

PROJ = unreal.SystemLibrary.get_project_directory()
ICI = os.path.join(PROJ, "Tools", "Routes")
JOURNAL = os.path.join(ICI, "ameliorer_routes.log")
LISTE_FLEURS = os.path.join(ICI, "fleurs_a_supprimer.txt")
ETAT = os.path.join(ICI, "ameliorer_routes_avant.json")


def log(msg):
    unreal.log("[Routes+] " + str(msg))
    with open(JOURNAL, "a", encoding="utf-8") as f:
        f.write(str(msg) + "\n")


def tags(a):
    return [str(t) for t in a.tags]


def _set_var(a, noms, valeur):
    for n in noms:
        try:
            a.set_editor_property(n, valeur)
            return n
        except Exception:
            continue
    return None


def _get_var(a, noms):
    for n in noms:
        try:
            return a.get_editor_property(n), n
        except Exception:
            continue
    return None, None


def relancer(a):
    a.set_editor_property("tags", [t for t in a.tags])     # relance le Construction Script du pack


def chercher_asphalte():
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    f = unreal.ARFilter(class_names=["MaterialInstanceConstant", "Material"], package_paths=["/Game"], recursive_paths=True)
    trouves = [d for d in ar.get_assets(f) if str(d.asset_name).lower() == NOM_ASPHALTE.lower()]
    if not trouves:
        return None, []
    chemins = sorted(str(d.object_path) for d in trouves)
    return unreal.load_asset(chemins[0]), chemins


def decrire_materiau(m):
    """noms des parametres + textures + couleurs (pour le journal et pour reconnaitre le jaune)"""
    infos = {"nom": m.get_path_name() if m else "", "scalaires": {}, "couleurs": {}, "textures": []}
    if m is None:
        return infos
    mel = unreal.MaterialEditingLibrary
    try:
        for n in mel.get_scalar_parameter_names(m):
            try:
                infos["scalaires"][str(n)] = round(mel.get_material_instance_scalar_parameter_value(m, n), 3)
            except Exception:
                infos["scalaires"][str(n)] = None
    except Exception:
        pass
    try:
        for n in mel.get_vector_parameter_names(m):
            try:
                c = mel.get_material_instance_vector_parameter_value(m, n)
                infos["couleurs"][str(n)] = [round(c.r, 2), round(c.g, 2), round(c.b, 2)]
            except Exception:
                pass
    except Exception:
        pass
    try:
        for n in mel.get_texture_parameter_names(m):
            try:
                t = mel.get_material_instance_texture_parameter_value(m, n)
                if t:
                    infos["textures"].append(t.get_name())
            except Exception:
                pass
    except Exception:
        pass
    return infos


def est_chaussee(mat):
    if mat is None:
        return False
    n = mat.get_path_name().lower()
    base = mat.get_name().lower()
    if base == NOM_ASPHALTE.lower():
        return False
    if any(m in base for m in MOTS_PAS_CHAUSSEE):
        return False
    return any(m in base for m in MOTS_CHAUSSEE) or "/advancedroads/" in n


# =============================================================================================== 1. hauteurs
def points_monde(sp):
    return [sp.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.WORLD) for i in range(sp.get_number_of_spline_points())]


def distances(pts):
    d = [0.0]
    for i in range(1, len(pts)):
        d.append(d[-1] + math.hypot(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y))
    return d


def remonter_bout(pts, debut, z_cible):
    """le bout de la route (debut=True : premier point) arrive exactement a z_cible ;
    raccord progressif sur RACCORD_M, sans jamais descendre la route (elle ne s'enfonce pas)"""
    d = distances(pts)
    L = d[-1]
    for i, p in enumerate(pts):
        s = d[i] if debut else L - d[i]
        if s > RACCORD_M * 100.0:
            continue
        w = 1.0 - s / (RACCORD_M * 100.0)
        w = w * w * (3 - 2 * w)                      # raccord en douceur
        pts[i] = unreal.Vector(p.x, p.y, max(p.z, p.z + (z_cible - p.z) * w) if z_cible >= p.z else p.z)
    i = 0 if debut else len(pts) - 1
    pts[i] = unreal.Vector(pts[i].x, pts[i].y, max(pts[i].z, z_cible))
    return pts


def uniformiser_hauteurs(routes, plaques, avant):
    infos = {}
    for a in routes:
        sp = a.get_component_by_class(unreal.SplineComponent)
        infos[a] = {"sp": sp, "pts": points_monde(sp), "ferme": sp.is_closed_loop()}
        avant["routes"][a.get_actor_label()] = [[p.x, p.y, p.z] for p in infos[a]["pts"]]
    ouvertes = [a for a in routes if not infos[a]["ferme"]]
    boucles = [a for a in routes if infos[a]["ferme"]]
    nb_car, ecarts = 0, []
    # ---- carrefours (plaques) : une seule hauteur pour la plaque et tous les bouts de route qui la touchent
    for pl in plaques:
        l = pl.get_actor_location()
        o, e = pl.get_actor_bounds(False)
        rayon = max(e.x, e.y)
        dessus = o.z + e.z
        bouts = []
        for a in ouvertes:
            pts = infos[a]["pts"]
            for debut, p in ((True, pts[0]), (False, pts[-1])):
                if math.hypot(p.x - l.x, p.y - l.y) < rayon + CONTACT_M * 100.0:
                    bouts.append((a, debut, p.z))
        if not bouts:
            continue
        zs = [z for _, _, z in bouts] + [dessus]
        zj = max(zs)
        ecarts.append(max(zs) - min(zs))
        avant["acteurs"][pl.get_actor_label()] = [l.x, l.y, l.z]
        pl.set_actor_location(unreal.Vector(l.x, l.y, l.z + (zj - dessus)), False, False)
        for a, debut, _ in bouts:
            infos[a]["pts"] = remonter_bout(infos[a]["pts"], debut, zj)
            infos[a]["modif"] = True
        nb_car += 1
    # ---- ronds-points (boucles) : les routes qui y arrivent sont a la hauteur de l'anneau, et inversement
    nb_rp = 0
    for b in boucles:
        bpts = infos[b]["pts"]
        for a in ouvertes:
            pts = infos[a]["pts"]
            for debut in (True, False):
                p = pts[0] if debut else pts[-1]
                k = min(range(len(bpts)), key=lambda j: math.hypot(bpts[j].x - p.x, bpts[j].y - p.y))
                if math.hypot(bpts[k].x - p.x, bpts[k].y - p.y) > CONTACT_BOUCLE_M * 100.0:
                    continue
                zj = max(p.z, bpts[k].z)
                ecarts.append(abs(p.z - bpts[k].z))
                infos[a]["pts"] = remonter_bout(pts, debut, zj)
                pts = infos[a]["pts"]
                # l'anneau monte un peu autour du point de contact (jamais vers le bas)
                for j, q in enumerate(bpts):
                    dist = math.hypot(q.x - bpts[k].x, q.y - bpts[k].y)
                    if dist < RACCORD_M * 100.0 and zj > q.z:
                        w = 1.0 - dist / (RACCORD_M * 100.0)
                        bpts[j] = unreal.Vector(q.x, q.y, q.z + (zj - q.z) * w * w * (3 - 2 * w))
                infos[a]["modif"] = True
                infos[b]["modif"] = True
                nb_rp += 1
    # ---- bouts de route qui se touchent sans plaque (rue qui continue)
    nb_bb = 0
    for i, a in enumerate(ouvertes):
        for b in ouvertes[i + 1:]:
            for da in (True, False):
                for db in (True, False):
                    pa = infos[a]["pts"][0 if da else -1]
                    pb = infos[b]["pts"][0 if db else -1]
                    if math.hypot(pa.x - pb.x, pa.y - pb.y) < 300.0 and abs(pa.z - pb.z) > 1.0:
                        zj = max(pa.z, pb.z)
                        infos[a]["pts"] = remonter_bout(infos[a]["pts"], da, zj)
                        infos[b]["pts"] = remonter_bout(infos[b]["pts"], db, zj)
                        infos[a]["modif"] = infos[b]["modif"] = True
                        nb_bb += 1
    # ---- application
    nb_r = 0
    for a, inf in infos.items():
        if not inf.get("modif"):
            continue
        sp = inf["sp"]
        try:
            sp.set_editor_property("spline_has_been_edited", True)
        except Exception:
            pass
        sp.set_spline_points(inf["pts"], unreal.SplineCoordinateSpace.WORLD, True)
        if inf["ferme"]:
            sp.set_closed_loop(True, True)
        nb_r += 1
    return nb_car, nb_rp, nb_bb, nb_r, ecarts


# =============================================================================================== 2. asphalte
def poser_asphalte(routes, plaques, mi, avant):
    # a) variable "materiau de route" du blueprint du pack, si elle existe
    nom_var = None
    for a in routes:
        v, n = _get_var(a, NOMS_VAR_MATERIAU)
        if n and (v is None or isinstance(v, unreal.MaterialInterface)):
            avant["var"][a.get_actor_label()] = [n, v.get_path_name() if v else ""]
            if _set_var(a, (n,), mi):
                nom_var = n
    # b) relance de toutes les routes (hauteurs + variable), puis tous les morceaux de chaussee
    for a in routes:
        relancer(a)
    vus_mat, meshes = {}, {}
    nb_slots = 0
    for a in routes:
        for c in a.get_components_by_class(unreal.StaticMeshComponent):     # SplineMeshComponent en fait partie
            m = c.get_editor_property("static_mesh")
            for i in range(c.get_num_materials()):
                mat = c.get_material(i)
                nom = mat.get_path_name() if mat else "(aucun)"
                vus_mat[nom] = vus_mat.get(nom, 0) + 1
                if est_chaussee(mat):
                    c.set_material(i, mi)
                    nb_slots += 1
                    if m is not None:
                        meshes.setdefault(m.get_path_name(), (m, set()))[1].add(i)
    # c) le modele 3D lui-meme (pour que le pack garde l'asphalte s'il refait ses morceaux)
    nb_mesh = 0
    sauv = os.path.join(ICI, "Sauvegardes", "asphalte_modeles_" + datetime.datetime.now().strftime("%Y-%m-%d_%Hh%M"))
    contenu = unreal.SystemLibrary.get_project_content_directory()
    for chemin, (m, slots) in meshes.items():
        paquet = chemin.split(".")[0]
        if not paquet.startswith("/Game/"):
            continue
        fichier = os.path.join(contenu, *paquet[len("/Game/"):].split("/")) + ".uasset"
        try:
            if os.path.isfile(fichier):
                dest = os.path.join(sauv, *paquet[len("/Game/"):].split("/")) + ".uasset"
                os.makedirs(os.path.dirname(dest), exist_ok=True)
                shutil.copy2(fichier, dest)
            for i in slots:
                ancien = m.get_material(i)
                avant["modeles"].setdefault(chemin, {})[str(i)] = ancien.get_path_name() if ancien else ""
                m.set_material(i, mi)
            unreal.EditorAssetLibrary.save_loaded_asset(m)
            nb_mesh += 1
        except Exception as e:
            log("   modele %s : %s" % (chemin, e))
    # d) plaques des croisements et petits ronds-points
    nb_pl = 0
    for pl in plaques:
        c = pl.get_component_by_class(unreal.StaticMeshComponent)
        if not c:
            continue
        anciens = []
        for i in range(c.get_num_materials()):
            mat = c.get_material(i)
            anciens.append(mat.get_path_name() if mat else "")
            c.set_material(i, mi)
        avant["plaques"][pl.get_actor_label()] = anciens
        nb_pl += 1
    return nom_var, nb_slots, nb_mesh, nb_pl, vus_mat, sauv


# =============================================================================================== 3. fleurs (LISTE seulement)
def mots(s):
    return s.lower()


def infos_fleur(mesh):
    """(est une fleur, jaune ?, indices) d'apres le nom du modele, de ses materiaux, textures et couleurs"""
    nom = mots(mesh.get_path_name())
    textes = [nom]
    couleurs = []
    try:
        for i in range(len(mesh.get_editor_property("static_materials"))):
            mat = mesh.get_material(i)
            if mat:
                d = decrire_materiau(mat)
                textes.append(mots(d["nom"]))
                textes += [mots(t) for t in d["textures"]]
                couleurs += list(d["couleurs"].values())
    except Exception:
        pass
    tout = " ".join(textes)
    fleur = any(m in tout for m in MOTS_FLEUR)
    indices = [m for m in MOTS_JAUNE if m in tout]
    for r, g, b in couleurs:
        if r > 0.55 and g > 0.45 and b < 0.35 and r - b > 0.35:
            indices.append("couleur jaune (%.2f %.2f %.2f)" % (r, g, b))
            break
    return fleur, bool(indices), indices


def inventaire_fleurs(acteurs):
    groupes = {}
    for a in acteurs:
        lab = a.get_actor_label()
        if isinstance(a, unreal.InstancedFoliageActor):
            ou = "foliage peint"
        elif "ChellesVegEau" in tags(a) or lab.startswith("ChellesVeg_"):
            ou = "vegetation OSM (ChellesVegetation)"
        else:
            ou = "objet pose"
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            m = c.get_editor_property("static_mesh")
            if m is None:
                continue
            n = c.get_instance_count() if isinstance(c, unreal.InstancedStaticMeshComponent) else 1
            if n <= 0:
                continue
            cle = m.get_path_name()
            g = groupes.setdefault(cle, {"mesh": m, "n": 0, "ou": set(), "acteurs": set(), "cache": 0})
            g["n"] += n
            g["ou"].add(ou)
            if len(g["acteurs"]) < 6:
                g["acteurs"].add(lab)
            if a.is_hidden_ed() or not c.is_visible():
                g["cache"] += n
    fleurs = []
    for cle, g in groupes.items():
        est, jaune, indices = infos_fleur(g["mesh"])
        if est:
            pack = cle.split("/")[2] if cle.startswith("/Game/") else cle.split("/")[1]
            fleurs.append((not jaune, pack, cle, g, jaune, indices))
    # herbe automatique du sol (LandscapeGrassType) : on ne compte pas les brins, on signale le modele
    try:
        ar = unreal.AssetRegistryHelpers.get_asset_registry()
        for dg in ar.get_assets(unreal.ARFilter(class_names=["LandscapeGrassType"], package_paths=["/Game"], recursive_paths=True)):
            gt = unreal.load_asset(str(dg.object_path))
            for v in gt.get_editor_property("grass_varieties"):
                m = v.get_editor_property("grass_mesh")
                if m is None or m.get_path_name() in groupes:
                    continue
                est, jaune, indices = infos_fleur(m)
                if est:
                    cle = m.get_path_name()
                    pack = cle.split("/")[2] if cle.startswith("/Game/") else "?"
                    fleurs.append((not jaune, pack, cle, {"n": 0, "ou": {"herbe auto du sol : " + gt.get_name()}, "acteurs": set(), "cache": 0},
                                   jaune, indices))
    except Exception as e:
        log("    (herbe du sol non lue : %s)" % e)
    fleurs.sort(key=lambda f: (f[0], f[1], f[2]))
    with open(LISTE_FLEURS, "w", encoding="utf-8") as h:
        h.write("FLEURS TROUVEES DANS RACHID (%s)\n" % datetime.datetime.now().strftime("%d/%m/%Y %H:%M"))
        h.write("Rien n'a ete supprime. Ligne qui commence par [X] = sera supprimee a l'etape 2.\n")
        h.write("Pour changer : mettre [X] ou [ ] au debut de la ligne, enregistrer, puis lancer l'etape 2.\n\n")
        for _, pack, cle, g, jaune, indices in fleurs:
            h.write("%s %-8s %6d  pack %-28s %s\n" % ("[X]" if jaune else "[ ]", "JAUNE" if jaune else "", g["n"], pack, cle))
            h.write("        ou : %s%s ; ex. : %s%s\n" % (", ".join(sorted(g["ou"])),
                    (" (%d cachees)" % g["cache"]) if g["cache"] else "", ", ".join(sorted(g["acteurs"]))[:160],
                    ("  ; jaune car : " + ", ".join(indices)) if indices else ""))
    return fleurs


# =============================================================================================== principal
def main():
    os.makedirs(ICI, exist_ok=True)
    open(JOURNAL, "w", encoding="utf-8").write("")
    ell = unreal.EditorLevelLibrary
    world = ell.get_editor_world()
    if world is None or world.get_name() != "Rachid":
        ell.load_level(CARTE)
        world = ell.get_editor_world()
    log("Niveau ouvert : %s   (%s)" % (world.get_path_name(), datetime.datetime.now().strftime("%d/%m/%Y %H:%M")))
    acteurs = ell.get_all_level_actors()
    routes = [a for a in acteurs if TAG in tags(a) and a.get_component_by_class(unreal.SplineComponent)]
    plaques = [a for a in acteurs if TAG in tags(a) and not a.get_component_by_class(unreal.SplineComponent)
               and a.get_component_by_class(unreal.StaticMeshComponent) and "ChellesRouteCollision" not in tags(a)]
    log("%d route(s) du pack (dont %d ronds-points en boucle), %d plaque(s) de croisement" % (
        len(routes), sum(1 for a in routes if a.get_component_by_class(unreal.SplineComponent).is_closed_loop()), len(plaques)))

    # ---- 0. fleurs : liste seulement (rien n'est supprime ici)
    fleurs = inventaire_fleurs(acteurs)
    nj = [f for f in fleurs if f[4]]
    log("0/4 fleurs : %d modele(s) de fleur dans la carte, %d juge(s) JAUNE(S) (%d plantes). Liste : Tools\\Routes\\fleurs_a_supprimer.txt"
        % (len(fleurs), len(nj), sum(f[3]["n"] for f in nj)))
    for _, pack, cle, g, jaune, indices in fleurs[:40]:
        log("    %s %6d  %s" % ("JAUNE" if jaune else "     ", g["n"], cle))

    if not routes:
        log("ABANDON routes : aucune route du pack (tag %s). Carte non modifiee." % TAG)
        return
    mi, tous = chercher_asphalte()
    if mi is None:
        log("ABANDON routes : materiau %s introuvable dans Content. Carte non modifiee." % NOM_ASPHALTE)
        return
    log("    asphalte : %s%s" % (mi.get_path_name(), ("  (autres : %s)" % ", ".join(tous[1:])) if len(tous) > 1 else ""))
    d = decrire_materiau(mi)
    log("    reglages de %s : %s ; textures %s" % (NOM_ASPHALTE, json.dumps(d["scalaires"]), ", ".join(d["textures"])))

    # ---- 1. sauvegarde
    pkg = world.get_outermost().get_name()
    rel = pkg[len("/Game/"):]
    dest = os.path.join(ICI, "Sauvegardes", "asphalte_" + datetime.datetime.now().strftime("%Y-%m-%d_%Hh%M"))
    base = os.path.join(unreal.SystemLibrary.get_project_content_directory(), *rel.split("/"))
    os.makedirs(dest, exist_ok=True)
    copies = 0
    for suffixe in (".umap", "_BuiltData.uasset"):
        if os.path.exists(base + suffixe):
            shutil.copy2(base + suffixe, dest)
            copies += 1
    if not copies:
        log("ABANDON : impossible de copier Rachid.umap. Carte non modifiee.")
        return
    log("1/4 sauvegarde de la carte : %s" % dest)
    avant = {"date": datetime.datetime.now().isoformat(), "routes": {}, "acteurs": {}, "var": {}, "modeles": {}, "plaques": {}}

    # ---- 2. meme hauteur aux croisements / ronds-points
    nb_car, nb_rp, nb_bb, nb_r, ecarts = uniformiser_hauteurs(routes, plaques, avant)
    log("2/4 hauteurs : %d croisement(s), %d arrivee(s) de rond-point, %d raccord(s) de rue ; %d route(s) retouchee(s)"
        % (nb_car, nb_rp, nb_bb, nb_r))
    if ecarts:
        log("    marche corrigee aux croisements : jusqu'a %.0f cm (moyenne %.0f cm)" % (max(ecarts), sum(ecarts) / len(ecarts)))

    # ---- 3. asphalte partout
    nom_var, nb_slots, nb_mesh, nb_pl, vus_mat, sauv = poser_asphalte(routes, plaques, mi, avant)
    log("3/4 asphalte %s : variable du pack %s ; %d morceau(x) de chaussee, %d modele(s) 3D du pack, %d plaque(s)"
        % (NOM_ASPHALTE, ("'%s'" % nom_var) if nom_var else "absente", nb_slots, nb_mesh, nb_pl))
    if nb_mesh:
        log("    copie des modeles avant changement : %s" % sauv)
    log("    materiaux vus sur les routes (avant) :")
    for nom, n in sorted(vus_mat.items(), key=lambda kv: -kv[1])[:25]:
        log("      %5d  %s%s" % (n, nom, "   -> asphalte" if est_chaussee(unreal.load_asset(nom) if nom.startswith("/") else None) else ""))
    json.dump(avant, open(ETAT, "w", encoding="utf-8"))

    # ---- 4. enregistrement
    if ell.save_current_level():
        log("4/4 TERMINE : carte Rachid enregistree. Retour arriere : copier les fichiers de %s dans Content\\Maps" % dest)
    else:
        log("4/4 ATTENTION : la carte n'a pas pu etre enregistree (Ctrl+S dans Unreal).")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)


def _quitter_si_bat():
    if os.environ.get("CHELLES_QUITTER") == "1":
        try:
            unreal.SystemLibrary.quit_editor()
        except Exception:
            pass


try:
    main()
except Exception:
    try:
        log("ERREUR :\n" + traceback.format_exc())
    except Exception:
        unreal.log_error(traceback.format_exc())
_quitter_si_bat()
