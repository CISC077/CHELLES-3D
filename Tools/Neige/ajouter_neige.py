"""SCRIPT A LANCER DANS UNREAL 4.27 : File > Execute Python Script... > ce fichier.

Fait TOMBER DE LA NEIGE sur la carte ouverte (Maps/Rachid), de tres haut (jusqu'a ~400 m au-dessus du joueur).

Comment ca marche (rien a regler pour commencer) :
  - 60 000 flocons dans un seul maillage ; c'est la carte graphique qui les fait tomber (materiau),
    donc c'est leger et ca ne ralentit pas le jeu ;
  - la neige SUIT LA CAMERA : ou que vous alliez dans Chelles (a pied, en voiture ou en volant
    avec Spider-Man), il neige autour de vous ;
  - 2 couches :
      * "Proche"    : petits flocons denses autour du joueur (40 m x 40 m, jusqu'a 21 m de haut) ;
      * "Lointaine" : gros flocons qui viennent de tres haut (300 m x 300 m, jusqu'a ~400 m de haut) ;
  - flocons eclaires par le soleil/ciel du niveau : ils s'assombrissent la nuit (cycle jour/nuit GoodSky).

  - SOL NEIGEUX : le Landscape recoit un materiau de neige (M_Sol_Neige) ;
  - EMPREINTES : quand on marche sur le Landscape (joueur, passants, soldats), des traces de pas
    apparaissent et s'effacent lentement (~45 s). Il faut le plugin Plugins/ChellesNeige (compile une fois
    avec Plugins/ChellesNeige/Compiler_le_plugin.bat, Unreal ferme).

Ce que le script fait, dans l'ordre :
  1. copie de sauvegarde du niveau ouvert dans Saved/Sauvegardes/neige_<date>/ ;
  2. cree les flocons (/Game/Chelles/Neige/SM_Neige_Flocons) et le materiau M_Neige_Flocons
     + 2 reglages MI_Neige_Proche et MI_Neige_Lointaine ;
  3. pose 2 acteurs "Chelles_Neige_Proche" et "Chelles_Neige_Lointaine" (dossier Chelles/Neige du World Outliner) ;
  4. sol : materiaux des empreintes, M_Sol_Neige sur le Landscape (l'ancien materiau est note pour pouvoir
     revenir en arriere), acteur "Chelles_Neige_Empreintes" ; puis enregistre le niveau.

Ne touche a RIEN d'autre (ciel, soleil, eclairage, batiments, routes...). Peut etre relance : il remplace sa neige.
Pour arreter la neige et rendre au Landscape son ancien materiau : retirer_neige.py.
Pour regler (plus/moins de neige, vitesse, vent, taille) : double-clic sur MI_Neige_Proche ou MI_Neige_Lointaine
dans /Game/Chelles/Neige, cocher le parametre et changer la valeur (Densite, Chute, Vent, Taille...).
Journal : Tools/out/neige.log
"""
import datetime, os, random, shutil, traceback

# ----------------------------------------------------------------------------------------------- reglages
NB_FLOCONS = 60000          # nombre de flocons dans le maillage (partage par les 2 couches)
TAILLE_MAILLAGE = 100000.0  # cm, cube dans lequel les flocons sont tires au hasard (ne pas changer)
PAS = 4.0                   # cm, grille des centres de flocons (ne pas changer)

COUCHES = {
    # nom : reglages du materiau (cm, cm/s)
    "Proche": dict(BoiteXY=4000.0, BoiteZ=3000.0, ZBas=0.3, Chute=110.0, Balancement=25.0, Taille=5.0,
                   Densite=0.45, ProcheDebut=20.0, ProcheLong=60.0, LoinDebut=1200.0, LoinLong=700.0,
                   Opacite=0.9, Vent=(40.0, 15.0)),
    "Lointaine": dict(BoiteXY=30000.0, BoiteZ=45000.0, ZBas=0.1, Chute=140.0, Balancement=60.0, Taille=45.0,
                      Densite=1.0, ProcheDebut=1300.0, ProcheLong=800.0, LoinDebut=9000.0, LoinLong=5500.0,
                      Opacite=0.75, Vent=(40.0, 15.0)),
}

