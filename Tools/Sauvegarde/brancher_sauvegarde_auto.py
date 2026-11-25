# -*- coding: utf-8 -*-
"""
CHELLES 3D - branche la SAUVEGARDE AUTOMATIQUE sur tous les scripts de Tools (25/11/2026)

Ajoute tout en haut de chaque script Python de Tools qui utilise Unreal (import unreal)
un petit bloc qui appelle Tools\\Sauvegarde\\sauvegarde_auto.py avant que le script travaille.
Le bloc est encadre par deux lignes "SAUVEGARDE AUTO CHELLES 3D" : on le reconnait et on
ne l'ajoute jamais deux fois. A relancer sans risque quand de nouveaux scripts arrivent.

    python brancher_sauvegarde_auto.py            -> branche
    python brancher_sauvegarde_auto.py --retirer  -> enleve le bloc de tous les scripts
    python brancher_sauvegarde_auto.py --essai    -> montre seulement ce qui serait fait

Une copie de chaque script modifie est gardee dans Tools\\Sauvegarde\\AvantBranchement\\.
"""
from __future__ import print_function

import io
import os
import re
import sys

DEBUT = "# >>> SAUVEGARDE AUTO CHELLES 3D"
FIN = "# <<< SAUVEGARDE AUTO CHELLES 3D"

BLOC = [
    DEBUT + " (pose par Tools\\Sauvegarde\\brancher_sauvegarde_auto.py, ne pas modifier)",
    "try:",
    "    import os as _sa_os, sys as _sa_sys",
    "    _sa_d = _sa_os.path.dirname(_sa_os.path.abspath(globals().get('__file__') or '.'))",
    "    _sa_m = None",
    "    while _sa_d:",
    "        if _sa_os.path.isfile(_sa_os.path.join(_sa_d, 'Tools', 'Sauvegarde', 'sauvegarde_auto.py')):",
    "            _sa_m = _sa_os.path.join(_sa_d, 'Tools', 'Sauvegarde')",
    "            break",
    "        _sa_p = _sa_os.path.dirname(_sa_d)",
    "        _sa_d = None if _sa_p == _sa_d else _sa_p",
    "    if _sa_m is None:",
    "        import unreal as _sa_u",
    "        _sa_m = _sa_os.path.join(_sa_u.Paths.convert_relative_path_to_full(_sa_u.Paths.project_dir()), 'Tools', 'Sauvegarde')",
    "    if _sa_m not in _sa_sys.path:",
    "        _sa_sys.path.insert(0, _sa_m)",
    "    import sauvegarde_auto as _sa_mod",
    "    _sa_mod.avant(globals().get('__file__') or 'script')",
    "except Exception as _sa_e:",
    "    print('[SauvegardeAuto] non faite : %s' % _sa_e)",
    FIN,
]

DOSSIERS_IGNORES = {"sauvegarde", "__pycache__", "out", "out_snap", "avantbranchement"}
IMPORT_UNREAL = re.compile(r"^\s*(import\s+unreal\b|from\s+unreal\s+import)", re.M)

ICI = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(ICI)
COPIES = os.path.join(ICI, "AvantBranchement")


def lire(chemin):
    brut = open(chemin, "rb").read()
    bom = brut.startswith(b"\xef\xbb\xbf")
    if bom:
        brut = brut[3:]
    for enc in ("utf-8", "cp1252", "latin-1"):
        try:
            return brut.decode(enc), enc, bom
        except UnicodeDecodeError:
            continue
    return None, None, bom


def ecrire(chemin, texte, enc, bom):
    donnees = texte.encode(enc)
    if bom:
        donnees = b"\xef\xbb\xbf" + donnees
    with open(chemin, "wb") as f:
        f.write(donnees)


def retirer_bloc(lignes):
    sortie, dedans, change = [], False, False
    for l in lignes:
        if l.startswith(DEBUT):
            dedans, change = True, True
            continue
        if dedans:
            if l.startswith(FIN):
                dedans = False
            continue
        sortie.append(l)
    return sortie, change


