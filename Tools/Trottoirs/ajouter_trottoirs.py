"""SCRIPT A LANCER DANS UNREAL 4.27 : File > Execute Python Script... > ce fichier.

Ajoute des TROTTOIRS le long des routes OpenStreetMap du quartier (les rubans "Chelles_Tile_Road_*") :
  - routes normales : un trottoir de chaque cote ;
  - ronds-points    : un trottoir seulement sur le bord EXTERIEUR (rien sur l'ilot central) ;
  - aux carrefours, le trottoir s'arrete au bord de la route qui croise (passage libre) ;
  - materiau : "Road_Inst" (cherche automatiquement dans tout le Content).

Ce que le script fait, dans l'ordre :
  1. copie de sauvegarde du niveau ouvert dans Saved/Sauvegardes/trottoirs_<date>/ ;
  2. calcule les trottoirs depuis Tools/out/osm_ways.json (les memes rues que les routes 3D et la circulation) ;
  3. importe 16 morceaux (un par tuile) dans /Game/Chelles/Trottoirs ;
  4. les pose EXACTEMENT au meme endroit que les routes (meme position / rotation que Chelles_Tile_Road_*),
     verifie la position, puis enregistre le niveau.

Ne touche a RIEN d'autre : routes, batiments, Landscape, circulation et passants restent tels quels.
Peut etre relance (il remplace ses propres trottoirs). Pour les enlever : retirer_trottoirs.py.
Journal : Tools/out/ajouter_trottoirs.log
"""
import datetime, json, math, os, shutil, traceback

# ----------------------------------------------------------------------------------------------- reglages
LARGEUR_TROTTOIR = 2.0     # m, largeur du trottoir
HAUTEUR_TROTTOIR = 0.15    # m, hauteur de la bordure au-dessus de la route
PROFONDEUR = 0.12          # m, le trottoir s'enfonce un peu sous la route pour ne pas "flotter"
UV_METRES = 2.0            # m, taille d'une repetition de la texture Road_Inst
NOM_MATERIAU = "Road_Inst"

# memes reglages que Tools/build_buildings.py et Tools/build_roads.py (ne pas changer)
CENTER = (48.8795, 2.5912)
SIZE_M, TILES = 1000.0, 4
WIDTH = {"secondary": 9.0, "tertiary": 8.0, "residential": 6.0, "unclassified": 5.5, "living_street": 5.0}
ROAD_Z = 0.03              # m, hauteur des rubans de route

PREFIXE = "Chelles_Trottoir_"
DOSSIER_ASSETS = "/Game/Chelles/Trottoirs"
DOSSIER_NIVEAU = "Chelles/Trottoirs"

lat0, lon0 = CENTER
M_LAT = 111320.0
M_LON = 111320.0 * math.cos(math.radians(lat0))
HALF = SIZE_M / 2


def to_xy(lat, lon):
    return ((lon - lon0) * M_LON, (lat - lat0) * M_LAT)


# =============================================================================================== geometrie
# Tout est en metres, repere des fichiers FBX (x = est, y = nord), comme build_roads.py.

def _dist_seg(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    L2 = dx * dx + dy * dy
    t = 0.0 if L2 < 1e-12 else max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / L2))
    return math.hypot(p[0] - a[0] - t * dx, p[1] - a[1] - t * dy)


def _cercle(points):
    """centre du cercle le plus proche des points (moindres carres) ; repli : moyenne"""
    n = len(points)
    mx = sum(p[0] for p in points) / n
    my = sum(p[1] for p in points) / n
    suu = suv = svv = suuu = svvv = suvv = svuu = 0.0
    for x, y in points:
        u, v = x - mx, y - my
        suu += u * u; svv += v * v; suv += u * v
        suuu += u * u * u; svvv += v * v * v; suvv += u * v * v; svuu += v * u * u
    det = suu * svv - suv * suv
    if n < 3 or abs(det) < 1e-9:
        return (mx, my)
    b1 = 0.5 * (suuu + suvv)
    b2 = 0.5 * (svvv + svuu)
    uc = (b1 * svv - b2 * suv) / det
    vc = (suu * b2 - suv * b1) / det
    return (mx + uc, my + vc)