DOSSIER_ASSETS = "/Game/Chelles/Neige"
DOSSIER_NIVEAU = "Chelles/Neige"
PREFIXE = "Chelles_Neige_"
NOM_MESH = "SM_Neige_Flocons"
NOM_MAT = "M_Neige_Flocons"

# Mouvement des flocons, calcule par la carte graphique (World Position Offset).
# Chaque flocon = 4 sommets presque au meme endroit ; leur case de grille commune donne un tirage au hasard
# identique pour les 4 coins, puis le flocon est replace dans une boite qui suit la camera et tourne vers elle.
CODE_MOUVEMENT = r"""
float3 cell = floor(Pos / Pas) + 0.5;
float3 r = frac(cell * Pas / TailleMaillage);
float h1 = frac(sin(dot(r, float3(12.9898, 78.233, 37.719))) * 43758.5453);
float h2 = frac(sin(dot(r, float3(39.346, 11.135, 83.155))) * 24634.6345);
float h3 = frac(sin(dot(r, float3(73.156, 52.235, 9.151))) * 63757.1235);
float3 boite = float3(BoiteXY, BoiteXY, BoiteZ);
float ph = 6.2831853 * h3;
float3 balance = float3(sin(T * 1.3 + ph), cos(T * 0.9 + ph * 1.7), 0.0) * Balancement * (0.5 + h2);
float3 vitesse = float3(Vent.x, Vent.y, -Chute * (0.75 + 0.5 * h3));
float3 W = r * boite + vitesse * T + balance;
float3 origine = Cam - float3(BoiteXY * 0.5, BoiteXY * 0.5, BoiteZ * ZBas);
float3 P = origine + boite * frac((W - origine) / boite);
float2 c = UV - 0.5;
float s = Taille * (0.6 + 0.8 * h2) * step(h1, Densite);
P += (Droite * c.x - Haut * c.y) * s;
return P - Pos;
"""

# Forme ronde et douce du flocon + fondu pres de la camera et au bord de la boite (pas de "coupure" visible).
CODE_OPACITE = r"""
float2 d = UV - 0.5;
float a = saturate(1.0 - dot(d, d) * 4.0);
a *= a;
float3 v = WP - Cam;
float dist = length(v);
float dxy = length(v.xy);
float fp = saturate((dist - ProcheDebut) / max(ProcheLong, 1.0));
float fl = 1.0 - saturate((dxy - LoinDebut) / max(LoinLong, 1.0));
float haut = BoiteZ * (1.0 - ZBas);
float bas = BoiteZ * ZBas;
float fh = 1.0 - saturate((v.z - haut * 0.8) / max(haut * 0.2, 1.0));
float fb = 1.0 - saturate((-v.z - bas * 0.8) / max(bas * 0.2, 1.0));
return a * fp * fl * fh * fb * Opacite;
"""

PARAMS_MOUVEMENT = ["TailleMaillage", "Pas", "BoiteXY", "BoiteZ", "ZBas", "Chute", "Balancement", "Taille", "Densite"]
PARAMS_OPACITE = ["ProcheDebut", "ProcheLong", "LoinDebut", "LoinLong", "BoiteZ", "ZBas", "Opacite"]


def ecrire_obj(chemin, n, graine=77108):
    """n flocons (2 triangles de 2 cm), centres au milieu de cases de PAS cm dans un cube de TAILLE_MAILLAGE cm."""
    rnd = random.Random(graine)
    cases = int(TAILLE_MAILLAGE / PAS)
    lignes = ["# flocons de neige CHELLES 3D (%d)" % n]
    for _ in range(n):
        cx, cy, cz = [(rnd.randrange(cases) + 0.5) * PAS for _ in range(3)]
        for dx, dy in ((-1.0, -1.0), (1.0, -1.0), (1.0, 1.0), (-1.0, 1.0)):
            lignes.append("v %.1f %.1f %.1f" % (cx + dx, cy + dy, cz))
    lignes += ["vt 0 1", "vt 1 1", "vt 1 0", "vt 0 0"]
    for i in range(n):
        b = 4 * i + 1
        lignes.append("f %d/1 %d/2 %d/3" % (b, b + 1, b + 2))
        lignes.append("f %d/1 %d/3 %d/4" % (b, b + 2, b + 3))
    with open(chemin, "w") as f:
        f.write("\n".join(lignes) + "\n")


