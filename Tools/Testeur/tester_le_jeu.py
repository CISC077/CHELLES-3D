# -*- coding: utf-8 -*-
"""
CHELLES 3D - Agent 5 "Testeur" : lance le jeu tout seul, le laisse parcourir la carte
(plugin ChellesTesteur), puis ecrit un rapport en francais simple.

  python tester_le_jeu.py            -> test maintenant, le rapport s'ouvre a la fin
  python tester_le_jeu.py auto       -> test du soir (tache Windows) : Bloc-notes seulement s'il y a un souci

Rien n'est modifie dans le projet : le jeu est lance en mode "jeu" (comme Play Standalone),
sans enregistrer la carte. Le rapport va dans Tools\\Testeur\\Rapport\\.
"""
import datetime
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import time

ICI = os.path.dirname(os.path.abspath(__file__))
PROJET = os.path.normpath(os.path.join(ICI, "..", ".."))
UPROJECT = os.path.join(PROJET, "FPS_Chelles.uproject")
PLUGIN = os.path.join(PROJET, "Plugins", "ChellesTesteur")
REGLAGES = os.path.join(PLUGIN, "Testeur.txt")
SAVED = os.path.join(PROJET, "Saved")
RESULTATS = os.path.join(SAVED, "Testeur", "resultats.json")
JOURNAL = os.path.join(SAVED, "Testeur", "journal_du_test.log")
CRASHS = os.path.join(SAVED, "Crashes")
DOSSIER_RAPPORT = os.path.join(ICI, "Rapport")
ETAT_VERIF = os.path.join(PROJET, "Tools", "VerifErreurs", "etat.json")
GARDER_JOURS = 30

MODE = (sys.argv[1] if len(sys.argv) > 1 else "normal").lower()
AUTO = MODE == "auto"


# ---------------------------------------------------------------------------
def lire_reglages():
    r = {"SansSon": "Oui", "Fenetre": "1600x900", "DureeMax": "25", "TestDuSoir": "Oui"}
    try:
        with open(REGLAGES, "r", encoding="utf-8-sig", errors="replace") as f:
            for ligne in f:
                ligne = ligne.strip()
                if not ligne or ligne.startswith("#") or "=" not in ligne:
                    continue
                cle, valeur = ligne.split("=", 1)
                r[cle.strip()] = valeur.strip()
    except OSError:
        pass
    return r


def oui(valeur):
    return str(valeur).strip().lower() in ("oui", "1", "yes", "true")


def trouver_unreal():
    candidats = []
    try:
        import winreg
        for racine in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
            try:
                with winreg.OpenKey(racine, r"SOFTWARE\EpicGames\Unreal Engine\4.27") as cle:
                    candidats.append(winreg.QueryValueEx(cle, "InstalledDirectory")[0])
            except OSError:
                pass
    except ImportError:
        pass
    candidats.append(r"C:\Program Files\Epic Games\UE_4.27")
    for dossier in candidats:
        exe = os.path.join(dossier, "Engine", "Binaries", "Win64", "UE4Editor.exe")
        if os.path.isfile(exe):
            return exe
    return None


def processus_ouverts(nom):
    try:
        sortie = subprocess.run(["tasklist", "/fi", "imagename eq %s" % nom, "/fo", "csv", "/nh"],
                                capture_output=True, text=True, errors="replace").stdout
    except OSError:
        return []
    pids = []
    for ligne in sortie.splitlines():
        morceaux = [m.strip('"') for m in ligne.split('","')]
        if len(morceaux) > 1 and morceaux[0].lower() == nom.lower():
            pids.append(morceaux[1])
    return pids


def dossiers_crash():
    if not os.path.isdir(CRASHS):
        return set()
    return set(os.listdir(CRASHS))


