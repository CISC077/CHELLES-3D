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

Ce que le script fait, dans l'ordre :
  1. copie de sauvegarde du niveau ouvert dans Saved/Sauvegardes/neige_<date>/ ;
  2. cree les flocons (/Game/Chelles/Neige/SM_Neige_Flocons) et le materiau M_Neige_Flocons
     + 2 reglages MI_Neige_Proche et MI_Neige_Lointaine ;
  3. pose 2 acteurs "Chelles_Neige_Proche" et "Chelles_Neige_Lointaine" (dossier Chelles/Neige du World Outliner),
     puis enregistre le niveau.

Ne touche a RIEN d'autre (ciel, soleil, eclairage, batiments, routes...). Peut etre relance : il remplace sa neige.
Pour arreter la neige : retirer_neige.py (ou cacher les 2 acteurs).
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
        log("1/4 sauvegarde du niveau : %s" % sauv)

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
        log("2/4 %d flocons importes (%s), taille %.0f cm, echelle %.3f" % (NB_FLOCONS, NOM_MESH, taille_mesuree, echelle))
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
        ok = all([
            mel.connect_material_property(mouvement, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET),
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
        log("3/4 materiau %s + reglages %s" % (chemin_mat, ", ".join("MI_Neige_" + n for n in COUCHES)))

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
        log("4/4 %d acteur(s) remplace(s), 2 poses : %s (dossier %s)"
            % (len(anciens), ", ".join(PREFIXE + n for n in COUCHES), DOSSIER_NIVEAU))
        if ell.save_current_level():
            log("TERMINE : niveau enregistre. Cliquez sur Play : il neige !")
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
