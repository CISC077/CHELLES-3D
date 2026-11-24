# -*- coding: utf-8 -*-
# CHELLES 3D - personnages jouables de MERIDJA : LENA, SOFIA, LUCIA... (2026-11-24)
#
# A lancer dans Unreal : menu File > Execute Python Script... > Tools\Personnages\creer_personnages.py
# On peut le relancer sans risque : ce qui est deja fait est garde.
#
# Pour chaque personnage de la liste PERSONNAGES :
#  1. importe le corps AVEC SON SQUELETTE (fichier Mixamo du dossier Animations du Bureau)
#     -> /Game/Personnages/<DOSSIER>/Joueur/SK_<Nom>  (+ squelette, physique)
#  2. lui met ses couleurs (materiau texture_Mat du projet, sinon M_<Nom> cree avec texture_diffuse.png
#     et texture_normal.png du dossier Textures du Bureau) et l'ENREGISTRE (24/11 : elles etaient perdues)
#  3. importe ses animations Mixamo -> /Game/Personnages/<DOSSIER>/Joueur/Anims/A_<Nom>_<Etat>_Anim
#     (et supprime les copies du corps qu'Unreal cree en plus a chaque animation : 64 Mo chacune)
#  4. regle sa taille dans Plugins\ChellesWraith\Wraith.txt (ligne Heros=<Nom>)
#
# Les modeles importes avant dans /Game/Personnages/<DOSSIER> (base, lod...) ne sont pas modifies.
#
# NOUVEAU PERSONNAGE : ajouter un bloc dans PERSONNAGES, une ligne Heros= et Skin= dans Wraith.txt,
# et une ligne dans PersosWraithInfos (Plugins\ChellesMenu\...\HabillageChelles.cpp) pour le bouton du menu.

import os
import unreal

BUREAU = r"C:\Users\CISCO\Desktop"

PERSONNAGES = [
    {
        "nom": "Lena", "dossier": "LENA", "taille_cm": 175.0,
        "fbx": os.path.join(BUREAU, "LENA3D", "Animations"),
        "corps": "base_basic_shaded.fbx",
        "materiau": "/Game/Personnages/LENA/texture_Mat",  # deja dans le projet (couleurs + relief)
        "textures": os.path.join(BUREAU, "LENA3D", "Textures"),
        "anims": [
            ("Walking.fbx", "Marche"),
            ("Fast Run.fbx", "Course"),
            ("Flying.fbx", "Vol"),
            ("Crouching.fbx", "Accroupi"),
            ("Hip Hop Dancing.fbx", "Danse"),
            ("Rumba Dancing.fbx", "Rumba"),
            ("Twist Dance.fbx", "Twist"),
            ("Brooklyn Uprock.fbx", "Uprock"),
            ("Flair.fbx", "Flair"),
            ("Dancing Twerk.fbx", "Twerk"),
        ],
    },
    {
        "nom": "Sofia", "dossier": "SOFIA", "taille_cm": 170.0,
        "fbx": os.path.join(BUREAU, "SOFIA 3D", "Animations"),
        "corps": "lod_basic_shaded.fbx",
        "materiau": "",  # pas encore dans le projet : cree avec les images du dossier "textures"
        "textures": os.path.join(BUREAU, "SOFIA 3D", "Textures"),
        "anims": [
            ("Walking.fbx", "Marche"),
            ("Fast Run.fbx", "Course"),
            ("Flying.fbx", "Vol"),
            ("Crouching.fbx", "Accroupi"),
            ("Jump2.fbx", "Saut"),
            ("Rumba Dancing.fbx", "Danse"),
            ("Northern Soul Floor Combo.fbx", "Soul"),
            ("Flair.fbx", "Flair"),
        ],
    },
    {
        "nom": "Lucia", "dossier": "LUCIA", "taille_cm": 170.0,
        "fbx": os.path.join(BUREAU, "LUCIA 3D", "Animations"),
        "corps": "lod_basic_shaded.fbx",
        "materiau": "/Game/Personnages/LUCIA/texture_Mat",  # deja dans le projet (couleurs + relief)
        "textures": os.path.join(BUREAU, "LUCIA 3D", "Textures"),
        "anims": [
            ("Walking.fbx", "Marche"),
            ("Fast Run.fbx", "Course"),
            ("Flying.fbx", "Vol"),
            ("Crouching.fbx", "Accroupi"),
            ("Jump.fbx", "Saut"),
            ("Hip Hop Dancing.fbx", "Danse"),
        ],
    },
]