def lire_crash(dossier):
    """Message et premieres lignes de la pile d'appels d'un plantage Unreal."""
    info = {"dossier": dossier, "message": "", "pile": []}
    xml = os.path.join(dossier, "CrashContext.runtime-xml")
    if os.path.isfile(xml):
        try:
            with open(xml, "r", encoding="utf-8", errors="replace") as f:
                texte = f.read()
            m = re.search(r"<ErrorMessage>(.*?)</ErrorMessage>", texte, re.S)
            if m:
                info["message"] = m.group(1).strip()[:500]
            m = re.search(r"<CallStack>(.*?)</CallStack>", texte, re.S)
            if m:
                info["pile"] = [l.strip() for l in m.group(1).strip().splitlines() if l.strip()][:8]
        except OSError:
            pass
    return info


# ---------------------------------------------------------------------------
#  Lecture du journal du jeu
# ---------------------------------------------------------------------------
RE_LIGNE = re.compile(r"^\[[^\]]*\]\[[^\]]*\](\w+): (Error|Warning|Fatal error|Display)?:?\s*(.*)$")

EXPLICATIONS = [
    (r"Failed to load|Can't find file|Couldn't find file|could not be found",
     "Un fichier du projet manque (texture, modele, son...). La verification des fichiers (Tools\\VerifErreurs) s'en occupe."),
    (r"Accessed None",
     "Un Blueprint utilise un objet qui n'existe pas (encore) : souvent sans gravite, mais peut casser une fonction."),
    (r"Blueprint Runtime Error|Script Msg",
     "Erreur dans un Blueprint pendant le jeu."),
    (r"Ensure condition failed|Assertion failed",
     "Le moteur a detecte une situation anormale (peut annoncer un plantage)."),
    (r"Fatal error|Unhandled Exception|EXCEPTION_ACCESS_VIOLATION",
     "PLANTAGE du jeu."),
    (r"Out of memory|OutOfMemory|Ran out of memory|texture streaming pool",
     "Manque de memoire (carte graphique ou PC) : baisser la qualite ou reduire les textures."),
    (r"NavMesh|Navigation",
     "Probleme de NavMesh : les personnages controles par l'ordinateur peuvent mal se deplacer."),
    (r"Shader|shader",
     "Probleme de shader (materiau) : un objet peut s'afficher en gris ou en damier."),
    (r"Network|NetDriver|Connection",
     "Probleme reseau (multijoueur)."),
]


def expliquer(texte):
    for motif, explication in EXPLICATIONS:
        if re.search(motif, texte):
            return explication
    return ""


def normaliser(texte):
    t = re.sub(r"0x[0-9A-Fa-f]+", "#", texte)
    t = re.sub(r"\d+", "#", t)
    return t.strip()[:220]


def lire_journal(chemin):
    resume = {"erreurs": {}, "avertissements": {}, "nb_erreurs": 0, "nb_avertissements": 0,
              "fatal": [], "fin_du_test": False, "existe": os.path.isfile(chemin), "dernieres_lignes": []}
    if not resume["existe"]:
        return resume
    with open(chemin, "r", encoding="utf-8", errors="replace") as f:
        lignes = f.readlines()
    resume["dernieres_lignes"] = [l.rstrip() for l in lignes[-15:]]
    for ligne in lignes:
        ligne = ligne.rstrip()
        if "[Testeur] FIN du test" in ligne:
            resume["fin_du_test"] = True
        if "Fatal error" in ligne or "Unhandled Exception" in ligne or "Assertion failed" in ligne:
            resume["fatal"].append(ligne[:400])
        m = RE_LIGNE.match(ligne)
        if not m:
            continue
        categorie, niveau, message = m.group(1), m.group(2) or "", m.group(3)
        if categorie == "LogChellesTesteur":
            continue
        if niveau in ("Error", "Fatal error"):
            cle = categorie + ": " + normaliser(message)
            d = resume["erreurs"].setdefault(cle, {"nombre": 0, "exemple": categorie + ": " + message[:300]})
            d["nombre"] += 1
            resume["nb_erreurs"] += 1
        elif niveau == "Warning":
            cle = categorie + ": " + normaliser(message)
            d = resume["avertissements"].setdefault(cle, {"nombre": 0, "exemple": categorie + ": " + message[:300]})
            d["nombre"] += 1
            resume["nb_avertissements"] += 1
    return resume


