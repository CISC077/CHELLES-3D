# -*- coding: utf-8 -*-
"""Cree le Blueprint /Game/ScifiGun/BP_Turet2026 : la tourelle qui tire toute seule sur les ennemis.

A lancer par double-clic sur Tools\\Tourelle\\INSTALLER_TOURELLE.bat (Unreal FERME),
ou dans l'editeur : File > Execute Python Script... > Tools/Tourelle/creer_bp_turet2026.py
(il faut que le plugin ChellesTourelle soit compile).

Ce que fait le script :
  1. trouve le modele de la tourelle dans /Game/ScifiGun (TuretStatic, ou le 1er modele dont le nom
     contient Turet / Turret) ;
  2. cree (ou met a jour) BP_Turet2026 dans /Game/ScifiGun, enfant de la classe C++ "Tourelle Chelles" ;
  3. mesure le modele pour placer l'axe de rotation et le bout du canon (d'ou partent les balles).

Rien n'est pose dans la carte : glissez BP_Turet2026 depuis le Content Browser (dossier ScifiGun)
la ou vous voulez. Les reglages sont visibles dans BP_Turet2026 > Class Defaults > Tourelle.
Journal : Tools/Tourelle/tourelle.log
"""
import os
import time
import traceback

import unreal

DOSSIER = "/Game/ScifiGun"
NOM_BP = "BP_Turet2026"
CLASSE_CPP = "/Script/ChellesTourelle.TourelleChelles"
MODELE_PREFERE = DOSSIER + "/TuretStatic"
HAUTEUR_VOULUE = (60.0, 400.0)   # si le modele est plus petit / plus grand, il est mis a 180 cm

eal = unreal.EditorAssetLibrary
outils = unreal.AssetToolsHelpers.get_asset_tools()
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ICI = os.path.join(PROJET, "Tools", "Tourelle")
JOURNAL = os.path.join(ICI, "tourelle.log")


def log(msg):
    unreal.log("[Tourelle] " + str(msg))
    try:
        os.makedirs(ICI, exist_ok=True)
        with open(JOURNAL, "a", encoding="utf-8") as f:
            f.write(time.strftime("%H:%M:%S ") + str(msg) + "\n")
    except Exception:
        pass


def regler(objet, nom, valeur):
    try:
        objet.set_editor_property(nom, valeur)
        return True
    except Exception as e:
        log("   ATTENTION : reglage %s impossible (%s)" % (nom, e))
        return False


def trouver_modele():
    if eal.does_asset_exist(MODELE_PREFERE):
        m = eal.load_asset(MODELE_PREFERE)
        if isinstance(m, (unreal.StaticMesh, unreal.SkeletalMesh)):
            return m
    trouves = []
    for chemin in eal.list_assets(DOSSIER, recursive=True, include_folder=False):
        nom = chemin.split("/")[-1].split(".")[0].lower()
        if "turet" in nom or "turret" in nom:
            a = eal.load_asset(chemin)
            if isinstance(a, (unreal.StaticMesh, unreal.SkeletalMesh)):
                trouves.append(a)
    # un StaticMesh d'abord (le plus simple a faire tourner)
    trouves.sort(key=lambda a: 0 if isinstance(a, unreal.StaticMesh) else 1)
    return trouves[0] if trouves else None


def mesurer(modele):
    """Rend (pivot, bout_du_canon, rotation, echelle) d'apres la boite du modele."""
    b = modele.get_bounds()
    o, e = b.origin, b.box_extent
    bas, haut = o.z - e.z, o.z + e.z
    hauteur = haut - bas
    log("   boite du modele : centre (%.0f, %.0f, %.0f), taille %.0f x %.0f x %.0f cm"
        % (o.x, o.y, o.z, e.x * 2, e.y * 2, hauteur))

    echelle = 1.0
    if hauteur < HAUTEUR_VOULUE[0] or hauteur > HAUTEUR_VOULUE[1]:
        echelle = 180.0 / max(hauteur, 1.0)
        log("   hauteur %.0f cm peu probable : echelle %.2f (180 cm de haut)" % (hauteur, echelle))

    # Le canon est le grand cote horizontal ; il est du cote ou le modele depasse le plus de son centre
    if e.x >= e.y:
        sens = 1.0 if o.x >= 0 else -1.0
        rotation = 0.0 if sens > 0 else 180.0
        longueur = abs(o.x) + e.x
    else:
        sens = 1.0 if o.y >= 0 else -1.0
        rotation = -90.0 if sens > 0 else 90.0
        longueur = abs(o.y) + e.y
    pivot = unreal.Vector(0.0, 0.0, bas + hauteur * 0.62)
    bout = unreal.Vector(longueur * 0.95, 0.0, (bas + hauteur * 0.72) - pivot.z)
    log("   canon vers %s%s : rotation du modele %.0f, axe a %.0f cm, bout du canon a %.0f cm devant"
        % ("+" if sens > 0 else "-", "X" if e.x >= e.y else "Y", rotation, pivot.z, bout.x))
    return pivot, bout, rotation, echelle


def creer_bp(parent):
    chemin = DOSSIER + "/" + NOM_BP
    if eal.does_asset_exist(chemin):
        log("   %s existe deja : mis a jour" % NOM_BP)
        return eal.load_blueprint_class(chemin)
    fab = unreal.BlueprintFactory()
    fab.set_editor_property("parent_class", parent)
    bp = outils.create_asset(NOM_BP, DOSSIER, unreal.Blueprint, fab)
    if bp is None:
        raise RuntimeError("Creation de %s impossible." % chemin)
    eal.save_asset(chemin, False)
    log("   %s cree" % NOM_BP)
    return eal.load_blueprint_class(chemin)


def main():
    open(JOURNAL, "w").close()
    cls_cpp = unreal.load_class(None, CLASSE_CPP)
    if cls_cpp is None:
        log("ABANDON : le plugin ChellesTourelle n'est pas compile/active. "
            "Fermez Unreal et lancez Tools\\Tourelle\\INSTALLER_TOURELLE.bat")
        return
    modele = trouver_modele()
    if modele is None:
        log("ABANDON : aucun modele de tourelle dans %s (TuretStatic). Dans Unreal : Fichier > Tout enregistrer, puis relancez."
            % DOSSIER)
        return
    log("1/3 modele : %s (%s)" % (modele.get_path_name(), modele.get_class().get_name()))

    pivot, bout, rotation, echelle = mesurer(modele)

    cls = creer_bp(cls_cpp)
    cdo = unreal.get_default_object(cls)
    if isinstance(modele, unreal.SkeletalMesh):
        regler(cdo, "modele_squelette", modele)
        regler(cdo, "modele_canon", None)
    else:
        # un seul morceau : toute la tourelle tourne vers sa cible
        regler(cdo, "modele_canon", modele)
        regler(cdo, "modele_squelette", None)
    regler(cdo, "modele_pied", None)
    regler(cdo, "pivot_canon", pivot)
    regler(cdo, "bout_du_canon_local", bout)
    regler(cdo, "rotation_modele", rotation)
    regler(cdo, "echelle", echelle)
    eal.save_asset(DOSSIER + "/" + NOM_BP, False)
    log("2/3 %s/%s pret" % (DOSSIER, NOM_BP))
    log("3/3 TERMINE. Glissez BP_Turet2026 (dossier ScifiGun) dans la carte et lancez le jeu : "
        "elle tire toute seule sur les ennemis a moins de 30 m.")
    log("Si elle tire de dos : ouvrez BP_Turet2026 > Class Defaults > Tourelle > Rotation Modele, ajoutez 180.")


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
