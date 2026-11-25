# -*- coding: utf-8 -*-
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
    {
        # 25/11/2026 : BRENDA. Les fichiers du dossier Bureau\BRENDA 3D sont reconnus tout seuls (corps, marche,
        # course, vol, accroupi, saut, danses) et ses lignes Heros=/Skin=/Danses= sont ecrites dans Wraith.txt.
        "nom": "Brenda", "dossier": "BRENDA", "taille_cm": 170.0, "auto": True,
        "racine": os.path.join(BUREAU, "BRENDA 3D"),
        "fbx": os.path.join(BUREAU, "BRENDA 3D", "Animations"),
        "corps": "",
        "materiau": "",
        "textures": os.path.join(BUREAU, "BRENDA 3D", "Textures"),
        "anims": [],
    },
    {
        # 25/11/2026 : Mr.SHEMS, personnage VIP (code 2468 a taper dans le lobby, voir HabillageChelles.cpp).
        # Fichiers dans Bureau\SHEMS_3D ; dans Wraith.txt son nom est MR.SHEMS + une ligne VIP=MR.SHEMS
        # (la touche K le saute tant que le code n'a pas ete tape).
        "nom": "Shems", "dossier": "Shems", "taille_cm": 180.0, "auto": True,
        "affiche": "MR.SHEMS", "vip": True,
        "racine": os.path.join(BUREAU, "SHEMS_3D"),
        "fbx": os.path.join(BUREAU, "SHEMS_3D", "Animations"),
        "corps": "",
        "materiau": "",
        "textures": os.path.join(BUREAU, "SHEMS_3D", "Textures"),
        "anims": [],
    },
]

# ---------------------------------------------------------------- reconnaissance automatique (25/11, BRENDA)
CLES_ANIMS = [
    (("idle", "pose"), "Repos"),
    (("dying", "death", "die"), "Mort"),
    (("walk",), "Marche"),
    (("fast run", "fast_run", "running", "run"), "Course"),
    (("fly",), "Vol"),
    (("crouch",), "Accroupi"),
    (("jump",), "Saut"),
]
MOTS_DANSE = ("danc", "dance", "rumba", "flair", "twerk", "soul", "uprock", "samba", "salsa", "hip hop", "hiphop",
              "breakdance", "twist", "shuffle", "macarena", "gangnam", "swing", "tango", "bboy", "robot", "thriller",
              "belly", "chicken", "silly", "cheer", "victory", "excited")
NOMS_CORPS = ("lod_basic_shaded.fbx", "base_basic_shaded.fbx", "lod_basic_pbr.fbx", "base_basic_pbr.fbx",
              "lod_model.fbx", "base.fbx", "model.fbx", "t-pose.fbx", "t pose.fbx", "tpose.fbx")
# 25/11 (SHEMS) : marche a reculons, de cote, avec un fusil, accroupi qui se releve... = pas l'etat de base
MOTS_EXCLUS = ("backward", "rifle", "strafe", "strafing", "sneak", "to stand", "left", "right", "pistol", "gun")
# Gestes joues avec J apres les danses (boxe, bagarre...)
MOTS_GESTE = ("boxing", "fist fight", "fight", "punch", "kick", "taunt", "salute", "wave", "flex")
IMAGES_COULEURS = ("texture_diffuse", "diffuse", "basecolor", "base_color", "albedo", "color", "shaded", "texture_pbr")
IMAGES_RELIEF = ("texture_normal", "normal")


def _tous_les_fichiers(racine, extensions):
    trouves = []
    for dossier, _, fichiers in os.walk(racine):
        for f in fichiers:
            if os.path.splitext(f)[1].lower() in extensions:
                trouves.append(os.path.join(dossier, f))
    return sorted(trouves)


def _image(dossiers, mots):
    for mot in mots:
        for d in dossiers:
            if not os.path.isdir(d):
                continue
            for f in _tous_les_fichiers(d, (".png", ".jpg", ".jpeg", ".tga")):
                if mot in os.path.basename(f).lower():
                    return f
    return ""