# ----------------------------------------------------------------------------------------------- sol neigeux
RESOLUTION_EMPREINTES = 2048   # pixels de la texture des empreintes (zone de 40 m -> ~2 cm par pixel)
ZONE_EMPREINTES = 4000.0       # cm, zone autour du joueur qui garde les empreintes
NOM_SOL = "M_Sol_Neige"
TAG_ANCIEN = "ChellesNeigeAncienSol="

CODE_PAS = r"""
float2 p = UV - 0.5;
float2 a = (p - float2(0.12, 0.0)) / float2(0.34, 0.45);
float2 b = (p - float2(-0.28, 0.0)) / float2(0.19, 0.36);
float e = max(saturate((1.0 - dot(a, a)) * 3.0), saturate((1.0 - dot(b, b)) * 3.0));
return e * Force;
"""

CODE_REPORT = r"""
float2 p = UV + Decalage.xy;
if (p.x < 0.0 || p.y < 0.0 || p.x > 1.0 || p.y > 1.0) return 0.0;
return Texture2DSampleLevel(Source, SourceSampler, p, 0).r * Attenuation;
"""

CODE_TRACES = r"""
float2 uv = (WP.xy - float2(CX, CY)) / max(Taille, 1.0) + 0.5;
if (uv.x < 0.002 || uv.y < 0.002 || uv.x > 0.998 || uv.y > 0.998) return float4(0.0, 0.0, 1.0, 0.0);
float t = 1.0 / max(Resolution, 1.0);
float c = saturate(Texture2DSampleLevel(Tex, TexSampler, uv, 0).r);
float l = saturate(Texture2DSampleLevel(Tex, TexSampler, uv - float2(t, 0.0), 0).r);
float r = saturate(Texture2DSampleLevel(Tex, TexSampler, uv + float2(t, 0.0), 0).r);
float d = saturate(Texture2DSampleLevel(Tex, TexSampler, uv - float2(0.0, t), 0).r);
float u = saturate(Texture2DSampleLevel(Tex, TexSampler, uv + float2(0.0, t), 0).r);
float3 n = normalize(float3((l - r) * Profondeur, (d - u) * Profondeur, 1.0));
return float4(n, c);
"""

CODE_COULEUR = r"""
float n1 = Texture2DSample(Bruit, BruitSampler, WP.xy / 3000.0).r;
float n2 = Texture2DSample(Bruit, BruitSampler, WP.xy / 450.0).r;
float v = lerp(0.86, 1.0, n1 * 0.6 + n2 * 0.4);
float k = saturate(Creux);
float3 col = lerp(Neige.rgb * v, Tasse.rgb, k);
return float4(col, lerp(0.7, 0.45, k));
"""