def lire_routes(chemin):
    """-> liste de rues : {"id", "pts" (m), "hw" (demi-largeur), "rond": bool, "ferme": bool}"""
    data = json.load(open(chemin, encoding="utf-8"))
    rues = []
    for el in data.get("elements", []):
        t = el.get("tags", {})
        hw = t.get("highway")
        if hw not in WIDTH or "geometry" not in el:
            continue
        brut = [to_xy(q["lat"], q["lon"]) for q in el["geometry"]]
        pts = []
        for p in brut:
            if not pts or math.hypot(p[0] - pts[-1][0], p[1] - pts[-1][1]) >= 0.3:
                pts.append(p)
        if len(pts) < 2:
            continue
        ferme = len(pts) > 3 and math.hypot(pts[0][0] - pts[-1][0], pts[0][1] - pts[-1][1]) < 0.5
        if ferme:
            pts = pts[:-1]
        rues.append({"id": el.get("id"), "pts": pts, "hw": WIDTH[hw] / 2.0, "ferme": ferme,
                     "rond": t.get("junction") in ("roundabout", "circular")})
    return rues


def centres_ronds_points(rues):
    """regroupe les morceaux de ronds-points qui se touchent et calcule le centre de chacun"""
    ronds = [r for r in rues if r["rond"]]
    groupe = list(range(len(ronds)))

    def racine(i):
        while groupe[i] != i:
            groupe[i] = groupe[groupe[i]]
            i = groupe[i]
        return i

    for i in range(len(ronds)):
        for j in range(i + 1, len(ronds)):
            if any(math.hypot(a[0] - b[0], a[1] - b[1]) < 1.0 for a in ronds[i]["pts"] for b in ronds[j]["pts"]):
                groupe[racine(i)] = racine(j)
    paquets = {}
    for i, r in enumerate(ronds):
        paquets.setdefault(racine(i), []).append(r)
    for paquet in paquets.values():
        pts = []
        for r in paquet:
            for p in r["pts"]:
                if all(math.hypot(p[0] - q[0], p[1] - q[1]) > 0.5 for q in pts):
                    pts.append(p)
        c = _cercle(pts)
        for r in paquet:
            r["centre"] = c
    return len(paquets)


class Grille:
    """index spatial des troncons de route, pour savoir vite si un point est SUR une chaussee"""
    def __init__(self, rues, cellule=20.0):
        self.c = cellule
        self.cases = {}
        for ri, r in enumerate(rues):
            p = r["pts"]
            n = len(p) if r["ferme"] else len(p) - 1
            for j in range(n):
                a, b = p[j], p[(j + 1) % len(p)]
                if abs(a[0] + b[0]) / 2 > HALF + 20 or abs(a[1] + b[1]) / 2 > HALF + 20:
                    continue                 # hors de la zone : build_roads.py ne l'a pas construit
                m = r["hw"] + 1.0
                for cx in range(int(math.floor((min(a[0], b[0]) - m) / cellule)), int(math.floor((max(a[0], b[0]) + m) / cellule)) + 1):
                    for cy in range(int(math.floor((min(a[1], b[1]) - m) / cellule)), int(math.floor((max(a[1], b[1]) + m) / cellule)) + 1):
                        self.cases.setdefault((cx, cy), []).append((a, b, r["hw"], ri, j))

    def sur_la_route(self, p, sauf_rue, sauf_troncon):
        for a, b, hw, ri, j in self.cases.get((int(math.floor(p[0] / self.c)), int(math.floor(p[1] / self.c))), ()):
            if ri == sauf_rue and j == sauf_troncon:
                continue
            if _dist_seg(p, a, b) < hw + 0.02:
                return True
        return False