def preparer_auto(p):
    """Remplit corps / anims / textures d'un personnage "auto" a partir de ce qu'il y a dans son dossier du Bureau."""
    racine = p["racine"]
    if not os.path.isdir(racine):
        return "%s : dossier %s introuvable" % (p["nom"], racine)
    # Seulement les fichiers DIRECTEMENT dans Animations (pas les sous-dossiers : ex. "Capoeira Pack", sans peau)
    def directs(d):
        if not os.path.isdir(d):
            return []
        return [os.path.join(d, f) for f in os.listdir(d)
                if os.path.isfile(os.path.join(d, f)) and f.lower().endswith(".fbx")]
    fbx = directs(p["fbx"]) or directs(racine)
    # "Walking (1).fbx", "Flair(1).fbx" = copies : apres l'original
    import re
    fbx.sort(key=lambda f: ("(" in os.path.basename(f), os.path.basename(f).lower()))
    if not fbx:
        return "%s : aucun fichier .fbx dans %s (exportez-le de Mixamo AVEC SKIN, en FBX)" % (p["nom"], racine)
    noms = dict((os.path.basename(f).lower(), f) for f in fbx)
    corps = ""
    for n in NOMS_CORPS:
        if n in noms:
            corps = noms[n]
            break
    if not corps:
        for f in fbx:
            b = os.path.basename(f).lower()
            if any(m in b for m in ("shaded", "pbr", "lod", "pose", "model", "base", p["nom"].lower())):
                corps = f
                break
    anims, pris, danses = [], set(), 0
    a_danser = []  # (0 = danse, 1 = geste, fichier) : les vraies danses d'abord (la 1re est jouee dans le menu)
    for f in fbx:
        if f == corps or os.path.basename(f).lower() in NOMS_CORPS or re.match(r"^(base|lod|model)\s*\(\d+\)", os.path.basename(f).lower()):
            continue
        b = os.path.splitext(os.path.basename(f))[0].lower()
        etat = ""
        if not any(x in b for x in MOTS_EXCLUS):
            for mots, e in CLES_ANIMS:
                if e not in pris and any(m in b for m in mots) and not any(d in b for d in MOTS_DANSE + MOTS_GESTE):
                    etat = e
                    break
        if not etat:
            if any(d in b for d in MOTS_DANSE):
                a_danser.append((0, f, b))
            elif any(d in b for d in MOTS_GESTE):
                a_danser.append((1, f, b))
            continue
        pris.add(etat)
        anims.append((f, etat))
    for _, f, b in sorted(a_danser, key=lambda x: x[0]):
        simple = re.sub(r"\s*\(\d+\)$", "", b).strip()
        if simple in pris:
            continue  # copie d'une danse deja prise
        pris.add(simple)
        danses += 1
        anims.append((f, "Danse" if danses == 1 else "Danse%d" % danses))
    if not corps:
        # Mixamo "avec skin" : chaque animation contient aussi le corps -> on prend la marche (ou la 1re)
        corps = next((f for f, e in anims if e == "Marche"), fbx[0])
        dire("%s : pas de fichier corps reconnu, corps pris dans %s" % (p["nom"], os.path.basename(corps)))
    p["corps"] = corps
    p["anims"] = anims
    p["textures"] = p["textures"] if os.path.isdir(p["textures"]) else racine
    dire("%s : corps %s, animations : %s" % (p["nom"], os.path.basename(corps), ", ".join(e for _, e in anims) or "aucune"))
    return ""