def installer_sol(unreal, log, paysages, prefixe):
    eal = unreal.EditorAssetLibrary
    ell = unreal.EditorLevelLibrary
    mel = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    outils = unreal.NeigeOutils

    def obtenir(nom, classe, fabrique):
        chemin = "%s/%s" % (DOSSIER_ASSETS, nom)
        if eal.does_asset_exist(chemin):
            return eal.load_asset(chemin)
        return tools.create_asset(nom, DOSSIER_ASSETS, classe, fabrique)

    def materiau(nom):
        m = obtenir(nom, unreal.Material, unreal.MaterialFactoryNew())
        mel.delete_all_material_expressions(m)
        return m

    def custom(m, code, sortie, entrees, x, y, desc):
        e = mel.create_material_expression(m, unreal.MaterialExpressionCustom, x, y)
        e.set_editor_property("code", code)
        e.set_editor_property("output_type", sortie)
        e.set_editor_property("description", desc)
        liste = []
        for nom, _ in entrees:
            ci = unreal.CustomInput()
            ci.set_editor_property("input_name", nom)
            liste.append(ci)
        e.set_editor_property("inputs", liste)
        for nom, source in entrees:
            if not mel.connect_material_expressions(source, "", e, nom):
                raise RuntimeError("liaison impossible : %s.%s" % (desc, nom))
        return e

    def scalaire(m, nom, val, x, y):
        e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
        e.set_editor_property("parameter_name", nom)
        e.set_editor_property("default_value", float(val))
        return e

    def couleur(m, nom, rgb, x, y):
        e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
        e.set_editor_property("parameter_name", nom)
        e.set_editor_property("default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return e

    def masque(m, src, canaux, x, y):
        e = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, x, y)
        for c in "rgba":
            e.set_editor_property(c, c in canaux)
        mel.connect_material_expressions(src, "", e, "")
        return e

    def sortie(m, e, prop):
        if not mel.connect_material_property(e, "", prop):
            raise RuntimeError("liaison impossible vers %s" % prop)

    # textures des empreintes (format flottant : l'effacement lent ne "colle" pas)
    rts = {}
    for nom in ("RT_Empreintes", "RT_Empreintes_Tampon"):
        rt = obtenir(nom, unreal.TextureRenderTarget2D, unreal.TextureRenderTargetFactoryNew())
        rt.set_editor_property("render_target_format", unreal.TextureRenderTargetFormat.RTF_R16F)
        rt.set_editor_property("size_x", RESOLUTION_EMPREINTES)
        rt.set_editor_property("size_y", RESOLUTION_EMPREINTES)
        rt.set_editor_property("clear_color", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
        rt.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
        rt.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
        eal.save_loaded_asset(rt)
        rts[nom] = rt

    mpc = obtenir("MPC_Empreintes", unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    for nom, val in (("CentreX", 0.0), ("CentreY", 0.0), ("Taille", ZONE_EMPREINTES)):
        if not outils.ajouter_scalaire_collection(mpc, nom, val):
            raise RuntimeError("MPC_Empreintes : parametre %s impossible" % nom)
    eal.save_loaded_asset(mpc)

    # forme d'un pas (additif)
    m_pas = materiau("M_Empreinte_Pas")
    m_pas.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m_pas.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    uv = mel.create_material_expression(m_pas, unreal.MaterialExpressionTextureCoordinate, -600, 0)
    force = scalaire(m_pas, "Force", 1.0, -600, 120)
    sortie(m_pas, custom(m_pas, CODE_PAS, unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                         [("UV", uv), ("Force", force)], -300, 0, "Empreinte"),
           unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m_pas)
    eal.save_loaded_asset(m_pas)

    # decalage + effacement (opaque)
    m_rep = materiau("M_Empreinte_Report")
    m_rep.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    uv = mel.create_material_expression(m_rep, unreal.MaterialExpressionTextureCoordinate, -600, 0)
    src = mel.create_material_expression(m_rep, unreal.MaterialExpressionTextureObjectParameter, -600, 120)
    src.set_editor_property("parameter_name", "Source")
    src.set_editor_property("texture", rts["RT_Empreintes_Tampon"])
    dec = couleur(m_rep, "Decalage", (0.0, 0.0, 0.0), -600, 300)
    att = scalaire(m_rep, "Attenuation", 1.0, -600, 450)
    sortie(m_rep, custom(m_rep, CODE_REPORT, unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                         [("UV", uv), ("Source", src), ("Decalage", dec), ("Attenuation", att)],
                         -300, 0, "Report"),
           unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m_rep)
    eal.save_loaded_asset(m_rep)

    # materiau du sol
    sol = materiau(NOM_SOL)
    wp = mel.create_material_expression(sol, unreal.MaterialExpressionWorldPosition, -900, -300)
    tex = mel.create_material_expression(sol, unreal.MaterialExpressionTextureObject, -900, -200)
    tex.set_editor_property("texture", rts["RT_Empreintes"])
    coll = {}
    y = -50
    for nom in ("CentreX", "CentreY", "Taille"):
        e = mel.create_material_expression(sol, unreal.MaterialExpressionCollectionParameter, -900, y)
        if not outils.regler_noeud_collection(e, mpc, nom):
            raise RuntimeError("noeud MPC %s impossible" % nom)
        coll[nom] = e
        y += 90
    res = scalaire(sol, "Resolution", RESOLUTION_EMPREINTES, -900, y)
    prof = scalaire(sol, "Profondeur", 6.0, -900, y + 90)
    traces = custom(sol, CODE_TRACES, unreal.CustomMaterialOutputType.CMOT_FLOAT4,
                    [("WP", wp), ("Tex", tex), ("CX", coll["CentreX"]), ("CY", coll["CentreY"]),
                     ("Taille", coll["Taille"]), ("Resolution", res), ("Profondeur", prof)],
                    -550, -100, "Neige_Traces")
    bruit_tex = None
    for chemin in ("/Game/StarterContent/Textures/T_Perlin_Noise_M", "/Engine/EngineResources/WhiteSquareTexture"):
        if eal.does_asset_exist(chemin):
            bruit_tex = eal.load_asset(chemin)
            break
    bruit = mel.create_material_expression(sol, unreal.MaterialExpressionTextureObject, -900, 350)
    bruit.set_editor_property("texture", bruit_tex)
    creux = masque(sol, traces, "a", -300, 50)
    neige = couleur(sol, "CouleurNeige", (0.82, 0.85, 0.9), -900, 500)
    tasse = couleur(sol, "CouleurTasse", (0.58, 0.64, 0.74), -900, 650)
    col = custom(sol, CODE_COULEUR, unreal.CustomMaterialOutputType.CMOT_FLOAT4,
                 [("WP", wp), ("Bruit", bruit), ("Creux", creux), ("Neige", neige), ("Tasse", tasse)],
                 -150, 300, "Neige_Couleur")
    sortie(sol, masque(sol, col, "rgb", 100, 250), unreal.MaterialProperty.MP_BASE_COLOR)
    sortie(sol, masque(sol, col, "a", 100, 400), unreal.MaterialProperty.MP_ROUGHNESS)
    sortie(sol, masque(sol, traces, "rgb", -300, -150), unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(sol)
    eal.save_loaded_asset(sol)
    log("5/5 sol : %s/%s, empreintes %dx%d sur %.0f m autour du joueur"
        % (DOSSIER_ASSETS, NOM_SOL, RESOLUTION_EMPREINTES, RESOLUTION_EMPREINTES, ZONE_EMPREINTES / 100.0))

    for p in paysages:
        ancien = p.get_editor_property("landscape_material")
        tags = [str(t) for t in p.get_editor_property("tags")]
        if not any(t.startswith(TAG_ANCIEN) for t in tags):
            tags.append(TAG_ANCIEN + (ancien.get_path_name() if ancien else ""))
            p.set_editor_property("tags", [unreal.Name(t) for t in tags])
        p.set_editor_property("landscape_material", sol)
        log("   Landscape '%s' : neige (ancien materiau : %s)" % (p.get_actor_label(), ancien.get_path_name() if ancien else "aucun"))

    for a in ell.get_all_level_actors():
        if a.get_actor_label() == prefixe + "Empreintes":
            ell.destroy_actor(a)
    a = ell.spawn_actor_from_class(unreal.EmpreintesNeige, unreal.Vector(0.0, 0.0, 0.0))
    a.set_actor_label(prefixe + "Empreintes")
    a.set_folder_path(DOSSIER_NIVEAU)
    for prop, val in (("cible", rts["RT_Empreintes"]), ("tampon", rts["RT_Empreintes_Tampon"]),
                      ("mat_empreinte", m_pas), ("mat_report", m_rep), ("collection", mpc),
                      ("taille_zone", ZONE_EMPREINTES)):
        a.set_editor_property(prop, val)
    log("   acteur %sEmpreintes pose (reglages : duree de disparition, taille des pas...)" % prefixe)


def executer_dans_unreal():
    import unreal

    proj = unreal.SystemLibrary.get_project_directory()
    out_dir = os.path.join(proj, "Tools", "out")
    os.makedirs(out_dir, exist_ok=True)
    journal = os.path.join(out_dir, "neige.log")
    open(journal, "w").write("")

    def log(msg):
        unreal.log(str(msg))
        open(journal, "a", encoding="utf-8").write(str(msg) + "\n")

    try:
        eal = unreal.EditorAssetLibrary
        ell = unreal.EditorLevelLibrary
        mel = unreal.MaterialEditingLibrary
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        world = ell.get_editor_world()
        log("Niveau ouvert : %s" % world.get_path_name())

        if not hasattr(unreal, "NeigeOutils") or not hasattr(unreal, "EmpreintesNeige"):
            log("ABANDON : le plugin ChellesNeige n'est pas charge. Fermez Unreal, double-cliquez sur "
                "Plugins/ChellesNeige/Compiler_le_plugin.bat, rouvrez le projet puis relancez ce script. "
                "Rien n'a ete modifie.")
            return
        paysages = [a for a in ell.get_all_level_actors() if isinstance(a, unreal.LandscapeProxy)]
        if not paysages:
            log("Attention : aucun Landscape dans ce niveau, il y aura les flocons mais pas de sol neigeux.")

        # ---- 1. sauvegarde du niveau
        pkg = world.get_outermost().get_name()
        rel = pkg[len("/Game/"):] if pkg.startswith("/Game/") else None
        stamp = datetime.datetime.now().strftime("%Y-%m-%d_%Hh%M")
        sauv = os.path.join(proj, "Saved", "Sauvegardes", "neige_" + stamp)
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

        # ---- 2. maillage des flocons
        obj = os.path.join(out_dir, NOM_MESH + ".obj")
        ecrire_obj(obj, NB_FLOCONS)
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ui.set_editor_property("import_as_skeletal", False)
        sm = ui.get_editor_property("static_mesh_import_data")
        sm.set_editor_property("combine_meshes", True)
        sm.set_editor_property("auto_generate_collision", False)
        sm.set_editor_property("generate_lightmap_u_vs", False)
        sm.set_editor_property("import_uniform_scale", 1.0)
        for prop, val in (("remove_degenerates", False), ("build_adjacency_buffer", False),
                          ("build_reversed_index_buffer", False)):
            try:
                sm.set_editor_property(prop, val)
            except Exception:
                pass
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", obj)
        task.set_editor_property("destination_path", DOSSIER_ASSETS)
        task.set_editor_property("destination_name", NOM_MESH)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        task.set_editor_property("options", ui)
        tools.import_asset_tasks([task])
        mesh = eal.load_asset("%s/%s" % (DOSSIER_ASSETS, NOM_MESH))
        if mesh is None:
            log("ABANDON : import des flocons rate (%s). Le niveau n'a pas ete modifie." % obj)
            return
        zero = unreal.Vector(0.0, 0.0, 0.0)
        mesh.set_editor_property("positive_bounds_extension", zero)
        mesh.set_editor_property("negative_bounds_extension", zero)
        ext = mesh.get_bounds().box_extent
        taille_mesuree = 2.0 * max(abs(ext.x), abs(ext.y), abs(ext.z))
        taille_prevue = TAILLE_MAILLAGE - PAS  # du premier au dernier centre possible, a peu pres
        echelle = taille_mesuree / taille_prevue if taille_prevue > 0 else 1.0
        if echelle < 0.5 or echelle > 2.0:
            echelle = round(echelle) if echelle >= 1 else echelle
        log("2/5 %d flocons importes (%s), taille %.0f cm, echelle %.3f" % (NB_FLOCONS, NOM_MESH, taille_mesuree, echelle))
        # tres grandes "bornes" : Unreal ne doit jamais cacher la neige, ou que soit la camera (10 km)
        grand = unreal.Vector(1000000.0, 1000000.0, 1000000.0)
        mesh.set_editor_property("positive_bounds_extension", grand)
        mesh.set_editor_property("negative_bounds_extension", grand)
        try:
            mesh.set_editor_property("generate_mesh_distance_field", False)
        except Exception:
            pass

        # ---- 3. materiau + 2 reglages
        chemin_mat = "%s/%s" % (DOSSIER_ASSETS, NOM_MAT)
        if eal.does_asset_exist(chemin_mat):
            mat = eal.load_asset(chemin_mat)
            mel.delete_all_material_expressions(mat)
        else:
            mat = tools.create_asset(NOM_MAT, DOSSIER_ASSETS, unreal.Material, unreal.MaterialFactoryNew())
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        try:  # eclairage par pixel : flocons bien eclaires par le soleil/ciel, meme loin, et sombres la nuit
            mat.set_editor_property("translucency_lighting_mode",
                                    unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
        except Exception:
            mat.set_editor_property("translucency_lighting_mode",
                                    unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
        mat.set_editor_property("two_sided", True)
        mat.set_editor_property("tangent_space_normal", False)  # normale "vers le ciel" pour tous les flocons

        def expr(classe, x, y):
            return mel.create_material_expression(mat, classe, x, y)

        valeurs_def = dict(COUCHES["Proche"])
        valeurs_def.update(TailleMaillage=TAILLE_MAILLAGE, Pas=PAS)
        scal = {}
        y = -900
        for nom in sorted(set(PARAMS_MOUVEMENT + PARAMS_OPACITE)):
            e = expr(unreal.MaterialExpressionScalarParameter, -900, y)
            e.set_editor_property("parameter_name", nom)
            e.set_editor_property("default_value", float(valeurs_def[nom]))
            e.set_editor_property("group", "Neige")
            scal[nom] = e
            y += 80
        vent = expr(unreal.MaterialExpressionVectorParameter, -900, y)
        vent.set_editor_property("parameter_name", "Vent")
        vent.set_editor_property("default_value", unreal.LinearColor(40.0, 15.0, 0.0, 0.0))
        vent.set_editor_property("group", "Neige")
        couleur = expr(unreal.MaterialExpressionVectorParameter, -400, 500)
        couleur.set_editor_property("parameter_name", "Couleur")
        couleur.set_editor_property("default_value", unreal.LinearColor(0.92, 0.95, 1.0, 1.0))
        couleur.set_editor_property("group", "Neige")
        eclat = expr(unreal.MaterialExpressionScalarParameter, -400, 650)
        eclat.set_editor_property("parameter_name", "LueurPropre")
        eclat.set_editor_property("default_value", 0.04)
        eclat.set_editor_property("group", "Neige")

        pos = expr(unreal.MaterialExpressionWorldPosition, -900, -1100)
        cam = expr(unreal.MaterialExpressionCameraPositionWS, -900, -1200)
        uv = expr(unreal.MaterialExpressionTextureCoordinate, -900, -1300)
        temps = expr(unreal.MaterialExpressionTime, -900, -1400)
        axes = {}
        for nom, vec, yy in (("Droite", (1.0, 0.0, 0.0), -1600), ("Haut", (0.0, 1.0, 0.0), -1500)):
            c = expr(unreal.MaterialExpressionConstant3Vector, -1100, yy)
            c.set_editor_property("constant", unreal.LinearColor(vec[0], vec[1], vec[2], 0.0))
            t = expr(unreal.MaterialExpressionTransform, -900, yy)
            t.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_VIEW)
            t.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
            mel.connect_material_expressions(c, "", t, "")
            axes[nom] = t

        def custom(code, sortie, entrees, x, y, desc):
            e = expr(unreal.MaterialExpressionCustom, x, y)
            e.set_editor_property("code", code)
            e.set_editor_property("output_type", sortie)
            e.set_editor_property("description", desc)
            liste = []
            for nom, _ in entrees:
                ci = unreal.CustomInput()
                ci.set_editor_property("input_name", nom)
                liste.append(ci)
            e.set_editor_property("inputs", liste)
            for nom, source in entrees:
                if not mel.connect_material_expressions(source, "", e, nom):
                    raise RuntimeError("liaison impossible : %s -> %s.%s" % (source.get_name(), desc, nom))
            return e

        mouvement = custom(CODE_MOUVEMENT, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                           [("Pos", pos), ("Cam", cam), ("UV", uv), ("T", temps), ("Droite", axes["Droite"]),
                            ("Haut", axes["Haut"]), ("Vent", vent)] + [(n, scal[n]) for n in PARAMS_MOUVEMENT],
                           -300, -1000, "Neige_Mouvement")
        opacite = custom(CODE_OPACITE, unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                         [("UV", uv), ("WP", pos), ("Cam", cam)] + [(n, scal[n]) for n in PARAMS_OPACITE],
                         -300, 0, "Neige_Opacite")
        normale = expr(unreal.MaterialExpressionConstant3Vector, -150, 400)
        normale.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
        lueur = expr(unreal.MaterialExpressionMultiply, -150, 600)
        mel.connect_material_expressions(couleur, "", lueur, "A")
        mel.connect_material_expressions(eclat, "", lueur, "B")
        # Python 4.27 ne sait pas brancher "World Position Offset" : le plugin ChellesNeige le fait
        ok = all([
            unreal.NeigeOutils.brancher_position_monde(mat, mouvement),
            mel.connect_material_property(opacite, "", unreal.MaterialProperty.MP_OPACITY),
            mel.connect_material_property(couleur, "", unreal.MaterialProperty.MP_BASE_COLOR),
            mel.connect_material_property(lueur, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR),
            mel.connect_material_property(normale, "", unreal.MaterialProperty.MP_NORMAL),
        ])
        if not ok:
            raise RuntimeError("liaison du materiau aux sorties impossible")
        mel.recompile_material(mat)
        eal.save_loaded_asset(mat)

        instances = {}
        for nom, reg in COUCHES.items():
            chemin_mi = "%s/MI_Neige_%s" % (DOSSIER_ASSETS, nom)
            if eal.does_asset_exist(chemin_mi):
                mi = eal.load_asset(chemin_mi)
            else:
                mi = tools.create_asset("MI_Neige_" + nom, DOSSIER_ASSETS, unreal.MaterialInstanceConstant,
                                        unreal.MaterialInstanceConstantFactoryNew())
            mel.set_material_instance_parent(mi, mat)
            vals = dict(reg)
            vals.update(TailleMaillage=TAILLE_MAILLAGE * echelle, Pas=PAS * echelle)
            for p in sorted(set(PARAMS_MOUVEMENT + PARAMS_OPACITE)):
                mel.set_material_instance_scalar_parameter_value(mi, p, float(vals[p]))
            vx, vy = reg["Vent"]
            mel.set_material_instance_vector_parameter_value(mi, "Vent", unreal.LinearColor(vx, vy, 0.0, 0.0))
            eal.save_loaded_asset(mi)
            instances[nom] = mi
        mesh.set_material(0, instances["Proche"])
        eal.save_loaded_asset(mesh)
        log("3/5 materiau %s + reglages %s" % (chemin_mat, ", ".join("MI_Neige_" + n for n in COUCHES)))

        # ---- 4. pose des 2 acteurs (a l'origine, sans rotation ni echelle : le materiau en a besoin)
        anciens = [a for a in ell.get_all_level_actors() if a.get_actor_label().startswith(PREFIXE)]
        for a in anciens:
            ell.destroy_actor(a)
        for nom, mi in instances.items():
            a = ell.spawn_actor_from_object(mesh, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
            a.set_actor_label(PREFIXE + nom)
            a.set_folder_path(DOSSIER_NIVEAU)
            a.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
            c = a.static_mesh_component
            c.set_mobility(unreal.ComponentMobility.MOVABLE)
            c.set_material(0, mi)
            c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            for prop, val in (("cast_shadow", False), ("receives_decals", False), ("generate_overlap_events", False),
                              ("affect_distance_field_lighting", False), ("visible_in_reflection_captures", False),
                              ("visible_in_ray_tracing", False), ("can_character_step_up_on",
                                                                   unreal.CanBeCharacterBase.ECB_NO)):
                try:
                    c.set_editor_property(prop, val)
                except Exception:
                    pass
        log("4/5 %d acteur(s) remplace(s), 2 poses : %s (dossier %s)"
            % (len(anciens), ", ".join(PREFIXE + n for n in COUCHES), DOSSIER_NIVEAU))
        # ---- 5. sol neigeux + empreintes
        installer_sol(unreal, log, paysages, PREFIXE)
        if ell.save_current_level():
            log("TERMINE : niveau enregistre. Cliquez sur Play : il neige, et vos pas marquent la neige !")
        else:
            log("TERMINE, mais le niveau n'a pas pu etre enregistre : faites Ctrl+S.")
    except Exception:
        log("ERREUR :\n" + traceback.format_exc())


if __name__ == "__main__":
    try:
        import unreal  # noqa: F401
        executer_dans_unreal()
    except ImportError:
        import sys
        chemin = sys.argv[1] if len(sys.argv) > 1 else "flocons_test.obj"
        ecrire_obj(chemin, int(sys.argv[2]) if len(sys.argv) > 2 else 1000)
        print("ecrit", chemin)
