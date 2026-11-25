# -*- coding: utf-8 -*-
"""
CHELLES 3D - RETOUR EN ARRIERE (25/11/2026)
Lance par RETOUR_EN_ARRIERE.bat, Unreal FERME.

Remet le projet comme sur une photo prise par sauvegarde_auto.py avant un script :
  - les fichiers modifies depuis la photo reprennent leur ancienne version ;
  - les fichiers apparus depuis sont mis de cote dans
    CHELLES3D_RetourArriere\\_Retires_<date>\\ (rien n'est efface pour de bon) ;
  - juste avant, une photo "avant_retour" de l'etat actuel est prise :
    on peut donc annuler le retour en relancant RETOUR_EN_ARRIERE.bat.
Le code C++ (Source) et les Binaries des plugins ne sont pas touches.
"""
from __future__ import print_function

import io
import os
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sauvegarde_auto as sa  # noqa: E402


def saisir(question):
    try:
        return input(question)
    except EOFError:
        return ""


def il_y_a(nom_dossier):
    try:
        t = time.mktime(time.strptime(nom_dossier[:19], "%Y-%m-%d_%Hh%Mm%S"))
    except ValueError:
        return ""
    m = int((time.time() - t) / 60)
    if m < 60:
        return "il y a %d min" % m
    if m < 48 * 60:
        return "il y a %d h %02d" % (m // 60, m % 60)
    return "il y a %d jours" % (m // 1440)


def meme_fichier(a, b):
    try:
        sa_, sb = os.stat(sa._long(a)), os.stat(sa._long(b))
    except OSError:
        return False
    if sa_.st_ino and sa_.st_ino == sb.st_ino and sa_.st_dev == sb.st_dev:
        return True  # meme fichier (lien dur)
    return sa_.st_size == sb.st_size and abs(sa_.st_mtime_ns - sb.st_mtime_ns) < 2000


def remettre(photo, projet, rel):
    src = os.path.join(photo, rel)
    dst = os.path.join(projet, rel)
    if os.path.exists(sa._long(dst)):
        if meme_fichier(src, dst):
            return False
        try:
            os.chmod(sa._long(dst), 0o666)
        except OSError:
            pass
        os.remove(sa._long(dst))
    else:
        parent = os.path.dirname(dst)
        if not os.path.isdir(sa._long(parent)):
            os.makedirs(sa._long(parent), exist_ok=True)
    if sa.est_copie(rel):
        shutil.copy2(sa._long(src), sa._long(dst))
    else:
        try:
            os.link(sa._long(src), sa._long(dst))
        except OSError:
            shutil.copy2(sa._long(src), sa._long(dst))
    return True


def main():
    projet = sa.trouver_projet()
    if not projet:
        print("Dossier du projet introuvable.")
        return 1
    racine = sa.dossier_racine(projet)
    photos = sa.lister_photos(racine)
    print("=" * 66)
    print("  RETOUR EN ARRIERE - CHELLES 3D")
    print("=" * 66)
    print("Projet : %s" % projet)
    print("")
    if not photos:
        print("Aucune photo automatique pour l'instant.")
        print("Elles sont prises toutes seules avant chaque script de Tools.")
        return 1
    print("Photos disponibles (la plus recente en premier) :")
    print("")
    for i, (d, infos) in enumerate(photos, 1):
        nom = os.path.basename(d)
        script = infos.get("SCRIPT", "?")
        quoi = "juste avant ton dernier retour en arriere" if script == "avant_retour" else "avant le script %s" % script
        print("  %d. %s  -  %s  (%s)" % (i, infos.get("DATE", nom[:19]), quoi, il_y_a(nom)))
    print("")
    choix = saisir("Appuie sur ENTREE pour revenir a la photo 1, ou tape un autre numero : ").strip()
    if not choix:
        choix = "1"
    if not choix.isdigit() or not (1 <= int(choix) <= len(photos)):
        print("Numero invalide. Rien n'a ete modifie.")
        return 1
    photo, infos = photos[int(choix) - 1]
    print("")
    print("Le projet va revenir a l'etat du %s." % infos.get("DATE", "?"))
    print("Ce qui a ete ajoute depuis sera MIS DE COTE (pas efface).")
    rep = saisir("Tape O puis ENTREE pour confirmer : ").strip().upper()
    if rep not in ("O", "OUI"):
        print("Retour annule. Rien n'a ete modifie.")
        return 0

    # 1. photo de securite de l'etat actuel (pour pouvoir annuler le retour)
    print("")
    print("[1/3] Photo de securite de l'etat actuel (environ 1 minute)...")
    sys.stdout.flush()
    sa.prendre_photo("avant_retour", projet, nettoyer=False)

    # 2. remettre les fichiers de la photo
    print("[2/3] Remise des fichiers de la photo...")
    sys.stdout.flush()
    dans_photo = set()
    remis = echecs = 0
    erreurs = []
    for rel, e in sa._parcourir(photo):
        if e is None:
            dans_photo.add(os.path.normcase(rel))
            continue
        if rel in (sa.FICHIER_INFO, sa.MARQUEUR_EN_COURS):
            continue
        dans_photo.add(os.path.normcase(rel))
        try:
            if remettre(photo, projet, rel):
                remis += 1
        except Exception as err:
            echecs += 1
            if len(erreurs) < 50:
                erreurs.append("%s : %s" % (rel, err))

    # 3. mettre de cote ce qui est apparu depuis la photo
    print("[3/3] Mise de cote des fichiers ajoutes depuis...")
    sys.stdout.flush()
    cote = os.path.join(racine, "_Retires_" + time.strftime("%Y-%m-%d_%Hh%Mm%S"))
    retires = 0
    dossiers_vus = []
    plugins_photo = {os.path.normcase(r) for r in dans_photo if r.count(os.sep) == 1 and r.lower().startswith("plugins")}
    for rel, e in sa.sources_photo(projet):
        cle = os.path.normcase(rel)
        if e is None:
            dossiers_vus.append(rel)
            continue
        if cle in dans_photo:
            continue
        # un plugin entier ajoute apres la photo : on n'y touche pas (code installe a la main)
        parts = rel.split(os.sep)
        if parts[0].lower() == "plugins" and len(parts) > 1 and os.path.normcase(os.path.join(parts[0], parts[1])) not in plugins_photo:
            continue
        try:
            cible = os.path.join(cote, rel)
            os.makedirs(sa._long(os.path.dirname(cible)), exist_ok=True)
            os.rename(sa._long(os.path.join(projet, rel)), sa._long(cible))
            retires += 1
        except Exception as err:
            echecs += 1
            if len(erreurs) < 50:
                erreurs.append("%s : %s" % (rel, err))
    # dossiers vides apparus depuis la photo
    for rel in sorted(dossiers_vus, key=len, reverse=True):
        if os.path.normcase(rel) in dans_photo:
            continue
        try:
            os.rmdir(sa._long(os.path.join(projet, rel)))
        except OSError:
            pass

    print("")
    print("=" * 66)
    print("  RETOUR EN ARRIERE TERMINE")
    print("=" * 66)
    print("%d fichier(s) remis comme avant, %d fichier(s) mis de cote." % (remis, retires))
    if retires:
        print("Fichiers mis de cote : %s" % cote)
    if echecs:
        print("")
        print("[ATTENTION] %d fichier(s) n'ont pas pu etre traites :" % echecs)
        for ligne in erreurs[:15]:
            print("   " + ligne)
        print("Ferme bien Unreal puis relance RETOUR_EN_ARRIERE.bat.")
    print("")
    print("Tu peux rouvrir Unreal.")
    print("Pour ANNULER ce retour : relance RETOUR_EN_ARRIERE.bat et choisis la photo")
    print("'avant_retour' (la 1re de la liste).")
    with io.open(os.path.join(racine, "journal_retours.txt"), "a", encoding="utf-8") as f:
        f.write(u"%s : retour a %s, %d remis, %d mis de cote, %d echecs\n" % (
            time.strftime("%d/%m/%Y %H:%M:%S"), os.path.basename(photo), remis, retires, echecs))
    return 0 if not echecs else 2


if __name__ == "__main__":
    sys.exit(main())
