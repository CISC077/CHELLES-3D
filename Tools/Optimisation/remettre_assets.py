# Remet les .uasset copies par resauver_assets_lents.py (UNREAL FERME). Lance par ANNULER_OPTIMISATION.bat
import os, shutil, sys
PROJET = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SAUVE = os.path.join(PROJET, "_Sauvegardes", "avant_optim_2026-11-24")
n = 0
for racine, _, fichiers in os.walk(SAUVE):
    for f in fichiers:
        src = os.path.join(racine, f)
        dst = os.path.join(PROJET, "Content", os.path.relpath(src, SAUVE))
        shutil.copy2(src, dst); n += 1
print("[Optim] %d fichier(s) remis." % n)
