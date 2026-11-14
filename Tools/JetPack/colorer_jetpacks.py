# -*- coding: utf-8 -*-
"""COULEURS DES JETPACKS (a lancer apres installer_jetpacks.py, ou tout seul).

Le materiau "Mask" du pack n'a pas de reglage de couleur lisible : ce script fabrique
notre propre materiau M_JetPack_Couleur (textures DualJet du pack + parametre Couleur)
dans /Game/FPS_Game/Blueprints/Bonus/JetPack_Materiau, et le donne a BP_JetPack.
Les BP_JetPack_Rouge/Bleu/... gardent leur couleur. Les JetPacks deja poses ne bougent pas.

Unreal ouvert : File > Execute Python Script > Tools/JetPack/colorer_jetpacks.py
Unreal ferme  : double-clic sur COLORER_JETPACKS.bat
Journal : Tools/JetPack/jetpack.log
"""
import os
import time
import traceback

import unreal

DOSSIER_BP = "/Game/FPS_Game/Blueprints/Bonus"
DOSSIER_MAT = DOSSIER_BP + "/JetPack_Materiau"
NOM_MAT = "M_JetPack_Couleur"
TEX = "/Game/Jet_Packs/Textures/DualJet/2K/Dual_Jet_Jetpack_2K_"
TEX_4K = "/Game/Jet_Packs/Textures/DualJet/Dual_Jet_Jetpack_4K_"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
JOURNAL = os.path.join(PROJET, "Tools", "JetPack", "jetpack.log")


def log(msg):
    unreal.log("[JetPack] " + str(msg))
    try:
        with open(JOURNAL, "a", encoding="utf-8") as f:
            f.write(time.strftime("%H:%M:%S ") + str(msg) + "\n")
    except Exception:
        pass


def texture(suffixe):
    for base in (TEX, TEX_4K):
        if eal.does_asset_exist(base + suffixe):
            return eal.load_asset(base + suffixe)
    log("   ATTENTION : texture %s introuvable" % suffixe)
    return None


def noeud(mat, classe, x, y):
    return mel.create_material_expression(mat, classe, x, y)


def echantillon(mat, tex, x, y, normal=False):
    n = noeud(mat, unreal.MaterialExpressionTextureSample, x, y)
    n.set_editor_property("texture", tex)
    if normal:
        n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    else:
        try:
            srgb = tex.get_editor_property("srgb")
        except Exception:
            srgb = True
        n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if srgb
                              else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    return n


