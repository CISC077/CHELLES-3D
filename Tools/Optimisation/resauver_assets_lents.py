# CHELLES 3D - Optimisation finale 24/11/2026
# Re-enregistre les squelettes (Skeletal Mesh) qu'Unreal "repare" a CHAQUE ouverture
# (message "Find and fix some bad tangent! please re-import" dans Saved\Logs).
# Une fois re-enregistres, la reparation est gardee : l'ouverture du projet est plus rapide.
# Copie de securite des .uasset avant : _Sauvegardes\avant_optim_2026-11-24\
# Lance par OPTIMISER_TOUT.bat (Unreal ferme). Lignes du journal : [Optim]
import os, re, glob, shutil
import unreal

PROJET = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
CONTENT = os.path.join(PROJET, "Content")
SAUVE = os.path.join(PROJET, "_Sauvegardes", "avant_optim_2026-11-24")

def log(t):
    unreal.log_warning("[Optim] " + t)  # Warning : visible dans la fenetre du .bat

def trouver_assets():
    vus = set()
    logs = sorted(glob.glob(os.path.join(PROJET, "Saved", "Logs", "FPS_Chelles*.log")), key=os.path.getmtime)[-6:]
    motif = re.compile(r"\]LogSkeletalMesh: Display: \[AssetLog\] (.+?\.uasset): Find and fix some bad tangent")
    for chemin in logs:
        try:
            with open(chemin, encoding="utf-8", errors="ignore") as f:
                for ligne in f:
                    m = motif.search(ligne)
                    if m:
                        vus.add(os.path.normpath(m.group(1)))
        except Exception as e:
            log("journal illisible %s : %s" % (chemin, e))
    return sorted(vus)

def vers_game(fichier):
    rel = os.path.relpath(fichier, CONTENT)
    if rel.startswith(".."):
        return None
    return "/Game/" + os.path.splitext(rel)[0].replace("\\", "/")

fichiers = [f for f in trouver_assets() if os.path.isfile(f)]
log("%d squelette(s) repare(s) a chaque ouverture trouves dans les journaux." % len(fichiers))
ok = 0
for f in fichiers:
    chemin = vers_game(f)
    if not chemin:
        continue
    dest = os.path.join(SAUVE, os.path.relpath(f, CONTENT))
    try:
        if not os.path.exists(dest):
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy2(f, dest)
        asset = unreal.EditorAssetLibrary.load_asset(chemin)
        if asset is None:
            log("introuvable : " + chemin)
            continue
        if unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
            ok += 1
            log("re-enregistre : " + chemin)
        else:
            log("PAS enregistre (fichier en lecture seule ?) : " + chemin)
    except Exception as e:
        log("erreur %s : %s" % (chemin, e))
log("TERMINE : %d / %d squelette(s) re-enregistre(s). Copies dans %s" % (ok, len(fichiers), SAUVE))
