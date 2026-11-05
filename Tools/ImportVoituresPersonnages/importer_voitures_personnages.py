"""SCRIPT A LANCER DANS UNREAL : File > Execute Python Script... > ce fichier.

Importe les voitures et les personnages prepares a partir de vos fichiers .glb
(Content/Voitures et Content/Personnages) :
  - les voitures      -> /Game/Voitures/<nom>/      (Static Mesh, avec LOD et vitres transparentes)
  - les personnages   -> /Game/Personnages/<nom>/   (Skeletal Mesh + animations, ou statues)
Les FBX se trouvent a cote de ce script, dans Tools/ImportVoituresPersonnages/ (modeles.zip.part00, part01... reassembles automatiquement).
Ne touche a aucun niveau ni a aucun asset existant. Peut etre relance sans risque
(il remplace seulement ce qu'il a lui-meme importe).
Journal : Tools/out/import_voitures_personnages.log
Ensuite, relancez Tools/installer_vie_urbaine.py pour que la circulation et les passants
prennent les nouveaux modeles.
"""
import os, traceback
import unreal

PROJET = unreal.SystemLibrary.get_project_directory()
RACINE = os.path.join(PROJET, "Tools", "ImportVoituresPersonnages")
_LOG = os.path.join(PROJET, "Tools", "out", "import_voitures_personnages.log")
os.makedirs(os.path.dirname(_LOG), exist_ok=True)
open(_LOG, "w").write("")

MOTS_VITRE = ("glass", "window", "vitre", "windshield", "windscreen", "verre")


def log(msg):
    unreal.log(str(msg))
    open(_LOG, "a", encoding="utf-8").write(str(msg) + "\n")


def options_statique():
    ui = unreal.FbxImportUI()
    ui.set_editor_property("automated_import_should_detect_type", False)
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("import_materials", True)
    ui.set_editor_property("import_textures", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    d = ui.get_editor_property("static_mesh_import_data")
    d.set_editor_property("combine_meshes", True)
    d.set_editor_property("auto_generate_collision", True)
    d.set_editor_property("generate_lightmap_u_vs", True)
    d.set_editor_property("remove_degenerates", True)
    return ui


def options_squelette():
    ui = unreal.FbxImportUI()
    ui.set_editor_property("automated_import_should_detect_type", False)
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("import_animations", True)
    ui.set_editor_property("import_materials", True)
    ui.set_editor_property("import_textures", True)
    ui.set_editor_property("create_physics_asset", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    d = ui.get_editor_property("skeletal_mesh_import_data")
    d.set_editor_property("import_morph_targets", False)
    return ui


def importer(fbx, dossier, nom, squelette):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", fbx)
    t.set_editor_property("destination_path", dossier)
    t.set_editor_property("destination_name", nom)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    t.set_editor_property("options", options_squelette() if squelette else options_statique())
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    return list(t.get_editor_property("imported_object_paths") or [])


def ajouter_lods(chemin):
    mesh = unreal.EditorAssetLibrary.load_asset(chemin)
    if not isinstance(mesh, unreal.StaticMesh):
        return
    reglages = []
    for pct, ecran in ((1.0, 1.0), (0.5, 0.35), (0.2, 0.15), (0.06, 0.05)):
        r = unreal.EditorScriptingMeshReductionSettings()
        r.set_editor_property("percent_triangles", pct)
        r.set_editor_property("screen_size", ecran)
        reglages.append(r)
    o = unreal.EditorScriptingMeshReductionOptions()
    o.set_editor_property("auto_compute_lod_screen_size", False)
    o.set_editor_property("reduction_settings", reglages)
    unreal.EditorStaticMeshLibrary.set_lods(mesh, o)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)


def vitres_transparentes(dossier):
    for chemin in unreal.EditorAssetLibrary.list_assets(dossier, recursive=True):
        nom = chemin.split(".")[-1].lower()
        if not any(m in nom for m in MOTS_VITRE):
            continue
        mat = unreal.EditorAssetLibrary.load_asset(chemin)
        if not isinstance(mat, unreal.Material):
            continue
        if mat.get_editor_property("blend_mode") == unreal.BlendMode.BLEND_TRANSLUCENT:
            continue
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        c = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 400)
        c.set_editor_property("r", 0.35)
        unreal.MaterialEditingLibrary.connect_material_property(c, "", unreal.MaterialProperty.MP_OPACITY)
        unreal.MaterialEditingLibrary.recompile_material(mat)
        unreal.EditorAssetLibrary.save_loaded_asset(mat)
        log("   vitre rendue transparente : %s" % chemin)


def deballer():
    """Les FBX arrivent dans modeles.zip : on le decompresse la premiere fois."""
    import zipfile
    archive = os.path.join(RACINE, "modeles.zip")
    morceaux = sorted(f for f in os.listdir(RACINE) if f.startswith("modeles.zip.part"))
    if morceaux and not os.path.isfile(archive):
        log("Assemblage de %d morceaux..." % len(morceaux))
        with open(archive, "wb") as sortie:
            for m in morceaux:
                with open(os.path.join(RACINE, m), "rb") as e:
                    sortie.write(e.read())
    if os.path.isfile(archive) and not os.path.isdir(os.path.join(RACINE, "Voitures")):
        log("Decompression de modeles.zip...")
        with zipfile.ZipFile(archive) as z:
            z.extractall(RACINE)


def run():
    deballer()
    if not os.path.isdir(RACINE):
        log("ABANDON : dossier introuvable %s" % RACINE)
        return
    total = 0
    for categorie in ("Voitures", "Personnages"):
        base = os.path.join(RACINE, categorie)
        if not os.path.isdir(base):
            continue
        for nom in sorted(os.listdir(base)):
            fbx = os.path.join(base, nom, nom + ".fbx")
            if not os.path.isfile(fbx):
                continue
            dossier = "/Game/%s/%s" % (categorie, nom)
            squelette = nom.startswith("SK_")
            log("Import %s -> %s" % (nom, dossier))
            try:
                chemins = importer(fbx, dossier, nom, squelette)
                for c in chemins:
                    log("   + %s" % c)
                if not chemins:
                    log("   ATTENTION : rien d'importe (voir Output Log)")
                    continue
                total += 1
                if not squelette:
                    ajouter_lods("%s/%s" % (dossier, nom))
                if categorie == "Voitures":
                    vitres_transparentes(dossier)
            except Exception:
                log("   ERREUR :\n" + traceback.format_exc())
    log("TERMINE : %d modele(s) importe(s). Relancez maintenant Tools/installer_vie_urbaine.py." % total)


with unreal.ScopedSlowTask(1, "Import des voitures et personnages de Chelles...") as tache:
    tache.make_dialog(True)
    run()