# ---------------------------------------------------------------------------
#  Lancement du jeu
# ---------------------------------------------------------------------------
def lancer_le_jeu(exe, reglages):
    largeur, hauteur = "1600", "900"
    m = re.match(r"(\d+)\s*[xX]\s*(\d+)", reglages.get("Fenetre", ""))
    if m:
        largeur, hauteur = m.group(1), m.group(2)
    try:
        duree_max = max(5, int(float(reglages.get("DureeMax", "25"))))
    except ValueError:
        duree_max = 25

    os.makedirs(os.path.dirname(RESULTATS), exist_ok=True)
    for ancien in (RESULTATS, JOURNAL):
        if os.path.exists(ancien):
            try:
                os.remove(ancien)
            except OSError:
                pass

    commande = [exe, UPROJECT, "/Game/Maps/Rachid", "-game", "-windowed",
                "-ResX=%s" % largeur, "-ResY=%s" % hauteur, "-ChellesTest",
                "-nosplash", "-unattended", "-ABSLOG=%s" % JOURNAL]
    if oui(reglages.get("SansSon", "Oui")):
        commande.append("-nosound")

    print("  Lancement du jeu (la carte Rachid met un moment a s'ouvrir)...")
    debut = time.time()
    processus = subprocess.Popen(commande, cwd=os.path.dirname(exe))
    fige = False
    try:
        code = processus.wait(timeout=duree_max * 60)
    except subprocess.TimeoutExpired:
        fige = True
        print("  Le jeu ne repond plus apres %d minutes : on le ferme." % duree_max)
        subprocess.run(["taskkill", "/PID", str(processus.pid), "/T", "/F"], capture_output=True)
        try:
            code = processus.wait(timeout=60)
        except subprocess.TimeoutExpired:
            code = None
    return {"code": code, "fige": fige, "duree": time.time() - debut, "duree_max": duree_max,
            "commande": " ".join('"%s"' % c if " " in c else c for c in commande)}


# ---------------------------------------------------------------------------
#  Rapport
# ---------------------------------------------------------------------------
def fps_mot(fps, faible):
    if fps <= 0:
        return "pas mesure"
    if fps >= max(50, faible + 20):
        return "tres fluide"
    if fps >= faible:
        return "correct"
    return "RAME"