def ecrire_wraith(p):
    """Lignes Heros=/Skin=/Danses= du personnage dans Plugins/ChellesWraith/Wraith.txt (une seule fois)."""
    nom, dossier = p["nom"], p["dossier"]
    reglages = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_plugins_dir()),
                            "ChellesWraith", "Wraith.txt")
    if not os.path.isfile(reglages):
        return
    with open(reglages, "r", encoding="latin-1", newline="") as f:
        lignes = f.read().replace("\r\n", "\n").split("\n")
    while lignes and lignes[-1] == "":
        lignes.pop()
    base = "/Game/Personnages/%s/Joueur/Anims/A_%s_" % (dossier, nom)

    def anim(etat):
        chemin = "%s%s_Anim" % (base, etat)
        return "%s.%s" % (chemin, chemin.split("/")[-1]) if EAL.does_asset_exist(chemin) else ""

    marche = anim("Marche")
    affiche = p.get("affiche", nom.upper())
    # Repos : la pose (SHEMS) en boucle, sinon l'image 0 de l'accroupi ou de la marche
    repos = anim("Repos") or ((anim("Accroupi") or marche) + "@0" if (anim("Accroupi") or marche) else "")
    etats = [repos, marche, anim("Course") or marche, anim("Vol") or marche,
             anim("Accroupi"), anim("Saut") or anim("Vol")]
    heros = "Heros=%s|Anims:%s||%s||||||1|0" % (nom, ";".join(etats), anim("Mort") or anim("Accroupi"))
    danses = [anim(e) for _, e in p["anims"] if e.startswith("Danse")]
    danses = [d for d in danses if d]
    corps = "/Game/Personnages/%s/Joueur/SK_%s.SK_%s" % (dossier, nom, nom)
    change = False
    if not any(l.startswith("Heros=%s|" % nom) for l in lignes):
        pos = max([i + 1 for i, l in enumerate(lignes) if l.startswith("Heros=")] or [len(lignes)])
        lignes.insert(pos, heros)
        change = True
    if not any(l.upper().startswith("SKIN=%s|" % affiche.upper()) for l in lignes):
        pos = next((i for i, l in enumerate(lignes) if l.upper().startswith("SKIN=SOLDAT D'ORIGINE")), len(lignes))
        lignes.insert(pos, "Skin=%s|%s||1||%s" % (affiche.upper(), corps, nom))
        change = True
    if p.get("vip") and not any(l.upper().replace(" ", "") == "VIP=%s" % affiche.upper() for l in lignes):
        pos = next((i for i, l in enumerate(lignes) if l.upper().startswith("SKIN=%s|" % affiche.upper())), len(lignes))
        lignes.insert(pos, "VIP=%s" % affiche.upper())
        change = True
    if danses and not any(l.startswith("Danses=%s|" % nom) for l in lignes):
        pos = max([i + 1 for i, l in enumerate(lignes) if l.startswith("Danses=")] or [len(lignes)])
        lignes.insert(pos, "Danses=%s|%s" % (nom, ";".join(danses)))
        change = True
    if change:
        sauvegarde = reglages + ".avant_" + nom.lower()
        if not os.path.isfile(sauvegarde):
            import shutil
            shutil.copy2(reglages, sauvegarde)
        with open(reglages, "w", encoding="latin-1", newline="") as f:
            f.write("\r\n".join(lignes) + "\r\n")
        dire("Wraith.txt : %s ajoute(e) (%d danses, touche J)" % (affiche.upper(), len(danses)))

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
    fichier_couleurs = os.path.join(dossier_tex, "texture_diffuse.png")
    if not os.path.isfile(fichier_couleurs):
        fichier_couleurs = _image([dossier_tex, p.get("racine", "")], IMAGES_COULEURS) or fichier_couleurs
    fichier_relief = os.path.join(dossier_tex, "texture_normal.png")
    if not os.path.isfile(fichier_relief):
        fichier_relief = _image([dossier_tex, p.get("racine", "")], IMAGES_RELIEF) or fichier_relief
    # 25/11 : textures deja importees par Rachid dans /Game/Personnages/<DOSSIER> (BRENDA) : on les reprend
    deja = "/Game/Personnages/%s/" % dossier
    couleurs = EAL.load_asset("%s/T_%s_Couleurs" % (destination, nom)) or \
        (EAL.load_asset(deja + "texture_diffuse") if EAL.does_asset_exist(deja + "texture_diffuse") else None) or \
        importer_texture(fichier_couleurs, destination, "T_%s_Couleurs" % nom)
    if not couleurs:
        unreal.log_warning("[Personnages] %s : pas de texture_diffuse.png dans %s" % (nom, dossier_tex))
        return None
    relief = EAL.load_asset("%s/T_%s_Relief" % (destination, nom)) or \
        (EAL.load_asset(deja + "texture_normal") if EAL.does_asset_exist(deja + "texture_normal") else None) or \
        importer_texture(fichier_relief, destination, "T_%s_Relief" % nom)
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
    # 25/11 (SHEMS gris) : sans ce drapeau, en jeu Unreal remplace le materiau par le gris par defaut
    mat.set_editor_property("used_with_skeletal_mesh", True)
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
    if p.get("auto"):
        erreur = preparer_auto(p)
        if erreur:
            unreal.log_warning("[Personnages] " + erreur)
            return erreur
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
    if materiau and isinstance(materiau, unreal.Material) and not materiau.get_editor_property("used_with_skeletal_mesh"):
        # 25/11 : materiau deja cree sans "Used with Skeletal Mesh" (M_Shems) -> corrige une fois
        materiau.set_editor_property("used_with_skeletal_mesh", True)
        unreal.MaterialEditingLibrary.recompile_material(materiau)
        sauver(materiau)
        dire("%s : %s marque pour les corps animes (n'etait plus gris en jeu)" % (nom, materiau.get_name()))
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

    if p.get("auto"):
        ecrire_wraith(p)

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
    if os.environ.get("CHELLES_SANS_FENETRE"):
        raise RuntimeError("lance par CREER_PERSONNAGES.bat")
    unreal.EditorDialog.show_message(
        "CHELLES 3D - personnages",
        texte + "\n\nFermez Unreal, compilez ChellesWraith et ChellesMenu,\npuis JOUER > HABILLAGE.",
        unreal.AppMsgType.OK)
except Exception:
    pass
