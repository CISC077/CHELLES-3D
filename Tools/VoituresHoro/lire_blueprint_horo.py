# CHELLES 3D - 25/11/2026 : LECTURE SEULE. Recompile en memoire (sans sauvegarder) le Blueprint de la voiture
# DV_Car_Horo pour que le journal d'Unreal affiche son code (option Kismet CompileDisplaysBinaryBackend).
# Sert a Claude pour comprendre pourquoi la voiture roule lentement, sans moteur ni phares. Rien n'est modifie.
import unreal

for chemin in ("/Game/DV_Car_Horo/Blueprints/BP_Car_Hatchback_Horo",
               "/Game/DV_Car_Horo/Blueprints/BP_vehicle_enter_Car_Hatchback_Horo"):
    bp = unreal.load_asset(chemin)
    unreal.log("[HoroCode] ===== DEBUT %s" % chemin)
    if bp is None:
        unreal.log("[HoroCode] introuvable")
        continue
    # UE 4.27 : pas de BlueprintEditorLibrary ; le Blueprint est compile a son chargement (le code s'affiche alors)
    if hasattr(unreal, "BlueprintEditorLibrary"):
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.log("[HoroCode] ===== FIN %s" % chemin)
