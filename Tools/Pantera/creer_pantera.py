# -*- coding: utf-8 -*-
# CHELLES 3D - PANTERA (2026-11-11)
# Dans Unreal : File > Execute Python Script... > choisir ce fichier.
#
# Cree le corps de PANTERA pour la roue TAB et le menu HABILLAGE :
#   /Game/Personnages/SK_Pantera/SK_Pantera_Vol   = copie du mannequin du pack SuperheroFlight
#                                                    (meme squelette : il vole avec les memes animations)
#   /Game/Personnages/SK_Pantera/M_Pantera_Costume = noir brillant, reflet violet sur les bords (tenue + masque)
#   /Game/Personnages/SK_Pantera/M_Pantera_Accent  = noir metal (pieces du torse)
#   /Game/Personnages/SK_Pantera/M_Pantera_Yeux    = blanc lumineux (les yeux du masque, poses par le jeu)
#
# Ne modifie ni ne supprime rien d'existant : tout est cree dans un nouveau dossier.
# On peut le relancer sans risque (il remet seulement les materiaux).
import unreal

DEST = "/Game/Personnages/SK_Pantera"
NOM = "SK_Pantera_Vol"
SOURCE = "/Game/SuperheroFlight/Characters/Mannequin/Character/Mesh/SK_Mannequin"

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def log(m):
    unreal.log_warning("[Pantera] " + m)


def constante(mat, valeur, x, y):
    c = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property("r", valeur)
    return c


def couleur(mat, r, g, b, x, y):
    c = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, x, y)
    c.set_editor_property("constant", unreal.LinearColor(r, g, b, 1.0))
    return c


def creer_materiau(nom, base, metal, rugosite, emissif=None, reflet=None, peau=True):
    """Materiau simple : couleur, metal, rugosite, + lumiere (emissif) ou reflet colore sur les bords."""
    chemin = DEST + "/" + nom
    if EAL.does_asset_exist(chemin):
        log(nom + " existe deja.")
        return unreal.load_asset(chemin)
    outils = unreal.AssetToolsHelpers.get_asset_tools()
    mat = outils.create_asset(nom, DEST, unreal.Material, unreal.MaterialFactoryNew())
    if peau:
        mat.set_editor_property("used_with_skeletal_mesh", True)
    MEL.connect_material_property(couleur(mat, base[0], base[1], base[2], -500, -200), "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(constante(mat, metal, -500, 0), "", unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(constante(mat, rugosite, -500, 100), "", unreal.MaterialProperty.MP_ROUGHNESS)
    if emissif:
        MEL.connect_material_property(couleur(mat, emissif[0], emissif[1], emissif[2], -500, 250), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    elif reflet:
        # Reflet sur les bords (effet Fresnel) : la silhouette noire se detache dans la nuit
        bord = MEL.create_material_expression(mat, unreal.MaterialExpressionFresnel, -700, 250)
        try:
            bord.set_editor_property("exponent", 4.0)
        except Exception:
            pass
        teinte = couleur(mat, reflet[0], reflet[1], reflet[2], -700, 400)
        fois = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -450, 300)
        MEL.connect_material_expressions(bord, "", fois, "A")
        MEL.connect_material_expressions(teinte, "", fois, "B")
        MEL.connect_material_property(fois, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat, False)
    log(nom + " cree.")
    return mat


if not EAL.does_asset_exist(SOURCE):
    raise RuntimeError("Mannequin du pack SuperheroFlight introuvable : " + SOURCE)
if not EAL.does_directory_exist(DEST):
    EAL.make_directory(DEST)

costume = creer_materiau("M_Pantera_Costume", (0.008, 0.008, 0.010), 0.35, 0.30, reflet=(0.05, 0.02, 0.12))
accent = creer_materiau("M_Pantera_Accent", (0.045, 0.045, 0.055), 0.9, 0.25, reflet=(0.03, 0.03, 0.05))
yeux = creer_materiau("M_Pantera_Yeux", (1.0, 1.0, 1.0), 0.0, 0.2, emissif=(2.5, 2.5, 2.6), peau=False)

chemin = DEST + "/" + NOM
if not EAL.does_asset_exist(chemin):
    if not EAL.duplicate_asset(SOURCE, chemin):
        raise RuntimeError("Copie du mannequin ratee : " + chemin)
    log(NOM + " copie depuis le mannequin SuperheroFlight.")
mesh = unreal.load_asset(chemin)

# Tenue et masque noirs ; le logo du torse devient une piece de metal noir.
# (Correction 2026-11-11 : en Python, chaque case de la liste est une COPIE. Il faut remettre
#  la case modifiee dans la liste, sinon le mannequin reste gris.)
mesh.modify()
mats = mesh.get_editor_property("materials")
for i in range(len(mats)):
    m = mats[i]
    mi = m.get_editor_property("material_interface")
    nom = (mi.get_name() if mi else "").lower()
    slot = str(m.get_editor_property("material_slot_name")).lower()
    m.set_editor_property("material_interface", accent if ("logo" in nom or "logo" in slot) else costume)
    mats[i] = m
mesh.set_editor_property("materials", mats)
try:
    mesh.post_edit_change()
except Exception:
    pass
EAL.save_loaded_asset(mesh, False)

# Verification : on relit les materiaux enregistres
gris = []
for m in mesh.get_editor_property("materials"):
    mi = m.get_editor_property("material_interface")
    log("Materiau du corps : " + (mi.get_path_name() if mi else "aucun"))
    if not mi or "Pantera" not in mi.get_name():
        gris.append(mi.get_name() if mi else "aucun")
if gris:
    raise RuntimeError("PANTERA est encore gris (materiaux : " + ", ".join(gris) + "). Envoie le Output Log a Claude.")

log("TERMINE : maintiens TAB en jeu et choisis PANTERA (case violette), ou choisis PANTERA dans HABILLAGE.")
log("Yeux mal places ? En jeu, console (touche ²) : Chelles.PanteraYeux 8.5 171 3.6 1 15 35  (avant, haut, ecart, taille, inclinaison, ouverture)")