EAL = unreal.EditorAssetLibrary
resultats = []
TRAVAIL = {"import": False}  # vrai si ce passage a importe quelque chose (le menage se fera au passage suivant)


def dire(texte):
    unreal.log("[Personnages] " + texte)


def importer(fichier, destination, nom, options):
    tache = unreal.AssetImportTask()
    tache.set_editor_property("filename", fichier)
    tache.set_editor_property("destination_path", destination)
    tache.set_editor_property("destination_name", nom)
    tache.set_editor_property("automated", True)
    tache.set_editor_property("replace_existing", True)
    tache.set_editor_property("save", True)
    tache.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([tache])
    return list(tache.get_editor_property("imported_object_paths") or [])


def nom_court(chemin):
    return chemin.split("/")[-1].split(".")[0]


def classe(chemin):
    donnees = EAL.find_asset_data(chemin)
    try:
        return str(donnees.asset_class)
    except Exception:
        return ""


def sauver(objet):
    """Enregistre tout de suite (meme si Unreal ne le croit pas modifie) : si Unreal plante ensuite, rien n'est perdu."""
    if objet:
        try:
            EAL.save_loaded_asset(objet, False)
        except Exception:
            pass


def memoire():
    try:
        unreal.SystemLibrary.collect_garbage()
    except Exception:
        pass


def squelette_de(anim):
    try:
        sq = anim.get_editor_property("skeleton")
        return sq.get_path_name() if sq else ""
    except Exception:
        return ""


def ranger_animations(dossier_anims, squelette):
    """Dans le dossier Anims, on ne garde que les animations du squelette du personnage.
    On supprime : les copies du corps (SkeletalMesh / PhysicsAsset / Skeleton) creees par les anciens imports,
    et les animations rattachees a ces copies (elles ne marchent pas sur le personnage)."""
    if not EAL.does_directory_exist(dossier_anims):
        return
    a_supprimer = []
    for chemin in EAL.list_assets(dossier_anims, recursive=False):
        chemin = chemin.split(".")[0]
        c = classe(chemin)
        if c in ("SkeletalMesh", "PhysicsAsset", "Skeleton"):
            a_supprimer.append(chemin)
        elif c == "AnimSequence":
            anim = EAL.load_asset(chemin)
            if not anim or squelette_de(anim) != squelette.get_path_name():
                a_supprimer.append(chemin)
    # les animations d'abord, puis les copies du corps
    a_supprimer.sort(key=lambda x: 0 if classe(x) == "AnimSequence" else 1)
    supprimes = 0
    for chemin in a_supprimer:
        if EAL.does_asset_exist(chemin) and EAL.delete_asset(chemin):
            supprimes += 1
    if supprimes:
        dire("%d fichiers en trop supprimes dans %s" % (supprimes, dossier_anims))
    memoire()


def importer_texture(fichier, destination, nom):
    if not os.path.isfile(fichier):
        return None
    tache = unreal.AssetImportTask()
    tache.set_editor_property("filename", fichier)
    tache.set_editor_property("destination_path", destination)
    tache.set_editor_property("destination_name", nom)
    tache.set_editor_property("automated", True)
    tache.set_editor_property("replace_existing", True)
    tache.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([tache])
    return EAL.load_asset("%s/%s" % (destination, nom))


