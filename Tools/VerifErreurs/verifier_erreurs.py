# -*- coding: utf-8 -*-
"""
Verification automatique des erreurs de chargement - CHELLES 3D (Unreal 4.27)

Ce que fait ce script (Unreal peut rester ouvert, il n'est pas necessaire) :

  1. Il part de la carte Rachid (et des cartes du Config) et suit TOUTES les
     references vers /Game/... de fichier en fichier, comme Unreal au
     chargement. Chaque reference vers un fichier absent = une erreur.
  2. Il lit aussi les journaux d'Unreal (Saved\\Logs) des dernieres 24 h
     pour recuperer les "Failed to load" vus pendant la journee.
  3. Pour chaque fichier manquant, il corrige SEULEMENT ce qui est sur :
       - le fichier est dans _Quarantaine  -> il le RECOPIE a sa place
         (la quarantaine garde son exemplaire) ;
       - le meme fichier existe ailleurs dans Content, dans un dossier qui
         ressemble (meme nom + meme dossier parent) et une seule fois
         -> il ajoute une redirection dans Config\\DefaultEngine.ini
         (copie de sauvegarde du .ini faite avant).
     Tout le reste est seulement ecrit dans le rapport.
  4. Il ecrit un rapport simple : Tools\\VerifErreurs\\Rapport\\

RIEN N'EST JAMAIS SUPPRIME NI DEPLACE.

Usage :
    python verifier_erreurs.py              -> verification + corrections sures
    python verifier_erreurs.py simulation   -> verification seule, aucun changement
    python verifier_erreurs.py auto         -> comme le premier, sans questions
                                               (utilise par la tache planifiee)
    python verifier_erreurs.py propositions -> applique aussi les propositions
                                               "moins sures" du dernier rapport
"""
import datetime
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time

ICI = os.path.dirname(os.path.abspath(__file__))
PROJET = os.path.abspath(os.path.join(ICI, "..", ".."))
CONTENT = os.path.join(PROJET, "Content")
INI = os.path.join(PROJET, "Config", "DefaultEngine.ini")
LOGS = os.path.join(PROJET, "Saved", "Logs")
QCONTENT = os.path.join(PROJET, "_Quarantaine", "Content")
RAPPORTS = os.path.join(ICI, "Rapport")
SAUVEGARDES = os.path.join(ICI, "Sauvegardes")
ETAT = os.path.join(ICI, "etat.json")

CARTES_DE_DEPART = ["/Game/Maps/Rachid"]
EXTENSIONS = (".uasset", ".umap")
COMPAGNONS = (".uexp", ".ubulk", ".uptnl")
MAX_FICHIERS = 60000

RE_GAME = re.compile(rb"/Game/[A-Za-z0-9_\-/]+(?:\.[A-Za-z0-9_\-]+)?")
RE_LOG_ASSET = re.compile(r"\[AssetLog\] (.+?): Failed to load '(/Game/[^']+)'")
RE_LOG_LOADERR = re.compile(r"LoadErrors: Failed to load (/Game/\S+) Referenced by (.+)$")
RE_REDIRECT = re.compile(r'^\s*\+PackageRedirects\s*=\s*\((.*)\)\s*$', re.I)
RE_CHAMP = re.compile(r'(\w+)\s*=\s*"?([^",]*)"?')


# ----------------------------------------------------------------------------
# Outils
# ----------------------------------------------------------------------------
def maintenant():
    return datetime.datetime.now()


def unreal_ouvert():
    try:
        out = subprocess.run(["tasklist"], capture_output=True, text=True).stdout.lower()
        return "ue4editor" in out
    except Exception:
        return False


def paquet_sans_objet(nom):
    """/Game/A/B.B  ->  /Game/A/B ; /Game/A/B.B_C -> /Game/A/B"""
    nom = nom.strip().strip("'\"")
    if ":" in nom:
        nom = nom.split(":")[0]
    if "." in nom.rsplit("/", 1)[-1]:
        nom = nom[: nom.rfind(".")]
    return nom.rstrip("/")


def rel_depuis_paquet(paquet):
    """/Game/A/B -> A/B"""
    return paquet[len("/Game/"):]