def ecrire_rapport(contexte):
    maintenant = datetime.datetime.now()
    L = []
    ajoute = L.append
    res = contexte.get("resultats") or {}
    journal = contexte.get("journal") or {}
    lancement = contexte.get("lancement") or {}
    etapes = res.get("etapes", [])
    faible = float(res.get("fps_faible", 30) or 30)

    problemes_etapes = [e for e in etapes if e.get("etat") == "probleme"]
    plantage = bool(contexte.get("crashs")) or bool(journal.get("fatal"))
    fige = lancement.get("fige", False)
    termine = bool(res.get("termine")) or journal.get("fin_du_test", False)

    # ---------------- Verdict ----------------
    if contexte.get("pas_lance"):
        verdict = "TEST NON FAIT"
    elif plantage:
        verdict = "PLANTAGE"
    elif fige:
        verdict = "JEU FIGE"
    elif not termine:
        verdict = "TEST INCOMPLET"
    elif problemes_etapes:
        verdict = "A REGARDER (%d souci%s)" % (len(problemes_etapes), "s" if len(problemes_etapes) > 1 else "")
    else:
        verdict = "TOUT VA BIEN"

    ajoute("RAPPORT DU TESTEUR AUTOMATIQUE - CHELLES 3D (Agent 5)")
    ajoute("Date : %s" % maintenant.strftime("%d/%m/%Y a %H:%M"))
    ajoute("")
    ajoute("=" * 78)
    ajoute("  RESULTAT : %s" % verdict)
    ajoute("=" * 78)

    if contexte.get("pas_lance"):
        ajoute("")
        ajoute("  " + contexte["pas_lance"])
        ajoute("")
        return verdict, "\n".join(L) + "\n"

    ajoute("")
    ajoute(">>> EN RESUME")
    if plantage:
        ajoute("  - Le jeu a PLANTE pendant le test (voir la partie PLANTAGE plus bas).")
    if fige:
        ajoute("  - Le jeu s'est FIGE : il ne repondait plus apres %d minutes, il a ete ferme." % lancement.get("duree_max", 25))
    if not termine and not plantage and not fige:
        ajoute("  - Le test ne s'est pas termine normalement (jeu ferme avant la fin).")
    if res:
        etape_cassee = res.get("etape_en_cours") if not termine else ""
        if etape_cassee:
            ajoute("  - Derniere etape commencee : %s" % etape_cassee)
        fps_moy = float(res.get("fps_moyen", 0) or 0)
        fps_min = float(res.get("fps_min", 0) or 0)
        ajoute("  - Fluidite : %.0f images par seconde en moyenne (%s), au plus bas %.0f." % (fps_moy, fps_mot(fps_moy, faible), fps_min))
        ajoute("  - Ouverture de la carte : %.0f secondes." % float(res.get("chargement_secondes", 0) or 0))
        ajoute("  - Memoire du PC utilisee au maximum : %.1f Go." % (float(res.get("memoire_max_mo", 0) or 0) / 1024.0))
        ajoute("  - Etapes reussies : %d sur %d." % (len([e for e in etapes if e.get("etat") == "ok"]), len(etapes)))
    else:
        ajoute("  - Le testeur n'a rien pu mesurer (le jeu s'est ferme avant d'arriver sur la carte).")
    ajoute("  - Erreurs dans le journal du jeu : %d (%d sortes differentes)." % (journal.get("nb_erreurs", 0), len(journal.get("erreurs", {}))))
    verif = contexte.get("verif")
    if verif is not None:
        ajoute("  - Fichiers manquants trouves par la verification du soir : %s (detail : Tools\\VerifErreurs\\Rapport\\DERNIER_RAPPORT.txt)." % verif)
    ajoute("")
    ajoute("  Comment lire : 'tres fluide' = 50 images/s ou plus, 'correct' = %.0f a 50, 'RAME' = moins de %.0f." % (faible, faible))

    # ---------------- Plantage ----------------
    if plantage:
        ajoute("")
        ajoute("=" * 78)
        ajoute("PLANTAGE")
        ajoute("=" * 78)
        for c in contexte.get("crashs", []):
            ajoute("  Message : %s" % (c.get("message") or "(pas de message)"))
            for l in c.get("pile", []):
                ajoute("     %s" % l)
            ajoute("  Dossier du plantage : %s" % c.get("dossier"))
        for l in journal.get("fatal", [])[:5]:
            ajoute("  Journal : %s" % l)
        ajoute("  >> Envoie ce rapport a Claude dans le projet CHELLES 3D (thread Agent 1 Crashs).")

    # ---------------- Problemes ----------------
    if problemes_etapes:
        ajoute("")
        ajoute("=" * 78)
        ajoute("LES SOUCIS TROUVES")
        ajoute("=" * 78)
        for e in problemes_etapes:
            ajoute("- %s" % e.get("nom"))
            for p in e.get("problemes", []):
                ajoute("    * %s" % p)
            if e.get("position"):
                ajoute("    (endroit : %s)" % e["position"])

    # ---------------- Toutes les etapes ----------------
    if etapes:
        ajoute("")
        ajoute("=" * 78)
        ajoute("TOUTES LES ETAPES DU PARCOURS")
        ajoute("=" * 78)
        symboles = {"ok": "[OK]      ", "probleme": "[SOUCI]   ", "non teste": "[PAS FAIT]", "en cours": "[COUPE]   ", "pas fait": "[PAS FAIT]"}
        categorie = None
        titres = {"Zone": "Visite de la carte", "Marche": "Deplacement", "Menu": "Menus", "Personnage": "Personnages et vehicules", "Depart": "Depart"}
        for e in etapes:
            if e.get("categorie") != categorie:
                categorie = e.get("categorie")
                ajoute("")
                ajoute("  -- %s --" % titres.get(categorie, categorie))
            fps = float(e.get("fps_moyen", 0) or 0)
            texte_fps = ("%3.0f img/s (%s)" % (fps, fps_mot(fps, faible))) if fps > 0 else ""
            erreurs = int(e.get("erreurs", 0) or 0)
            texte_err = (", %d erreur%s" % (erreurs, "s" if erreurs > 1 else "")) if erreurs else ""
            ajoute("  %s %-38s %s%s" % (symboles.get(e.get("etat"), "[?]       "), e.get("nom", "")[:38], texte_fps, texte_err))
            for r in e.get("remarques", []):
                ajoute("               note : %s" % r)
            for x in e.get("exemples_erreurs", [])[:2]:
                ajoute("               erreur : %s" % x[:160])

    # ---------------- Erreurs du journal ----------------
    erreurs = sorted(journal.get("erreurs", {}).items(), key=lambda kv: -kv[1]["nombre"])
    if erreurs:
        ajoute("")
        ajoute("=" * 78)
        ajoute("LES ERREURS DU JOURNAL (les plus frequentes d'abord, 20 max)")
        ajoute("=" * 78)
        for cle, d in erreurs[:20]:
            ajoute("- %d fois : %s" % (d["nombre"], d["exemple"][:240]))
            explication = expliquer(d["exemple"])
            if explication:
                ajoute("      = %s" % explication)
        if len(erreurs) > 20:
            ajoute("  ... et %d autres sortes d'erreurs (journal complet : Saved\\Testeur\\journal_du_test.log)" % (len(erreurs) - 20))

    avert = sorted(journal.get("avertissements", {}).items(), key=lambda kv: -kv[1]["nombre"])
    if avert:
        ajoute("")
        ajoute("AVERTISSEMENTS LES PLUS FREQUENTS (moins graves, 8 max) : %d au total" % journal.get("nb_avertissements", 0))
        for cle, d in avert[:8]:
            ajoute("- %d fois : %s" % (d["nombre"], d["exemple"][:200]))

    # ---------------- Infos techniques ----------------
    ajoute("")
    ajoute("=" * 78)
    ajoute("INFOS TECHNIQUES (pour Claude)")
    ajoute("=" * 78)
    if res:
        ajoute("  Carte graphique : %s" % res.get("carte_graphique", ""))
        ajoute("  Carte : %s   Duree du parcours : %.0f s" % (res.get("carte", ""), float(res.get("duree_test_secondes", 0) or 0)))
    ajoute("  Code de sortie du jeu : %s   Duree totale : %.0f s" % (lancement.get("code"), float(lancement.get("duree", 0) or 0)))
    ajoute("  Journal complet : Saved\\Testeur\\journal_du_test.log")
    if not termine and journal.get("dernieres_lignes"):
        ajoute("  Dernieres lignes du journal :")
        for l in journal["dernieres_lignes"]:
            ajoute("     %s" % l[:200])
    ajoute("")
    ajoute("Pour refaire le test tout de suite : Tools\\Testeur\\TESTER_LE_JEU.bat")
    ajoute("Pour changer le parcours (personnages, zones, duree) : Plugins\\ChellesTesteur\\Testeur.txt")
    return verdict, "\n".join(L) + "\n"


