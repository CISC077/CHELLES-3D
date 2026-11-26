# -*- coding: utf-8 -*-
"""CHELLES 3D - allegement PARAGON (26/11/2026, demande de Rachid :
garder seulement Wukong et les heroines, retirer les autres).

Lance par ALLEGER_PARAGON.bat (Unreal ferme). Ce script NE SUPPRIME RIEN :
  1. pour chaque heros a retirer, cherche si un fichier GARDE du jeu (carte Rachid,
     blueprint, autre heros...) l'utilise. Si oui, le heros est BLOQUE (garde).
  2. met en commentaire ("; RETIRE PARAGON 26/11 ") ses lignes Heros=/Skin=/Danses=
     dans Plugins/ChellesWraith/Wraith.txt (copie Wraith.txt.avant_paragon).
  3. ecrit Tools/out/paragon_a_retirer.txt : une ligne OK <Dossier> par dossier a deplacer.
Le .bat deplace ensuite les dossiers OK dans _Sauvegardes (RESTAURER_PARAGON.bat les remet).
"""
import os
import unreal

# Dossier Content -> nom du heros dans Wraith.txt
A_RETIRER = {
    "ParagonBoris": "Boris",
    "ParagonDekker": "Dekker",
    "ParagonGreystone": "Greystone",
    "ParagonIggyScorch": "IggyScorch",
    "ParagonKwang": "Kwang",
    "ParagonSteel": "Steel",
    "ParagonWraith": "Wraith",
}
# Gardes : Aurora, Countess, Phase, Shinbi, Terra, Yin, Zinx (heroines) + SunWukong + ParagonProps

PROJET = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SORTIE = os.path.join(PROJET, "Tools", "out", "paragon_a_retirer.txt")
lignes_sortie = []


def dire(t):
    unreal.log("[Paragon] " + t)
    lignes_sortie.append("# " + t)


ar = unreal.AssetRegistryHelpers.get_asset_registry()
try:
    ar.search_all_assets(True)
except Exception:
    pass

paquets = {}
for dossier in A_RETIRER:
    noms = set()
    for a in ar.get_assets_by_path("/Game/" + dossier, recursive=True):
        noms.add(str(a.package_name))
    paquets[dossier] = noms
tous_retires = set().union(*paquets.values())

opts = unreal.AssetRegistryDependencyOptions()
bloques = {}
for dossier, noms in paquets.items():
    dehors = set()
    for p in noms:
        try:
            refs = ar.get_referencers(p, opts) or []
        except Exception:
            refs = []
        for r in refs:
            r = str(r)
            if r in tous_retires or not r.startswith("/Game/"):
                continue
            dehors.add(r)
    if not noms:
        dire("%s : absent du projet (rien a faire)" % dossier)
    elif dehors:
        bloques[dossier] = sorted(dehors)
        dire("%s : GARDE, utilise par %d fichier(s) du jeu : %s" % (dossier, len(dehors), ", ".join(sorted(dehors)[:5])))
    else:
        lignes_sortie.append("OK %s" % dossier)
        dire("%s : %d fichiers, utilise nulle part ailleurs -> a retirer" % (dossier, len(noms)))

# ---- "statues" sans os des persos Mixamo (base, base_basic_pbr, base_basic_shaded) : inutilisees par le jeu
for dossier_perso in ("LENA", "SOFIA", "LUCIA", "BRENDA", "Shems", "Rebecca"):
    for nom in ("base", "base_basic_pbr", "base_basic_shaded"):
        paquet = "/Game/Personnages/%s/%s" % (dossier_perso, nom)
        if not ar.get_assets_by_package_name(paquet):
            continue
        try:
            refs = [str(r) for r in (ar.get_referencers(paquet, opts) or []) if str(r).startswith("/Game/")]
        except Exception:
            refs = ["?"]
        if refs:
            dire("%s : GARDE, utilise par %s" % (paquet, ", ".join(refs[:3])))
        else:
            lignes_sortie.append("FICHIER Personnages\\%s\\%s.uasset" % (dossier_perso, nom))
            dire("%s : statue inutilisee -> a retirer" % paquet)

# ---- Wraith.txt : lignes des heros retires mises en commentaire
heros_retires = set(A_RETIRER[d] for d in A_RETIRER if d not in bloques and paquets[d])
reglages = os.path.join(PROJET, "Plugins", "ChellesWraith", "Wraith.txt")
if heros_retires and os.path.isfile(reglages):
    with open(reglages, "r", encoding="latin-1", newline="") as f:
        texte = f.read()
    fin = "\r\n" if "\r\n" in texte else "\n"
    lignes = texte.replace("\r\n", "\n").split("\n")
    change = 0
    for i, l in enumerate(lignes):
        cle, _, valeur = l.partition("=")
        cle = cle.strip().lower()
        morceaux = valeur.split("|")
        heros = ""
        if cle in ("heros", "danses"):
            heros = morceaux[0].strip()
        elif cle == "skin" and len(morceaux) > 5:
            heros = morceaux[5].strip()
        if heros in heros_retires:
            lignes[i] = "; RETIRE PARAGON 26/11 " + l
            change += 1
    if change:
        sauvegarde = reglages + ".avant_paragon"
        if not os.path.isfile(sauvegarde):
            with open(sauvegarde, "w", encoding="latin-1", newline="") as f:
                f.write(texte)
        with open(reglages, "w", encoding="latin-1", newline="") as f:
            f.write(fin.join(lignes))
        dire("Wraith.txt : %d lignes retirees (%s)" % (change, ", ".join(sorted(heros_retires))))

os.makedirs(os.path.dirname(SORTIE), exist_ok=True)
with open(SORTIE, "w", encoding="utf-8", newline="") as f:
    f.write("\r\n".join(lignes_sortie) + "\r\n")
