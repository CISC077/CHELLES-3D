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
# CHELLES 3D - Voitures garees DV_Car_Horo + Bugatti de Rachid (25/11/2026)
# Lance par INSTALLER_VOITURES_HORO.bat (Unreal ferme). Lignes du journal : [Horo]
#  1. Config\DefaultInput.ini : touches du pack gardees localement (C = camera, Inser = ordinateur de bord)
#     Les autres actions (phares, saut, reinitialiser, clignotants, warnings) sont lues par le plugin ChellesVolVoiture
#     (touches dans Plugins\ChellesVolVoiture\VolVoiture.txt) et rejouees chez tous les joueurs.
#  2. Config\DefaultGame.ini : dossiers DV_Car_Horo et Chelles/Bugatti toujours inclus dans le jeu exporte
#  3. Bugatti : textures + materiau M_Bugatti + 5 meshes (carrosserie, 4 roues) allegees (LOD) dans /Game/Chelles/Bugatti
#  4. BP_Bugatti_Chelles = copie (Blueprint enfant) de BP_Car_Hatchback_Horo : meme conduite, apparence changee par le plugin
# Copies avant modification : *.avant_horo
import os, shutil, io
import unreal

PROJET = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJET, "Tools", "VoituresHoro", "Bugatti")
DEST = "/Game/Chelles/Bugatti"
HORO = "/Game/DV_Car_Horo/Blueprints/BP_Car_Hatchback_Horo"
ERREURS = []


def log(texte):
    unreal.log("[Horo] " + texte)
    print("[Horo] " + texte)


# ---------------------------------------------------------------- fichiers ini
def lire_ini(chemin):
    brut = open(chemin, "rb").read()
    if brut.startswith(b"\xff\xfe") or brut.startswith(b"\xfe\xff"):
        return brut.decode("utf-16"), "utf-16"
    if brut.startswith(b"\xef\xbb\xbf"):
        return brut[3:].decode("utf-8"), "utf-8-sig"
    try:
        return brut.decode("utf-8"), "utf-8"
    except UnicodeDecodeError:
        return brut.decode("cp1252"), "cp1252"


def ajouter_lignes(chemin, section, debut_ligne, lignes):
    """Ajoute les lignes absentes dans la section, apres la derniere ligne qui commence par debut_ligne."""
    texte, codage = lire_ini(chemin)
    fin = "\r\n" if "\r\n" in texte else "\n"
    rangees = texte.split(fin)
    a_ajouter = [l for l in lignes if l not in rangees]
    if not a_ajouter:
        log("%s : deja a jour" % os.path.basename(chemin))
        return
    if not os.path.exists(chemin + ".avant_horo"):
        shutil.copy2(chemin, chemin + ".avant_horo")
    try:
        i_section = rangees.index(section)
    except ValueError:
        rangees += ["", section]
        i_section = len(rangees) - 1
    i_fin = len(rangees)
    for j in range(i_section + 1, len(rangees)):
        if rangees[j].startswith("["):
            i_fin = j
            break
    place = i_section + 1
    for j in range(i_section + 1, i_fin):
        if rangees[j].startswith(debut_ligne) and rangees[j].rstrip().endswith(")"):
            place = j + 1
    rangees[place:place] = a_ajouter
    with io.open(chemin, "w", encoding=codage, newline="") as f:
        f.write(fin.join(rangees))
    log("%s : %d ligne(s) ajoutee(s)" % (os.path.basename(chemin), len(a_ajouter)))


def touche(action, cle):
    return '+ActionMappings=(ActionName="%s",bShift=False,bCtrl=False,bAlt=False,bCmd=False,Key=%s)' % (action, cle)


try:
    ajouter_lignes(os.path.join(PROJET, "Config", "DefaultInput.ini"), "[/Script/Engine.InputSettings]", "+ActionMappings=",
                   [touche("Camera_toggle", "C"), touche("Cycle_onboard_computer", "Insert")])
    ajouter_lignes(os.path.join(PROJET, "Config", "DefaultGame.ini"), "[/Script/UnrealEd.ProjectPackagingSettings]",
                   "+DirectoriesToAlwaysCook=",
                   ['+DirectoriesToAlwaysCook=(Path="/Game/DV_Car_Horo/Blueprints")',
                    '+DirectoriesToAlwaysCook=(Path="/Game/DV_Car_Horo/UI")',
                    '+DirectoriesToAlwaysCook=(Path="/Game/Chelles/Bugatti")'])
except Exception as e:
    ERREURS.append("ini : %s" % e)
    log("ERREUR ini : %s" % e)

# ---------------------------------------------------------------- Bugatti
outils = unreal.AssetToolsHelpers.get_asset_tools()
bibli = unreal.EditorAssetLibrary


def importer(fichier, nom, options=None):
    tache = unreal.AssetImportTask()
    tache.set_editor_property("filename", fichier)
    tache.set_editor_property("destination_path", DEST)
    tache.set_editor_property("destination_name", nom)
    tache.set_editor_property("replace_existing", True)
    tache.set_editor_property("automated", True)
    tache.set_editor_property("save", True)
    if options is not None:
        tache.set_editor_property("options", options)
    outils.import_asset_tasks([tache])
    return unreal.load_asset(DEST + "/" + nom)


def importer_texture(fichier, nom, normale=False, couleur=True):
    tex = importer(os.path.join(SOURCE, fichier), nom)
    if tex is None:
        raise RuntimeError("texture %s non importee" % fichier)
    tex.set_editor_property("srgb", couleur)
    if normale:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    bibli.save_loaded_asset(tex)
    return tex