def position(lignes):
    """Apres le shebang, la ligne d'encodage, la docstring et les 'from __future__'."""
    i = 0
    n = len(lignes)
    while i < n and i < 2 and (lignes[i].startswith("#!") or re.match(r"^#.*coding[:=]", lignes[i])):
        i += 1
    # docstring du module
    j = i
    while j < n and (not lignes[j].strip() or lignes[j].lstrip().startswith("#")):
        j += 1
    if j < n:
        s = lignes[j].lstrip()
        for q in ('"""', "'''"):
            for pre in ("", "r", "u", "R", "U"):
                if s.startswith(pre + q):
                    reste = s[len(pre) + 3:]
                    if q in reste:
                        i = j + 1
                    else:
                        k = j + 1
                        while k < n and q not in lignes[k]:
                            k += 1
                        i = min(n, k + 1)
                    break
            else:
                continue
            break
    # imports __future__ (doivent rester en premier)
    k = i
    while k < n:
        s = lignes[k].strip()
        if not s or s.startswith("#"):
            k += 1
            continue
        if not s.startswith("from __future__"):
            break
        if "(" in s and ")" not in s:
            while k < n and ")" not in lignes[k]:
                k += 1
        k += 1
        i = k
    return i


def traiter(chemin, retirer, essai):
    texte, enc, bom = lire(chemin)
    if texte is None:
        return "illisible"
    nl = "\r\n" if "\r\n" in texte else "\n"
    lignes = texte.replace("\r\n", "\n").split("\n")
    lignes, avait = retirer_bloc(lignes)
    if retirer:
        if not avait:
            return None
        if not essai:
            ecrire(chemin, nl.join(lignes), enc, bom)
        return "retire"
    if avait:
        return None
    if not IMPORT_UNREAL.search(texte):
        return None
    if "SauvegardeAuto: non" in texte:
        return None
    i = position(lignes)
    nouveau = lignes[:i] + BLOC + lignes[i:]
    if not essai:
        rel = os.path.relpath(chemin, TOOLS)
        copie = os.path.join(COPIES, rel)
        if not os.path.exists(copie):
            os.makedirs(os.path.dirname(copie), exist_ok=True)
            with open(chemin, "rb") as a, open(copie, "wb") as b:
                b.write(a.read())
        ecrire(chemin, nl.join(nouveau), enc, bom)
        # verification : le script doit toujours se compiler
        try:
            compile(nl.join(nouveau).replace("\r\n", "\n"), chemin, "exec")
        except SyntaxError as err:
            with open(copie, "rb") as a, open(chemin, "wb") as b:
                b.write(a.read())
            return "ERREUR (remis comme avant) : %s" % err
    return "branche"


def main():
    retirer = "--retirer" in sys.argv
    essai = "--essai" in sys.argv
    compte = {}
    for base, dossiers, fichiers in os.walk(TOOLS):
        dossiers[:] = [d for d in dossiers if d.lower() not in DOSSIERS_IGNORES]
        for nom in sorted(fichiers):
            if not nom.lower().endswith(".py"):
                continue
            chemin = os.path.join(base, nom)
            try:
                r = traiter(chemin, retirer, essai)
            except Exception as err:
                r = "ERREUR : %s" % err
            if r:
                compte[r] = compte.get(r, 0) + 1
                print("  %-10s %s" % (r if len(r) < 11 else r[:10], os.path.relpath(chemin, TOOLS)))
                if r.startswith("ERREUR"):
                    print("             " + r)
    print("")
    if not compte:
        print("Rien a faire : tous les scripts sont deja a jour.")
    for k, v in sorted(compte.items()):
        print("%s : %d script(s)" % (k, v))
    return 1 if any(k.startswith("ERREUR") for k in compte) else 0


if __name__ == "__main__":
    sys.exit(main())