def scalaire(mat, nom, valeur, x, y):
    n = noeud(mat, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", nom)
    n.set_editor_property("default_value", valeur)
    return n


def creer_materiau():
    chemin = DOSSIER_MAT + "/" + NOM_MAT
    if not eal.does_directory_exist(DOSSIER_MAT):
        eal.make_directory(DOSSIER_MAT)
    d, nrm, orm, masque = texture("D"), texture("N"), texture("ORM"), texture("Mask")
    if d is None:
        raise RuntimeError("Textures DualJet du pack introuvables.")

    if eal.does_asset_exist(chemin):
        mat = eal.load_asset(chemin)
        mel.delete_all_material_expressions(mat)  # on le refait a neuf
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            NOM_MAT, DOSSIER_MAT, unreal.Material, unreal.MaterialFactoryNew())
    c = mel.connect_material_expressions

    t_d = echantillon(mat, d, -1400, -200)
    # Teinte = Couleur x (gris de la texture x Eclat + Plancher) : meme le noir prend la couleur
    desat = noeud(mat, unreal.MaterialExpressionDesaturation, -1100, -300)
    c(t_d, "RGB", desat, "Input")
    eclat = scalaire(mat, "Eclat", 1.6, -1100, -150)
    fois_eclat = noeud(mat, unreal.MaterialExpressionMultiply, -900, -300)
    c(desat, "", fois_eclat, "A")
    c(eclat, "", fois_eclat, "B")
    plancher = scalaire(mat, "Plancher", 0.3, -900, -150)
    plus = noeud(mat, unreal.MaterialExpressionAdd, -700, -300)
    c(fois_eclat, "", plus, "A")
    c(plancher, "", plus, "B")
    couleur = noeud(mat, unreal.MaterialExpressionVectorParameter, -700, -500)
    couleur.set_editor_property("parameter_name", "Couleur")
    couleur.set_editor_property("default_value", unreal.LinearColor(1.0, 0.05, 0.05, 1.0))
    teinte = noeud(mat, unreal.MaterialExpressionMultiply, -500, -400)
    c(plus, "", teinte, "A")
    c(couleur, "", teinte, "B")   # sortie d'un VectorParameter = "" (pas "RGB")

    # Ou peindre : zones du masque du pack (R+V+B) + un peu partout (Partout)
    partout = scalaire(mat, "Partout", 0.35, -700, 150)
    if masque is not None:
        t_m = echantillon(mat, masque, -1400, 200)
        a1 = noeud(mat, unreal.MaterialExpressionAdd, -1100, 200)
        c(t_m, "R", a1, "A")
        c(t_m, "G", a1, "B")
        a2 = noeud(mat, unreal.MaterialExpressionAdd, -900, 200)
        c(a1, "", a2, "A")
        c(t_m, "B", a2, "B")
        a3 = noeud(mat, unreal.MaterialExpressionAdd, -700, 250)
        c(a2, "", a3, "A")
        c(partout, "", a3, "B")
        alpha_src = a3
    else:
        alpha_src = partout
    sat = noeud(mat, unreal.MaterialExpressionSaturate, -500, 200)
    c(alpha_src, "", sat, "Input")
    lerp = noeud(mat, unreal.MaterialExpressionLinearInterpolate, -300, -200)
    c(t_d, "RGB", lerp, "A")
    c(teinte, "", lerp, "B")
    c(sat, "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)

    if nrm is not None:
        t_n = echantillon(mat, nrm, -400, 300, normal=True)
        mel.connect_material_property(t_n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    if orm is not None:
        t_o = echantillon(mat, orm, -400, 600)
        mel.connect_material_property(t_o, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
        mel.connect_material_property(t_o, "G", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.connect_material_property(t_o, "B", unreal.MaterialProperty.MP_METALLIC)

    mel.recompile_material(mat)
    eal.save_asset(chemin, False)
    log("   materiau %s cree (parametres : Couleur, Eclat, Plancher, Partout)" % chemin)
    return mat


def colorer():
    mat = creer_materiau()
    chemin_bp = DOSSIER_BP + "/BP_JetPack"
    if not eal.does_asset_exist(chemin_bp):
        raise RuntimeError("BP_JetPack introuvable : lancez d'abord INSTALLER_JETPACKS.bat")
    cdo = unreal.get_default_object(eal.load_blueprint_class(chemin_bp))
    modele = cdo.get_editor_property("modele_jet_pack")
    try:
        slots = modele.get_editor_property("materials") if modele else []
        log("   modele %s : %d emplacement(s) de materiau : %s" % (
            modele.get_name() if modele else "?", len(slots),
            ", ".join(str(s.get_editor_property("material_slot_name")) for s in slots)))
    except Exception:
        pass
    cdo.set_editor_property("materiau_couleur", mat)
    cdo.set_editor_property("parametres_couleur", [unreal.Name("Couleur")])
    cdo.set_editor_property("parametres_couleur_sombre", [])
    eal.save_asset(chemin_bp, False)
    log("   BP_JetPack utilise maintenant %s" % NOM_MAT)
    # rafraichir les JetPacks poses dans la carte ouverte
    try:
        n = 0
        for a in unreal.EditorLevelLibrary.get_all_level_actors():
            if a.get_class().get_name().startswith("BP_JetPack"):
                a.rerun_construction_scripts()
                n += 1
        if n:
            unreal.EditorLevelLibrary.save_current_level()
            log("   %d JetPack(s) de la carte rafraichi(s)" % n)
    except Exception as e:
        log("   (rafraichissement de la carte : %s)" % e)


if __name__ == "__main__":
    try:
        open(JOURNAL, "w").close()
        log("COULEURS des JetPacks ...")
        colorer()
        log("TERMINE. Les JetPacks ont leur couleur (Play pour verifier).")
    except Exception:
        log("ERREUR :\n" + traceback.format_exc())
    if os.environ.get("CHELLES_QUITTER") == "1":
        unreal.SystemLibrary.quit_editor()