def creer_materiau(p):
    """M_<Nom> : texture_diffuse (couleurs) + texture_normal (relief) du dossier Textures du Bureau."""
    nom, dossier = p["nom"], p["dossier"]
    destination = "/Game/Personnages/%s/Joueur" % dossier
    chemin = "%s/M_%s" % (destination, nom)
    if EAL.does_asset_exist(chemin):
        return EAL.load_asset(chemin)
    dossier_tex = p.get("textures", "")
    couleurs = EAL.load_asset("%s/T_%s_Couleurs" % (destination, nom)) or \
        importer_texture(os.path.join(dossier_tex, "texture_diffuse.png"), destination, "T_%s_Couleurs" % nom)
    if not couleurs:
        unreal.log_warning("[Personnages] %s : pas de texture_diffuse.png dans %s" % (nom, dossier_tex))
        return None
    relief = EAL.load_asset("%s/T_%s_Relief" % (destination, nom)) or \
        importer_texture(os.path.join(dossier_tex, "texture_normal.png"), destination, "T_%s_Relief" % nom)
    MEL = unreal.MaterialEditingLibrary
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_" + nom, destination, unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        return None
    ex = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, -100)
    ex.set_editor_property("texture", couleurs)
    MEL.connect_material_property(ex, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    if relief:
        try:
            relief.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            relief.set_editor_property("srgb", False)
            sauver(relief)
        except Exception:
            pass
        exn = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, 200)
        exn.set_editor_property("texture", relief)
        exn.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        MEL.connect_material_property(exn, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mat.set_editor_property("two_sided", False)
    MEL.recompile_material(mat)
    sauver(mat)
    dire("%s : materiau M_%s cree avec ses textures" % (nom, nom))
    return mat


def effacer_dossier(dossier):
    """Efface tout ce qu'il y a dans un dossier (animations d'abord)."""
    if not EAL.does_directory_exist(dossier):
        return 0
    chemins = [c.split(".")[0] for c in EAL.list_assets(dossier, recursive=True)]
    chemins.sort(key=lambda x: 0 if classe(x) == "AnimSequence" else (2 if classe(x) == "Skeleton" else 1))
    n = 0
    for chemin in chemins:
        if EAL.does_asset_exist(chemin) and EAL.delete_asset(chemin):
            n += 1
    return n


def regler_taille(nom, taille):
    reglages = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_plugins_dir()),
                            "ChellesWraith", "Wraith.txt")
    if not os.path.isfile(reglages):
        return
    with open(reglages, "r", encoding="latin-1", newline="") as f:
        lignes = f.read().split("\n")
    change = False
    for i, ligne in enumerate(lignes):
        if ligne.startswith("Heros=" + nom + "|"):
            fin = "\r" if ligne.endswith("\r") else ""
            morceaux = ligne.rstrip("\r").split("|")
            while len(morceaux) < 11:
                morceaux.append("")
            morceaux[9] = str(taille)
            lignes[i] = "|".join(morceaux) + fin
            change = True
    if change:
        with open(reglages, "w", encoding="latin-1", newline="") as f:
            f.write("\n".join(lignes))
        dire("Wraith.txt : taille de %s = %s" % (nom, taille))
    else:
        unreal.log_warning("[Personnages] Pas de ligne Heros=%s dans Wraith.txt." % nom)