def enregistrer(verdict, texte, contexte):
    os.makedirs(DOSSIER_RAPPORT, exist_ok=True)
    jour = datetime.datetime.now().strftime("%Y-%m-%d")
    for nom in ("rapport_test_%s.txt" % jour, "DERNIER_RAPPORT_TEST.txt"):
        with open(os.path.join(DOSSIER_RAPPORT, nom), "w", encoding="utf-8-sig") as f:
            f.write(texte)
    with open(os.path.join(DOSSIER_RAPPORT, "rapport_test_%s.json" % jour), "w", encoding="utf-8") as f:
        json.dump({"verdict": verdict, **{k: v for k, v in contexte.items() if k != "journal"},
                   "journal_resume": {k: v for k, v in (contexte.get("journal") or {}).items() if k != "dernieres_lignes"}},
                  f, ensure_ascii=False, indent=1, default=str)
    if os.path.isfile(JOURNAL):
        try:
            shutil.copy2(JOURNAL, os.path.join(DOSSIER_RAPPORT, "journal_%s.log" % jour))
        except OSError:
            pass
    # menage : on garde 30 jours
    limite = time.time() - GARDER_JOURS * 86400
    for ancien in glob.glob(os.path.join(DOSSIER_RAPPORT, "*_20??-??-??.*")):
        try:
            if os.path.getmtime(ancien) < limite:
                os.remove(ancien)
        except OSError:
            pass
    return os.path.join(DOSSIER_RAPPORT, "DERNIER_RAPPORT_TEST.txt")


