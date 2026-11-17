# CHELLES 3D - Arsenal : cree le materiau lumineux des lames de sabre et des traits du blaster.
# A lancer UNE FOIS dans Unreal : Fichier > Executer un script Python > ce fichier.
# Cree /Game/ChellesArsenal/M_LueurChelles (non eclaire, couleur "Color" x intensite "Intensite").
import unreal

DOSSIER = "/Game/ChellesArsenal"
NOM = "M_LueurChelles"
CHEMIN = DOSSIER + "/" + NOM

if unreal.EditorAssetLibrary.does_asset_exist(CHEMIN):
    unreal.log("[Arsenal] " + CHEMIN + " existe deja, rien a faire.")
else:
    outils = unreal.AssetToolsHelpers.get_asset_tools()
    mat = outils.create_asset(NOM, DOSSIER, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mel = unreal.MaterialEditingLibrary

    couleur = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 0)
    couleur.set_editor_property("parameter_name", "Color")
    couleur.set_editor_property("default_value", unreal.LinearColor(0.1, 1.0, 0.2, 1.0))

    force = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 250)
    force.set_editor_property("parameter_name", "Intensite")
    force.set_editor_property("default_value", 12.0)

    fois = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 100)
    mel.connect_material_expressions(couleur, "", fois, "A")
    mel.connect_material_expressions(force, "", fois, "B")
    mel.connect_material_property(fois, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(CHEMIN)
    unreal.log("[Arsenal] Materiau lumineux cree : " + CHEMIN)