def _normale_gauche(a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    L = math.hypot(dx, dy)
    return (-dy / L, dx / L), L


def bandes_de_trottoir(rues, grille):
    """-> liste de quadrilateres (inner_a, outer_a, outer_b, inner_b) en m : les morceaux de trottoir gardes"""
    quads = []
    for ri, r in enumerate(rues):
        p = r["pts"]
        nseg = len(p) if r["ferme"] else len(p) - 1
        segs = [(p[j], p[(j + 1) % len(p)]) for j in range(nseg)]
        normales = [_normale_gauche(a, b) for a, b in segs]
        # cotes a traiter : +1 = gauche du sens de dessin, -1 = droite
        if r["rond"]:
            cotes_par_seg = []
            for (a, b), (n, _) in zip(segs, normales):
                mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
                c = r["centre"]
                ext_gauche = n[0] * (mx - c[0]) + n[1] * (my - c[1]) > 0
                cotes_par_seg.append((1,) if ext_gauche else (-1,))
        else:
            cotes_par_seg = [(1, -1)] * nseg

        def onglet(j, fin):
            """decalage (vecteur) au debut (fin=False) ou a la fin (fin=True) du troncon j, raccorde au voisin"""
            n0 = normales[j][0]
            k = j + 1 if fin else j - 1
            if r["ferme"]:
                k %= nseg
            elif k < 0 or k >= nseg:
                return n0
            n1 = normales[k][0]
            d = 1.0 + n0[0] * n1[0] + n0[1] * n1[1]
            if d < 0.5:                      # virage tres serre : pas de raccord
                return n0
            return ((n0[0] + n1[0]) / d, (n0[1] + n1[1]) / d)

        hw = r["hw"]
        for j, ((a, b), (n, L)) in enumerate(zip(segs, normales)):
            if L < 0.3:
                continue
            ma, mb = onglet(j, False), onglet(j, True)
            for s in cotes_par_seg[j]:
                ia = (a[0] + s * ma[0] * hw, a[1] + s * ma[1] * hw)
                oa = (a[0] + s * ma[0] * (hw + LARGEUR_TROTTOIR), a[1] + s * ma[1] * (hw + LARGEUR_TROTTOIR))
                ib = (b[0] + s * mb[0] * hw, b[1] + s * mb[1] * hw)
                ob = (b[0] + s * mb[0] * (hw + LARGEUR_TROTTOIR), b[1] + s * mb[1] * (hw + LARGEUR_TROTTOIR))
                k = max(1, int(math.ceil(L / 1.0)))       # morceaux de 1 m environ
                garde = []
                for q in range(k):
                    t = (q + 0.5) / k
                    pa = (ia[0] + (ib[0] - ia[0]) * t, ia[1] + (ib[1] - ia[1]) * t)
                    po = (oa[0] + (ob[0] - oa[0]) * t, oa[1] + (ob[1] - oa[1]) * t)
                    tests = [(pa[0] + (po[0] - pa[0]) * f, pa[1] + (po[1] - pa[1]) * f) for f in (0.08, 0.5, 0.97)]
                    centre_ok = abs(tests[1][0]) < HALF + 20 and abs(tests[1][1]) < HALF + 20
                    garde.append(centre_ok and not any(grille.sur_la_route(tp, ri, j) for tp in tests))
                # les morceaux gardes qui se suivent forment un seul bloc
                q = 0
                while q < k:
                    if not garde[q]:
                        q += 1
                        continue
                    q1 = q
                    while q1 + 1 < k and garde[q1 + 1]:
                        q1 += 1
                    t0, t1 = q / float(k), (q1 + 1) / float(k)

                    def lerp(u, v, t):
                        return (u[0] + (v[0] - u[0]) * t, u[1] + (v[1] - u[1]) * t)
                    quad = (lerp(ia, ib, t0), lerp(oa, ob, t0), lerp(oa, ob, t1), lerp(ia, ib, t1))
                    quads.append((quad, ri))
                    q = q1 + 1
    return quads


def _ccw(quad):
    aire = sum(quad[i][0] * quad[(i + 1) % 4][1] - quad[(i + 1) % 4][0] * quad[i][1] for i in range(4))
    return quad if aire > 0 else (quad[3], quad[2], quad[1], quad[0])


def bloc(quad, ri):
    """quadrilatere -> (sommets xyz, faces, uvs) : dessus, bordure cote route, cote exterieur, deux bouts"""
    q = _ccw(quad)
    dz = (ri % 4) * 0.004            # 0 a 1,2 cm : evite le scintillement quand deux trottoirs se chevauchent
    zt = ROAD_Z + HAUTEUR_TROTTOIR + dz
    zb = ROAD_Z - PROFONDEUR
    haut = [(x, y, zt) for x, y in q]
    bas = [(x, y, zb) for x, y in q]
    verts = haut + bas
    faces = [(0, 1, 2, 3)]
    uvs = [[(x / UV_METRES, y / UV_METRES) for x, y in q]]
    for i in range(4):
        j = (i + 1) % 4
        L = math.hypot(q[j][0] - q[i][0], q[j][1] - q[i][1])
        faces.append((4 + i, 4 + j, j, i))           # paroi verticale tournee vers l'exterieur
        h = (zt - zb) / UV_METRES
        uvs.append([(0, 0), (L / UV_METRES, 0), (L / UV_METRES, h), (0, h)])
    return verts, faces, uvs


def calculer(osm_ways):
    rues = lire_routes(osm_ways)
    n_ronds = centres_ronds_points(rues)
    grille = Grille(rues)
    quads = bandes_de_trottoir(rues, grille)
    tile = SIZE_M / TILES
    paquets = {}
    for quad, ri in quads:
        cx = sum(p[0] for p in quad) / 4
        cy = sum(p[1] for p in quad) / 4
        tx = min(TILES - 1, max(0, int((cx + HALF) // tile)))
        ty = min(TILES - 1, max(0, int((cy + HALF) // tile)))
        paquets.setdefault((tx, ty), []).append((quad, ri))
    surface = sum(0.5 * abs(sum(q[i][0] * q[(i + 1) % 4][1] - q[(i + 1) % 4][0] * q[i][1] for i in range(4)))
                  for q, _ in quads)
    return rues, n_ronds, paquets, surface


def ecrire_fbx(chemin, nom, blocs):
    """FBX texte (meme format que build_buildings.write_fbx), un seul materiau 'Trottoir'"""
    verts, polys, uvlist = [], [], []
    for v, f, uv in blocs:
        o = len(verts)
        verts += v
        for fuv in uv:
            uvlist += [c for pair in fuv for c in pair]
        for face in f:
            ids = [i + o for i in face]
            ids[-1] = -(ids[-1] + 1)
            polys += ids
    nfaces = sum(len(f) for _, f, _ in blocs)
    vs = ",".join("%.2f,%.2f,%.2f" % (x * 100, y * 100, z * 100) for x, y, z in verts)
    ps = ",".join(map(str, polys))
    ms = ",".join(["0"] * nfaces)
    uvs = ",".join("%.4f" % c for c in uvlist)
    with open(chemin, "w") as fh:
        fh.write('''; FBX 6.1.0 project file
FBXHeaderExtension:  {
	FBXHeaderVersion: 1003
	FBXVersion: 6100
}
GlobalSettings:  {
	Version: 1000
	Properties60:  {
		Property: "UpAxis", "int", "",2
		Property: "UpAxisSign", "int", "",1
		Property: "FrontAxis", "int", "",1
		Property: "FrontAxisSign", "int", "",-1
		Property: "CoordAxis", "int", "",0
		Property: "CoordAxisSign", "int", "",1
		Property: "UnitScaleFactor", "double", "",1
	}
}
Definitions:  {
	Version: 100
	Count: 2
	ObjectType: "Model" {
		Count: 1
	}
	ObjectType: "Material" {
		Count: 1
	}
}
Objects:  {
	Material: "Material::Trottoir", "" {
		Version: 102
		ShadingModel: "phong"
		MultiLayer: 0
		Properties60:  {
			Property: "DiffuseColor", "ColorRGB", "",0.5,0.5,0.5
		}
	}
	Model: "Model::%(nom)s", "Mesh" {
		Version: 232
		Properties60:  {
			Property: "Lcl Translation", "Lcl Translation", "A+",0,0,0
			Property: "Lcl Rotation", "Lcl Rotation", "A+",0,0,0
			Property: "Lcl Scaling", "Lcl Scaling", "A+",1,1,1
		}
		MultiLayer: 0
		MultiTake: 1
		Shading: Y
		Culling: "CullingOff"
		Vertices: %(vs)s
		PolygonVertexIndex: %(ps)s
		GeometryVersion: 124
		LayerElementMaterial:  {
			Version: 101
			Name: ""
			MappingInformationType: "ByPolygon"
			ReferenceInformationType: "IndexToDirect"
			Materials: %(ms)s
		}
		LayerElementUV: 0 {
			Version: 101
			Name: "map1"
			MappingInformationType: "ByPolygonVertex"
			ReferenceInformationType: "Direct"
			UV: %(uvs)s
		}
		Layer: 0 {
			Version: 100
			LayerElement:  {
				Type: "LayerElementMaterial"
				TypedIndex: 0
			}
			LayerElement:  {
				Type: "LayerElementUV"
				TypedIndex: 0
			}
		}
	}
}
Relations:  {
	Model: "Model::%(nom)s", "Mesh" {
	}
}
Connections:  {
	Connect: "OO", "Model::%(nom)s", "Model::Scene"
	Connect: "OO", "Material::Trottoir", "Model::%(nom)s"
}
''' % {"nom": nom, "vs": vs, "ps": ps, "ms": ms, "uvs": uvs})
    xs = [x for x, _, _ in verts]
    ys = [y for _, y, _ in verts]
    zs = [z for _, _, z in verts]
    # boite attendue (cm) : centre et demi-dimensions, repere FBX
    return [(min(xs) + max(xs)) * 50, (min(ys) + max(ys)) * 50, (min(zs) + max(zs)) * 50,
            (max(xs) - min(xs)) * 50, (max(ys) - min(ys)) * 50, (max(zs) - min(zs)) * 50]


# =============================================================================================== Unreal
def executer_dans_unreal():
    import unreal

    proj = unreal.SystemLibrary.get_project_directory()
    out_dir = os.path.join(proj, "Tools", "out")
    fbx_dir = os.path.join(out_dir, "trottoirs")
    os.makedirs(fbx_dir, exist_ok=True)
    journal = os.path.join(out_dir, "ajouter_trottoirs.log")
    open(journal, "w").write("")

    def log(msg):
        unreal.log(str(msg))
        open(journal, "a", encoding="utf-8").write(str(msg) + "\n")

    try:
        eal = unreal.EditorAssetLibrary
        ell = unreal.EditorLevelLibrary
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        world = ell.get_editor_world()
        log("Niveau ouvert : %s" % world.get_path_name())

        # ---- 0. verifications avant de toucher a quoi que ce soit
        actors = ell.get_all_level_actors()
        routes = [a for a in actors if a.get_actor_label().startswith("Chelles_Tile_Road_")]
        if not routes:
            log("ABANDON : aucune route 'Chelles_Tile_Road_*' dans ce niveau. Ouvrez le niveau du quartier "
                "(celui ou chelles_complet.py a pose les routes), puis relancez. Rien n'a ete modifie.")
            return
        osm = os.path.join(out_dir, "osm_ways.json")
        if not os.path.exists(osm):
            log("ABANDON : fichier %s introuvable. Rien n'a ete modifie." % osm)
            return

        ar = unreal.AssetRegistryHelpers.get_asset_registry()
        candidats = []
        for classe in ("MaterialInstanceConstant", "Material"):
            for d in ar.get_assets_by_class(classe, True):
                if str(d.asset_name).lower() == NOM_MATERIAU.lower():
                    candidats.append(str(d.object_path))
        if not candidats:
            log("ABANDON : aucun materiau nomme '%s' dans le Content. Rien n'a ete modifie." % NOM_MATERIAU)
            return
        candidats.sort(key=lambda p: (len(p), p))
        mat_route = unreal.load_asset(candidats[0])
        log("Materiau trouve : %s%s" % (candidats[0], ("  (autres : %s)" % ", ".join(candidats[1:])) if len(candidats) > 1 else ""))

        # ---- 1. sauvegarde du niveau (copie du fichier .umap tel qu'il est enregistre sur le disque)
        pkg = world.get_outermost().get_name()                    # ex. /Game/FPS_Game/Maps/Rachid
        rel = pkg[len("/Game/"):] if pkg.startswith("/Game/") else None
        stamp = datetime.datetime.now().strftime("%Y-%m-%d_%Hh%M")
        sauv = os.path.join(proj, "Saved", "Sauvegardes", "trottoirs_" + stamp)
        copies = 0
        if rel:
            base = os.path.join(unreal.SystemLibrary.get_project_content_directory(), *rel.split("/"))
            os.makedirs(sauv, exist_ok=True)
            for suffixe in (".umap", "_BuiltData.uasset"):
                if os.path.exists(base + suffixe):
                    shutil.copy2(base + suffixe, sauv)
                    copies += 1
        if copies == 0:
            log("ABANDON : impossible de sauvegarder le niveau (enregistrez-le d'abord avec Ctrl+S). Rien n'a ete modifie.")
            return
        log("1/5 sauvegarde du niveau : %s" % sauv)

        # ---- 2. calcul des trottoirs
        rues, n_ronds, paquets, surface = calculer(osm)
        log("2/5 %d rues (%d rond(s)-point(s)) -> %d blocs de trottoir, %.0f m2"
            % (len(rues), n_ronds, sum(len(v) for v in paquets.values()), surface))
        tuiles = []
        for (tx, ty), liste in sorted(paquets.items()):
            nom = "SM_Trottoir_%d_%d" % (tx, ty)
            boite = ecrire_fbx(os.path.join(fbx_dir, nom + ".fbx"), nom, [bloc(q, ri) for q, ri in liste])
            tuiles.append((nom, boite))

        # ---- 3. materiau : copie "deux faces" de Road_Inst (Road_Inst lui-meme n'est pas modifie)
        mat = mat_route
        try:
            chemin_mi = DOSSIER_ASSETS + "/MI_Trottoir_Road"
            mi = eal.load_asset(chemin_mi) if eal.does_asset_exist(chemin_mi) else tools.create_asset(
                "MI_Trottoir_Road", DOSSIER_ASSETS, unreal.MaterialInstanceConstant,
                unreal.MaterialInstanceConstantFactoryNew())
            unreal.MaterialEditingLibrary.set_material_instance_parent(mi, mat_route)
            ov = mi.get_editor_property("base_property_overrides")
            ov.set_editor_property("override_two_sided", True)
            ov.set_editor_property("two_sided", True)
            mi.set_editor_property("base_property_overrides", ov)
            unreal.MaterialEditingLibrary.update_material_instance(mi)
            eal.save_loaded_asset(mi)
            mat = mi
            log("3/5 materiau : %s (enfant de %s, visible des deux cotes)" % (chemin_mi, NOM_MATERIAU))
        except Exception as e:
            log("3/5 materiau : %s utilise directement (%s)" % (NOM_MATERIAU, e))

        # ---- 4. import
        tasks = []
        for nom, _ in tuiles:
            ui = unreal.FbxImportUI()
            ui.set_editor_property("import_mesh", True)
            ui.set_editor_property("import_materials", False)
            ui.set_editor_property("import_textures", False)
            ui.set_editor_property("import_as_skeletal", False)
            sm = ui.get_editor_property("static_mesh_import_data")
            sm.set_editor_property("combine_meshes", True)
            sm.set_editor_property("auto_generate_collision", False)
            sm.set_editor_property("generate_lightmap_u_vs", False)
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", os.path.join(fbx_dir, nom + ".fbx"))
            task.set_editor_property("destination_path", DOSSIER_ASSETS)
            task.set_editor_property("destination_name", nom)
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("save", False)
            task.set_editor_property("options", ui)
            tasks.append(task)
        tools.import_asset_tasks(tasks)
        meshes = {}
        for nom, _ in tuiles:
            m = eal.load_asset("%s/%s" % (DOSSIER_ASSETS, nom))
            if m is None:
                log("ABANDON : import rate pour %s. Le niveau n'a pas ete modifie." % nom)
                return
            for i in range(len(m.get_editor_property("static_materials"))):
                m.set_material(i, mat)
            try:
                bs = m.get_editor_property("body_setup")
                bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
                bs.set_editor_property("double_sided_geometry", True)
            except Exception as e:
                log("   collision : %s" % e)
            eal.save_loaded_asset(m)
            meshes[nom] = m
        log("4/5 %d morceaux importes dans %s" % (len(meshes), DOSSIER_ASSETS))

        # ---- 5. pose : meme position / rotation / echelle que les routes, puis verification
        ref = routes[0]
        loc, rot, sca = ref.get_actor_location(), ref.get_actor_rotation(), ref.get_actor_scale3d()
        # sens de l'axe Y du niveau, mesure sur une route existante (comme snap_to_landscape.py)
        o, _ = ref.get_actor_bounds(False)
        ysign = -1.0
        try:
            man = json.load(open(os.path.join(out_dir, "manifest.json")))
            bx = next(t["box_cm"] for t in man["tiles"] if t["name"] == ref.get_actor_label())
            ysign = -1.0 if abs(o.y + bx[1]) <= abs(o.y - bx[1]) else 1.0
        except Exception:
            pass

        anciens = [a for a in ell.get_all_level_actors() if a.get_actor_label().startswith(PREFIXE)]
        for a in anciens:
            a.destroy_actor()
        poses, pire = [], 0.0
        for nom, bx in tuiles:
            a = ell.spawn_actor_from_object(meshes[nom], loc, rot)
            a.set_actor_scale3d(sca)
            a.set_actor_label(PREFIXE + nom[len("SM_Trottoir_"):])
            a.set_folder_path(DOSSIER_NIVEAU)
            a.root_component.set_mobility(ref.root_component.get_editor_property("mobility"))
            o, e = a.get_actor_bounds(False)
            ecart = max(abs(o.x - bx[0]), abs(o.y - ysign * bx[1]), abs(e.x - bx[3]), abs(e.y - bx[4]))
            pire = max(pire, ecart)
            poses.append(a)
        if pire > 100.0:
            for a in poses:
                a.destroy_actor()
            log("ABANDON : les trottoirs ne tombent pas sur les routes (ecart %.0f cm). Ils ont ete retires, "
                "le niveau est comme avant." % pire)
            return
        log("5/5 %d morceaux poses (dossier %s du World Outliner), ecart %.1f cm%s"
            % (len(poses), DOSSIER_NIVEAU, pire, (", %d ancien(s) remplace(s)" % len(anciens)) if anciens else ""))
        if ell.save_current_level():
            log("TERMINE : niveau enregistre. Cliquez sur Play pour marcher sur les trottoirs.")
        else:
            log("TERMINE, mais le niveau n'a pas pu etre enregistre : faites Ctrl+S.")
    except Exception:
        log("ERREUR :\n" + traceback.format_exc())


if __name__ == "__main__":
    try:
        import unreal  # noqa: F401
        executer_dans_unreal()
    except ImportError:
        # hors d'Unreal : simple calcul pour verifier (python ajouter_trottoirs.py chemin/osm_ways.json)
        import sys
        chemin = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
            os.path.dirname(os.path.abspath(__file__)), "..", "out", "osm_ways.json")
        rues, n_ronds, paquets, surface = calculer(chemin)
        print("%d rues, %d ronds-points, %d blocs, %.0f m2" % (len(rues), n_ronds,
                                                                sum(len(v) for v in paquets.values()), surface))