def lire_verif():
    try:
        with open(ETAT_VERIF, "r", encoding="utf-8") as f:
            etat = json.load(f)
        if time.time() - float(etat.get("derniere_verif", 0)) < 36 * 3600:
            return etat.get("restant")
    except (OSError, ValueError):
        pass
    return None


def ouvrir(chemin):
    try:
        subprocess.Popen(["notepad.exe", chemin])
    except OSError:
        pass


# ---------------------------------------------------------------------------
def main():
    print("")
    print("  ===== CHELLES 3D - Testeur automatique (Agent 5) =====")
    reglages = lire_reglages()
    contexte = {"mode": MODE, "verif": lire_verif()}

    if AUTO and not oui(reglages.get("TestDuSoir", "Oui")):
        print("  Test du soir coupe dans Testeur.txt (TestDuSoir=Non).")
        return 0

    exe = trouver_unreal()
    dll = os.path.join(PLUGIN, "Binaries", "Win64", "UE4Editor-ChellesTesteur.dll")
    pas_lance = None
    if not exe:
        pas_lance = "Unreal Engine 4.27 introuvable sur ce PC."
    elif not os.path.isfile(UPROJECT):
        pas_lance = "Projet introuvable : %s" % UPROJECT
    elif not os.path.isfile(dll):
        pas_lance = "Le testeur n'est pas encore installe : lance Tools\\Testeur\\INSTALLER_LE_TESTEUR.bat (une seule fois)."
    elif processus_ouverts("UE4Editor.exe"):
        pas_lance = ("Unreal etait ouvert a ce moment-la : le test a ete reporte pour ne pas te gener "
                     "(et pour ne pas abimer la carte). Il se refera au prochain lancement.")

    if pas_lance:
        print("  " + pas_lance)
        contexte["pas_lance"] = pas_lance
        verdict, texte = ecrire_rapport(contexte)
        chemin = enregistrer(verdict, texte, contexte)
        if not AUTO:
            ouvrir(chemin)
        return 2

    avant = dossiers_crash()
    contexte["lancement"] = lancer_le_jeu(exe, reglages)
    time.sleep(3)
    nouveaux = sorted(dossiers_crash() - avant)
    contexte["crashs"] = [lire_crash(os.path.join(CRASHS, d)) for d in nouveaux]
    contexte["journal"] = lire_journal(JOURNAL)
    try:
        with open(RESULTATS, "r", encoding="utf-8-sig") as f:
            contexte["resultats"] = json.load(f)
    except (OSError, ValueError):
        contexte["resultats"] = None

    verdict, texte = ecrire_rapport(contexte)
    chemin = enregistrer(verdict, texte, contexte)
    print("")
    print("  RESULTAT : %s" % verdict)
    print("  Rapport : %s" % chemin)
    if not AUTO or verdict != "TOUT VA BIEN":
        ouvrir(chemin)
    return 0 if verdict == "TOUT VA BIEN" else 1


if __name__ == "__main__":
    sys.exit(main())