def creer_materiau(couleur, normale, metal, rugosite):
    chemin = DEST + "/M_Bugatti"
    if bibli.does_asset_exist(chemin):
        bibli.delete_asset(chemin)
    mat = outils.create_asset("M_Bugatti", DEST, unreal.Material, unreal.MaterialFactoryNew())
    mel = unreal.MaterialEditingLibrary
    def echantillon(tex, x, y, type_):
        e = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, x, y)
        e.set_editor_property("texture", tex)
        e.set_editor_property("sampler_type", type_)
        return e
    c = echantillon(couleur, -400, -200, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    mel.connect_material_property(c, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    n = echantillon(normale, -400, 100, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    m = echantillon(metal, -400, 400, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    mel.connect_material_property(m, "R", unreal.MaterialProperty.MP_METALLIC)
    r = echantillon(rugosite, -400, 700, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    mel.connect_material_property(r, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    bibli.save_loaded_asset(mat)
    return mat


def options_fbx():
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    d = ui.get_editor_property("static_mesh_import_data")
    d.set_editor_property("combine_meshes", True)
    d.set_editor_property("generate_lightmap_u_vs", False)
    d.set_editor_property("auto_generate_collision", False)
    d.set_editor_property("remove_degenerates", True)
    d.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return ui


def poser_materiau(mesh, mat):
    try:
        mesh.set_material(0, mat)
        return
    except Exception:
        pass
    mats = mesh.get_editor_property("static_materials")
    if mats:
        mats[0].set_editor_property("material_interface", mat)
        mesh.set_editor_property("static_materials", mats)


def alleger(mesh, pourcents):
    # LOD0 allege aussi (la Bugatti fait 500 000 triangles) puis 2 niveaux de loin
    opts = unreal.EditorScriptingMeshReductionOptions()
    opts.set_editor_property("auto_compute_lod_screen_size", False)
    reglages = []
    for pct, ecran in pourcents:
        r = unreal.EditorScriptingMeshReductionSettings()
        r.set_editor_property("percent_triangles", pct)
        r.set_editor_property("screen_size", ecran)
        reglages.append(r)
    opts.set_editor_property("reduction_settings", reglages)
    unreal.EditorStaticMeshLibrary.set_lods(mesh, opts)


try:
    if not os.path.isfile(os.path.join(SOURCE, "SM_Bugatti_Carrosserie.fbx")):
        raise RuntimeError("fichiers de la Bugatti absents de %s" % SOURCE)
    unreal.EditorAssetLibrary.make_directory(DEST)
    log("Textures...")
    t_couleur = importer_texture("texture_diffuse.png", "T_Bugatti_Couleur")
    t_normale = importer_texture("texture_normal.png", "T_Bugatti_Normale", normale=True, couleur=False)
    t_metal = importer_texture("texture_metallic.png", "T_Bugatti_Metal", couleur=False)
    t_rugo = importer_texture("texture_roughness.png", "T_Bugatti_Rugosite", couleur=False)
    log("Materiau M_Bugatti...")
    mat = creer_materiau(t_couleur, t_normale, t_metal, t_rugo)
    for nom, pourcents in (("SM_Bugatti_Carrosserie", [(0.15, 1.0), (0.05, 0.35), (0.015, 0.12)]),
                           ("SM_Bugatti_Roue_AVG", [(0.15, 1.0), (0.04, 0.2)]),
                           ("SM_Bugatti_Roue_AVD", [(0.15, 1.0), (0.04, 0.2)]),
                           ("SM_Bugatti_Roue_ARG", [(0.18, 1.0), (0.05, 0.2)]),
                           ("SM_Bugatti_Roue_ARD", [(0.18, 1.0), (0.05, 0.2)])):
        log("Import %s (1 a 3 minutes pour la carrosserie)..." % nom)
        mesh = importer(os.path.join(SOURCE, nom + ".fbx"), nom, options_fbx())
        if mesh is None:
            raise RuntimeError("%s non importe" % nom)
        poser_materiau(mesh, mat)
        try:
            alleger(mesh, pourcents)
        except Exception as e:
            log("allegement de %s impossible (%s) : garde tel quel" % (nom, e))
        bibli.save_loaded_asset(mesh)
        boite = mesh.get_bounding_box()
        log("%s : %s sommets (LOD0), taille %s" % (nom, unreal.EditorStaticMeshLibrary.get_number_verts(mesh, 0),
                                                       boite.max - boite.min))
except Exception as e:
    ERREURS.append("Bugatti : %s" % e)
    log("ERREUR Bugatti : %s" % e)

# ---------------------------------------------------------------- Blueprint Bugatti
try:
    parent = bibli.load_blueprint_class(HORO)
    if parent is None:
        raise RuntimeError("%s introuvable" % HORO)
    chemin_bp = DEST + "/BP_Bugatti_Chelles"
    if bibli.does_asset_exist(chemin_bp):
        log("BP_Bugatti_Chelles existe deja")
    else:
        fab = unreal.BlueprintFactory()
        fab.set_editor_property("parent_class", parent)
        bp = outils.create_asset("BP_Bugatti_Chelles", DEST, unreal.Blueprint, fab)
        if bp is None:
            raise RuntimeError("creation du Blueprint refusee")
        bibli.save_loaded_asset(bp)
        log("BP_Bugatti_Chelles cree (enfant de BP_Car_Hatchback_Horo)")
except Exception as e:
    ERREURS.append("Blueprint : %s" % e)
    log("ERREUR Blueprint : %s" % e)

bibli.save_directory(DEST, only_if_is_dirty=True, recursive=True)
etat = "OK" if not ERREURS else "ERREUR : " + " ; ".join(ERREURS)
open(os.path.join(PROJET, "Tools", "VoituresHoro", "etat.txt"), "w").write(etat)
log("FIN : " + etat)