def creer(p):
    nom, dossier = p["nom"], p["dossier"]
    destination = "/Game/Personnages/%s/Joueur" % dossier
    dossier_anims = destination + "/Anims"
    chemin_corps = "%s/SK_%s" % (destination, nom)

    # ---- 0. corps abime (squelette jamais enregistre : Unreal avait plante pendant l'import du 24/11) -> on refait
    if EAL.does_asset_exist(chemin_corps):
        ancien = EAL.load_asset(chemin_corps)
        if not ancien or not isinstance(ancien, unreal.SkeletalMesh) or not ancien.get_editor_property("skeleton"):
            ancien = None
            memoire()
            n = effacer_dossier(destination)
            dire("%s : corps sans squelette, on recommence (%d fichiers effaces)" % (nom, n))
            memoire()

    # ---- 1. corps avec squelette
    if not EAL.does_asset_exist(chemin_corps):
        fbx = os.path.join(p["fbx"], p["corps"])
        if not os.path.isfile(fbx):
            unreal.log_error("[Personnages] %s : fichier introuvable %s" % (nom, fbx))
            return "%s : fichier %s introuvable" % (nom, fbx)
        ui = unreal.FbxImportUI()
        ui.set_editor_property("automated_import_should_detect_type", False)
        ui.set_editor_property("original_import_type", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_as_skeletal", True)
        ui.set_editor_property("import_animations", False)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ui.set_editor_property("create_physics_asset", True)
        ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
        dire("%s : import du corps %s" % (nom, fbx))
        TRAVAIL["import"] = True
        importer(fbx, destination, "SK_" + nom, ui)
    corps = EAL.load_asset(chemin_corps)
    if not corps or not isinstance(corps, unreal.SkeletalMesh):
        unreal.log_error("[Personnages] %s : l'import du corps a echoue (voir Output Log)." % nom)
        return "%s : import du corps rate" % nom
    squelette = corps.get_editor_property("skeleton")
    if not squelette:
        unreal.log_error("[Personnages] %s : le corps n'a pas de squelette." % nom)
        return "%s : corps sans squelette" % nom
    # squelette et physique enregistres TOUT DE SUITE
    sauver(squelette)
    try:
        sauver(corps.get_editor_property("physics_asset"))
    except Exception:
        pass

    # ---- 2. couleurs
    materiau = EAL.load_asset(p["materiau"]) if p["materiau"] and EAL.does_asset_exist(p["materiau"]) else None
    if not materiau:
        TRAVAIL["import"] = True
        materiau = creer_materiau(p)
    if materiau:
        mats = corps.get_editor_property("materials")
        for i in range(len(mats)):
            m = mats[i]
            m.set_editor_property("material_interface", materiau)
            mats[i] = m  # en Python UE, mats[i] est une COPIE : il faut la remettre
        corps.set_editor_property("materials", mats)
        dire("%s : couleurs %s" % (nom, materiau.get_path_name()))
    else:
        unreal.log_warning("[Personnages] %s : pas de materiau, %s restera gris(e)." % (nom, nom))
    sauver(corps)
    memoire()

    # ---- 3. animations (seulement l'animation : plus de copie du corps de 130 Mo a chaque fois)
    ranger_animations(dossier_anims, squelette)
    faites = 0
    for fichier, etat in p["anims"]:
        voulu = "%s/A_%s_%s_Anim" % (dossier_anims, nom, etat)
        if EAL.does_asset_exist(voulu):
            faites += 1
            continue
        fbx = os.path.join(p["fbx"], fichier)
        if not os.path.isfile(fbx):
            unreal.log_warning("[Personnages] %s : animation absente %s" % (nom, fbx))
            continue
        ua = unreal.FbxImportUI()
        ua.set_editor_property("automated_import_should_detect_type", False)
        ua.set_editor_property("original_import_type", unreal.FBXImportType.FBXIT_ANIMATION)
        ua.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
        ua.set_editor_property("import_mesh", False)
        ua.set_editor_property("import_as_skeletal", True)
        ua.set_editor_property("import_animations", True)
        ua.set_editor_property("import_materials", False)
        ua.set_editor_property("import_textures", False)
        ua.set_editor_property("create_physics_asset", False)
        ua.set_editor_property("skeleton", squelette)
        ua.anim_sequence_import_data.set_editor_property(
            "animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
        TRAVAIL["import"] = True
        importer(fbx, dossier_anims, "A_%s_%s_Anim" % (nom, etat), ua)
        if not EAL.does_asset_exist(voulu):
            # selon la version, Unreal ajoute ou non "_Anim" au nom
            for autre in ("%s/A_%s_%s" % (dossier_anims, nom, etat), voulu + "_Anim"):
                if EAL.does_asset_exist(autre) and classe(autre) == "AnimSequence":
                    EAL.rename_asset(autre, voulu)
                    break
        anim = EAL.load_asset(voulu) if EAL.does_asset_exist(voulu) else None
        if anim and squelette_de(anim) == squelette.get_path_name():
            sauver(anim)
            faites += 1
        else:
            unreal.log_warning("[Personnages] %s : animation non creee (%s)" % (nom, fichier))
        memoire()
    ranger_animations(dossier_anims, squelette)
    dire("%s : %d animations sur %d." % (nom, faites, len(p["anims"])))

    # ---- 4. taille
    try:
        hauteur = corps.get_bounds().box_extent.z * 2.0
    except Exception:
        hauteur = 0.0
    if hauteur > 10.0:
        taille = round(max(0.2, min(5.0, p["taille_cm"] / hauteur)), 3)
        dire("%s mesure %.0f cm -> taille %s" % (nom, hauteur, taille))
        regler_taille(nom, taille)
    return "%s : pret(e), %d animations" % (nom.upper(), faites)


def alleger(p):
    """FLUIDITE : ajoute 3 versions simplifiees (LOD) du corps, utilisees de loin (autres joueurs, apercus).
    De pres rien ne change. Une seule fois par personnage (si le corps n'a qu'une version)."""
    nom, dossier = p["nom"], p["dossier"]
    chemin = "/Game/Personnages/%s/Joueur/SK_%s" % (dossier, nom)
    corps = EAL.load_asset(chemin) if EAL.does_asset_exist(chemin) else None
    if not corps:
        return ""
    try:
        lib = unreal.EditorSkeletalMeshLibrary
        if lib.get_lod_count(corps) > 1:
            return ""
        dire("%s : creation des versions allegees (LOD)..." % nom)
        if lib.regenerate_lod(corps, 4, False, False):
            sauver(corps)
            return "%s : allege(e) (4 niveaux de detail)" % nom.upper()
    except Exception as e:
        unreal.log_warning("[Personnages] %s : LOD non crees (%s)" % (nom, e))
    return ""


# Doublons de l'import fait a la main de LUCIA (24/11) : un corps de 129 Mo + squelette + physique par animation,
# inutilisables (chacun son squelette). Supprimes seulement quand LUCIA est prete (SK_Lucia + ses animations),
# et seulement s'ils ne sont utilises nulle part ailleurs (carte Rachid, blueprint...). Accord de MERIDJA le 24/11.
DOUBLONS = {
    "LUCIA": ["Walking", "Crouching", "Fast_Run", "Flying", "Flying_1_", "Flying_Kick",
              "Hip_Hop_Dancing", "Jump", "T-Pose", "lod_basic_shaded"],
}
SUFFIXES = ["", "_Anim", "_Skeleton", "_PhysicsAsset"]


def nettoyer_doublons(dossier, noms, nom_perso):
    base = "/Game/Personnages/" + dossier
    if not EAL.does_asset_exist("%s/Joueur/SK_%s" % (base, nom_perso)) or \
            not EAL.does_asset_exist("%s/Joueur/Anims/A_%s_Marche_Anim" % (base, nom_perso)):
        dire("%s pas encore prete : doublons gardes." % nom_perso)
        return ""
    candidats = []
    for n in noms:
        for suf in SUFFIXES:
            chemin = "%s/%s%s" % (base, n, suf)
            if EAL.does_asset_exist(chemin):
                candidats.append(chemin)
    paquets = set(candidats)
    gardes, supprimes = [], 0
    for chemin in candidats:
        try:
            refs = EAL.find_package_referencers_for_asset(chemin, False) or []
        except Exception:
            refs = []
        autres = [r for r in refs if r.split(".")[0] not in paquets]
        if autres:
            gardes.append("%s (utilise par %s)" % (chemin, autres[0]))
    if gardes:
        # Un doublon sert ailleurs : on ne supprime rien (ils dependent les uns des autres)
        for g in gardes:
            unreal.log_warning("[Personnages] Utilise ailleurs : " + g)
        texte = "%s : doublons gardes, %d sont utilises ailleurs (voir Output Log)" % (dossier, len(gardes))
        dire(texte)
        return texte
    # petits fichiers d'abord, puis les gros corps un par un en vidant la memoire entre chaque
    # (le 24/11 Unreal a plante, memoire pleine, en les effacant tous d'affilee)
    candidats.sort(key=lambda x: 1 if classe(x) == "SkeletalMesh" else 0)
    for chemin in candidats:
        if EAL.does_asset_exist(chemin) and EAL.delete_asset(chemin):
            supprimes += 1
        memoire()
    texte = "%s : %d doublons supprimes (environ 1,3 Go)" % (dossier, supprimes)
    dire(texte)
    return texte


for perso in PERSONNAGES:
    resultats.append(creer(perso))
    memoire()
EAL.save_directory("/Game/Personnages", only_if_is_dirty=True, recursive=True)
memoire()

if TRAVAIL["import"]:
    # Pour ne pas remplir la memoire : le menage et l'allegement se font au passage SUIVANT (rien a importer)
    resultats.append("\nRELANCEZ CE SCRIPT UNE 2e FOIS (apres avoir rouvert Unreal) :\n"
                     "il fera le menage des doublons et allegera les personnages (LOD).")
else:
    for perso in PERSONNAGES:
        t = alleger(perso)
        if t:
            resultats.append(t)
        memoire()
    for perso in PERSONNAGES:
        if perso["dossier"] in DOUBLONS:
            t = nettoyer_doublons(perso["dossier"], DOUBLONS[perso["dossier"]], perso["nom"])
            if t:
                resultats.append(t)
    EAL.save_directory("/Game/Personnages", only_if_is_dirty=True, recursive=True)

texte = "\n".join(resultats)
dire("TERMINE.\n" + texte)
try:
    unreal.EditorDialog.show_message(
        "CHELLES 3D - personnages",
        texte + "\n\nFermez Unreal, compilez ChellesWraith et ChellesMenu,\npuis JOUER > HABILLAGE.",
        unreal.AppMsgType.OK)
except Exception:
    pass