# ----------------------------------------------------------------------------
# Index de Content
# ----------------------------------------------------------------------------
class Index(object):
    def __init__(self, racine):
        self.racine = racine
        self.paquets = {}      # chemin minuscule "a/b" -> chemin fichier
        self.par_nom = {}      # "b" -> ["A/B", ...]
        self.dossiers = set()  # "a/x" minuscule
        for r, ds, fs in os.walk(racine):
            rel_dir = os.path.relpath(r, racine).replace(os.sep, "/")
            if rel_dir == ".":
                rel_dir = ""
            if rel_dir:
                self.dossiers.add(rel_dir.lower())
            for f in fs:
                base, ext = os.path.splitext(f)
                if ext.lower() not in EXTENSIONS:
                    continue
                rel = (rel_dir + "/" + base) if rel_dir else base
                self.paquets[rel.lower()] = os.path.join(r, f)
                self.par_nom.setdefault(base.lower(), []).append(rel)

    def fichier(self, paquet):
        return self.paquets.get(rel_depuis_paquet(paquet).lower())

    def existe(self, paquet):
        rel = rel_depuis_paquet(paquet).lower()
        return rel in self.paquets or rel in self.dossiers


# ----------------------------------------------------------------------------
# Redirections (CoreRedirects) du DefaultEngine.ini
# ----------------------------------------------------------------------------
def lire_redirections():
    reds = []
    if not os.path.exists(INI):
        return reds
    section = ""
    with open(INI, encoding="utf-8", errors="replace") as f:
        for ligne in f:
            l = ligne.strip()
            if l.startswith("[") and l.endswith("]"):
                section = l.lower()
                continue
            if section != "[coreredirects]":
                continue
            m = RE_REDIRECT.match(l)
            if not m:
                continue
            champs = dict((k.lower(), v) for k, v in RE_CHAMP.findall(m.group(1)))
            if "oldname" in champs and "newname" in champs:
                reds.append((champs["oldname"], champs["newname"],
                             champs.get("matchsubstring", "").lower() == "true"))
    return reds


def appliquer_redirections(paquet, reds):
    for _ in range(5):
        change = False
        for old, new, sub in reds:
            if sub:
                i = paquet.lower().find(old.lower())
                if i >= 0:
                    paquet = paquet[:i] + new + paquet[i + len(old):]
                    change = True
                    break
            elif paquet.lower() == paquet_sans_objet(old).lower():
                paquet = paquet_sans_objet(new)
                change = True
                break
        if not change:
            break
    return paquet


# ----------------------------------------------------------------------------
# Lecture de l'en-tete d'un .uasset / .umap (table des noms)
# ----------------------------------------------------------------------------
def taille_entete(b):
    try:
        tag, = struct.unpack_from("<I", b, 0)
        if tag != 0x9E2A83C1:
            return None
        legacy, = struct.unpack_from("<i", b, 4)
        o = 8
        if legacy != -4:
            o += 4
        o += 8
        if legacy <= -2:
            n, = struct.unpack_from("<i", b, o)
            o += 4
            if n < 0 or n > 10000:
                return None
            o += n * 20
        total, = struct.unpack_from("<i", b, o)
        if 0 < total < 512 * 1024 * 1024:
            return total
    except struct.error:
        pass
    return None


def references(chemin):
    try:
        with open(chemin, "rb") as f:
            debut = f.read(64 * 1024)
            t = taille_entete(debut) or 8 * 1024 * 1024
            if t > len(debut):
                f.seek(0)
                data = f.read(min(t, 64 * 1024 * 1024))
            else:
                data = debut[:t]
    except (IOError, OSError):
        return set()
    propre = os.path.splitext(os.path.basename(chemin))[0].lower()
    refs = set()
    for m in RE_GAME.findall(data):
        s = m.decode("ascii", "ignore")
        if s.endswith("/"):
            continue
        p = paquet_sans_objet(s)
        if p.count("/") < 2:
            continue
        # le propre nom interne du fichier (pack copie ailleurs) n'est pas une reference
        if p.rsplit("/", 1)[-1].lower() == propre:
            continue
        refs.add(p)
    return refs


# ----------------------------------------------------------------------------
# Journaux Unreal
# ----------------------------------------------------------------------------
def erreurs_des_journaux(depuis):
    trouve = {}
    if not os.path.isdir(LOGS):
        return trouve
    for f in os.listdir(LOGS):
        if not f.lower().endswith(".log"):
            continue
        p = os.path.join(LOGS, f)
        try:
            if os.path.getmtime(p) < depuis:
                continue
            with open(p, encoding="utf-8", errors="replace") as fh:
                for ligne in fh:
                    if "Failed to load" not in ligne:
                        continue
                    m = RE_LOG_ASSET.search(ligne)
                    if m:
                        ref = m.group(1).strip()
                        if ref.lower().startswith(CONTENT.lower()):
                            ref = "/Game/" + os.path.splitext(
                                os.path.relpath(ref, CONTENT))[0].replace(os.sep, "/")
                        trouve.setdefault(paquet_sans_objet(m.group(2)), set()).add(ref)
                        continue
                    m = RE_LOG_LOADERR.search(ligne.rstrip())
                    if m:
                        trouve.setdefault(paquet_sans_objet(m.group(1)), set()).add(
                            m.group(2).strip())
        except (IOError, OSError):
            pass
    return trouve


# ----------------------------------------------------------------------------
# Recherche d'une solution pour un fichier manquant
# ----------------------------------------------------------------------------
def dans_quarantaine(paquet):
    rel = rel_depuis_paquet(paquet).replace("/", os.sep)
    for ext in EXTENSIONS:
        p = os.path.join(QCONTENT, rel + ext)
        if os.path.exists(p):
            return p
    return None


def chercher_ailleurs(paquet, index):
    """Renvoie (meilleur, score, nb_ex_aequo, tous_les_candidats)."""
    morceaux = rel_depuis_paquet(paquet).split("/")
    nom = morceaux[-1].lower()
    dossiers = [m.lower() for m in morceaux[:-1]]
    candidats = index.par_nom.get(nom, [])
    if not candidats:
        return None, 0, 0, []
    notes = []
    for c in candidats:
        cd = [m.lower() for m in c.split("/")[:-1]]
        s = 0
        while s < len(dossiers) and s < len(cd) and dossiers[-1 - s] == cd[-1 - s]:
            s += 1
        notes.append((s, c))
    notes.sort(key=lambda x: -x[0])
    meilleur = notes[0][0]
    ex = [c for s, c in notes if s == meilleur]
    return "/Game/" + ex[0], meilleur, len(ex), ["/Game/" + c for _, c in notes[:5]]


def recopier_de_quarantaine(paquet, fichier_q):
    rel = rel_depuis_paquet(paquet).replace("/", os.sep)
    base_q = os.path.splitext(fichier_q)[0]
    copies = []
    for ext in EXTENSIONS + COMPAGNONS:
        src = base_q + ext
        if not os.path.exists(src):
            continue
        dst = os.path.join(CONTENT, rel + ext)
        if os.path.exists(dst):
            continue
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
        copies.append(dst)
    return copies


def ajouter_redirections(paires, simulation):
    """paires = [(ancien, nouveau)] ; ecrit dans [CoreRedirects] avec sauvegarde."""
    if not paires or simulation:
        return None
    os.makedirs(SAUVEGARDES, exist_ok=True)
    horo = maintenant().strftime("%Y-%m-%d_%Hh%M")
    sauvegarde = os.path.join(SAUVEGARDES, "DefaultEngine_%s.ini" % horo)
    shutil.copy2(INI, sauvegarde)
    with open(INI, encoding="utf-8", errors="replace", newline="") as f:
        texte = f.read()
    fin = "\r\n" if "\r\n" in texte else "\n"
    lignes = ["; VerifErreurs automatique %s" % maintenant().strftime("%Y-%m-%d %H:%M")]
    for old, new in paires:
        lignes.append('+PackageRedirects=(OldName="%s",NewName="%s")' % (old, new))
    bloc = fin.join(lignes) + fin
    m = re.search(r"^\[CoreRedirects\][^\S\r\n]*\r?\n", texte, re.M | re.I)
    if m:
        # a la fin de la section [CoreRedirects]
        suite = texte[m.end():]
        n = re.search(r"^\[", suite, re.M)
        pos = m.end() + (n.start() if n else len(suite))
        avant = texte[:pos]
        while avant.endswith(fin + fin):
            avant = avant[: -len(fin)]
        if not avant.endswith(fin):
            avant += fin
        texte = avant + bloc + fin + texte[pos:].lstrip("\r\n")
    else:
        if not texte.endswith(fin):
            texte += fin
        texte += fin + "[CoreRedirects]" + fin + bloc
    with open(INI, "w", encoding="utf-8", newline="") as f:
        f.write(texte)
    return sauvegarde


# ----------------------------------------------------------------------------
# Programme principal
# ----------------------------------------------------------------------------
def principal():
    mode = (sys.argv[1].lower() if len(sys.argv) > 1 else "normal")
    simulation = mode == "simulation"
    auto = mode == "auto"
    accepter_propositions = mode == "propositions"
    debut = time.time()

    etat = {}
    if os.path.exists(ETAT):
        try:
            etat = json.load(open(ETAT, encoding="utf-8"))
        except Exception:
            etat = {}
    depuis = min(etat.get("derniere_verif", 0) or 0, time.time() - 24 * 3600) \
        if etat.get("derniere_verif") else time.time() - 24 * 3600

    print("")
    print("  Verification des erreurs de chargement - CHELLES 3D")
    print("  Mode : %s" % {"simulation": "SIMULATION (aucun changement)",
                           "auto": "automatique (fin de journee)",
                           "propositions": "corrections + propositions"}.get(mode, "corrections sures"))
    print("  Lecture de la liste des fichiers de Content (1 a 2 minutes)...")
    index = Index(CONTENT)
    print("  %d fichiers Unreal trouves." % len(index.paquets))
    reds = lire_redirections()

    # Cartes de depart : Rachid + cartes citees dans le Config
    departs = list(CARTES_DE_DEPART)
    try:
        for l in open(INI, encoding="utf-8", errors="replace"):
            if l.split("=")[0].strip() in ("EditorStartupMap", "GameDefaultMap", "ServerDefaultMap"):
                v = l.split("=", 1)[1].strip()
                if v.startswith("/Game/"):
                    departs.append(paquet_sans_objet(v))
    except IOError:
        pass

    # 1) Suivre les references depuis les cartes
    print("  Je suis toutes les references depuis la carte Rachid...")
    manquants = {}        # paquet -> set(referents)
    vus = set()
    file_attente = [(p, "(carte de depart)") for p in departs]
    recopies = []
    nb_lus = 0
    while file_attente and nb_lus < MAX_FICHIERS:
        paquet, referent = file_attente.pop()
        cible = appliquer_redirections(paquet, reds)
        cle = cible.lower()
        fichier = index.fichier(cible)
        if fichier is None:
            if index.existe(cible):
                continue  # c'est un dossier
            manquants.setdefault(paquet, set()).add(referent)
            # si en quarantaine : on suit quand meme ses references
            fq = dans_quarantaine(cible)
            if fq is None or cle in vus:
                continue
            vus.add(cle)
            fichier = fq
        else:
            if cle in vus:
                continue
            vus.add(cle)
        nb_lus += 1
        if nb_lus % 1000 == 0:
            print("    %d fichiers lus..." % nb_lus)
        for r in references(fichier):
            if r.lower() not in vus:
                file_attente.append((r, cible))
    print("  %d fichiers suivis depuis la carte." % nb_lus)

    # 2) Journaux d'Unreal
    du_journal = erreurs_des_journaux(depuis)
    for p, refs in du_journal.items():
        manquants.setdefault(p, set()).update(refs)

    # 3) Classer et corriger
    deja_corriges, recopies_q, redirs_auto, propositions, introuvables = [], [], [], [], []
    for paquet in sorted(manquants):
        refs = sorted(manquants[paquet])
        cible = appliquer_redirections(paquet, reds)
        if index.existe(cible):
            deja_corriges.append({"manquant": paquet, "vers": cible, "referents": refs})
            continue
        fq = dans_quarantaine(cible)
        if fq:
            recopies_q.append({"manquant": cible, "quarantaine": fq, "referents": refs})
            continue
        meilleur, score, ex, cands = chercher_ailleurs(cible, index)
        if meilleur and score >= 1 and ex == 1:
            redirs_auto.append({"manquant": paquet, "vers": meilleur, "referents": refs})
        elif meilleur:
            propositions.append({"manquant": paquet, "vers": meilleur, "candidats": cands,
                                 "referents": refs,
                                 "pourquoi": "meme nom de fichier mais dossier different"
                                 if score == 0 else "plusieurs copies possibles"})
        else:
            introuvables.append({"manquant": paquet, "referents": refs})

    # propositions du dernier rapport, si demande
    if accepter_propositions:
        redirs_auto.extend(propositions)
        propositions = []

    sauvegarde_ini = None
    if not simulation:
        for e in recopies_q:
            try:
                e["copies"] = recopier_de_quarantaine(e["manquant"], e["quarantaine"])
            except Exception as ex_:
                e["erreur"] = str(ex_)
        try:
            sauvegarde_ini = ajouter_redirections(
                [(e["manquant"], e["vers"]) for e in redirs_auto], simulation)
        except Exception as ex_:
            for e in redirs_auto:
                e["erreur"] = str(ex_)

    # 4) Rapport
    os.makedirs(RAPPORTS, exist_ok=True)
    jour = maintenant().strftime("%Y-%m-%d")
    heure = maintenant().strftime("%H:%M")
    ue = unreal_ouvert()
    L = []
    L.append("RAPPORT DES ERREURS DE CHARGEMENT - CHELLES 3D")
    L.append("Date : %s a %s   Mode : %s" % (jour, heure, mode))
    L.append("Fichiers suivis depuis la carte Rachid : %d   Journaux lus depuis : %s"
             % (nb_lus, datetime.datetime.fromtimestamp(depuis).strftime("%Y-%m-%d %H:%M")))
    L.append("")
    total_restant = len(propositions) + len(introuvables)
    if not manquants:
        L.append(">>> TOUT EST BON : aucune erreur de chargement trouvee.")
    else:
        L.append(">>> RESUME")
        L.append("  Deja corriges (redirection existante)  : %d" % len(deja_corriges))
        L.append("  Recopies depuis _Quarantaine          : %d%s" % (
            len(recopies_q), " (simulation : rien copie)" if simulation else ""))
        L.append("  Redirections ajoutees automatiquement : %d%s" % (
            len(redirs_auto), " (simulation : rien ecrit)" if simulation else ""))
        L.append("  A VERIFIER (propositions)             : %d" % len(propositions))
        L.append("  INTROUVABLES                          : %d" % len(introuvables))
    if (recopies_q or redirs_auto) and not simulation:
        L.append("")
        L.append("  >> Redemarre Unreal pour que les corrections soient prises en compte%s."
                 % (" (Unreal est ouvert en ce moment)" if ue else ""))
    if sauvegarde_ini:
        L.append("  >> Copie du DefaultEngine.ini avant changement : %s" % sauvegarde_ini)

    def bloc(titre, liste, avec_vers=True):
        if not liste:
            return
        L.append("")
        L.append("=" * 78)
        L.append(titre)
        L.append("=" * 78)
        for e in liste:
            L.append("- " + e["manquant"])
            if avec_vers and e.get("vers"):
                L.append("    -> " + e["vers"])
            if e.get("pourquoi"):
                L.append("    (" + e["pourquoi"] + ")")
            for c in e.get("candidats", [])[1:]:
                L.append("    autre possibilite : " + c)
            if e.get("erreur"):
                L.append("    ERREUR : " + e["erreur"])
            L.append("    utilise par : " + ", ".join(e["referents"][:4])
                     + (" ..." if len(e["referents"]) > 4 else ""))

    bloc("RECOPIES DEPUIS LA QUARANTAINE (le pack avait ete mis de cote mais sert encore)",
         [dict(e, vers="Content (copie)") for e in recopies_q])
    bloc("REDIRECTIONS AJOUTEES AUTOMATIQUEMENT (fichier retrouve ailleurs, meme dossier)",
         redirs_auto)
    bloc("A VERIFIER - PROPOSITIONS (pas appliquees : lance Accepter_les_propositions.bat"
         " si elles te conviennent)", propositions)
    bloc("INTROUVABLES (le fichier n'existe nulle part dans le projet)", introuvables, False)
    if introuvables:
        L.append("")
        L.append("  Pour les introuvables : soit le pack d'origine n'est plus la (le")
        L.append("  reinstaller), soit l'objet qui l'utilise peut etre retire de la carte.")
        L.append("  Montre ce rapport a Claude, il te dira quoi faire.")
    L.append("")
    L.append("Duree : %d s. Rien n'a ete supprime." % int(time.time() - debut))

    texte = "\r\n".join(L) + "\r\n"
    chemin_txt = os.path.join(RAPPORTS, "rapport_%s.txt" % jour)
    with open(chemin_txt, "w", encoding="utf-8") as f:
        f.write(texte)
    shutil.copy2(chemin_txt, os.path.join(RAPPORTS, "DERNIER_RAPPORT.txt"))
    with open(os.path.join(RAPPORTS, "rapport_%s.json" % jour), "w", encoding="utf-8") as f:
        json.dump({"date": jour, "heure": heure, "mode": mode, "fichiers_suivis": nb_lus,
                   "deja_corriges": deja_corriges, "recopies_quarantaine": recopies_q,
                   "redirections_auto": redirs_auto, "propositions": propositions,
                   "introuvables": introuvables}, f, ensure_ascii=False, indent=1)

    if not simulation:
        etat["derniere_verif"] = time.time()
        etat["dernier_rapport"] = chemin_txt
        etat["restant"] = total_restant
        json.dump(etat, open(ETAT, "w", encoding="utf-8"), indent=1)

    print("")
    print(texte.replace("\r\n", "\n"))
    print("  Rapport : %s" % chemin_txt)

    # En mode automatique : ouvrir le rapport seulement s'il y a quelque chose a voir
    if auto and (total_restant or recopies_q or redirs_auto):
        try:
            subprocess.Popen(["notepad.exe", chemin_txt])
        except Exception:
            pass
    return 0


if __name__ == "__main__":
    try:
        sys.exit(principal())
    except KeyboardInterrupt:
        print("  Arrete.")
        sys.exit(1)
