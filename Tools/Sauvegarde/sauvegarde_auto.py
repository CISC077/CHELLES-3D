# -*- coding: utf-8 -*-
"""
CHELLES 3D - SAUVEGARDE AUTOMATIQUE avant chaque script (25/11/2026)

Chaque script Python de Tools appelle, tout en haut :
    sauvegarde_auto.avant(__file__)
(le bloc est pose par brancher_sauvegarde_auto.py, rien a faire a la main).

Avant que le script touche a quoi que ce soit, une "photo" du projet est prise dans :
    <Unreal Projects>\\CHELLES3D_RetourArriere\\<date>_<script>\\
Elle contient :
  - Content (et le Content des plugins) : en LIENS DURS NTFS. C'est presque instantane et
    ca ne prend pas de place : Unreal enregistre un fichier en ecrivant un fichier neuf puis
    en le mettant a la place de l'ancien, donc la photo garde l'ancienne version ;
  - Config, le .uproject et les fichiers texte (txt, ini, csv, json...) : en vraies copies,
    car ceux-la sont modifies "sur place" ;
  - les reglages texte des plugins (Armes.txt, Raccourcis.txt...) en vraies copies.
    Le code C++ (Source) et les Binaries des plugins ne sont PAS dans la photo.

Pour revenir en arriere : Tools\\Sauvegarde\\RETOUR_EN_ARRIERE.bat (Unreal ferme).
Reglages : Tools\\Sauvegarde\\sauvegarde_auto.ini

Ligne de commande (python d'Unreal, Unreal ferme ou non) :
    python sauvegarde_auto.py --maintenant [nom]
"""
from __future__ import print_function

import builtins
import io
import os
import shutil
import sys
import threading
import time

NOM_RACINE = "CHELLES3D_RetourArriere"
MARQUEUR_EN_COURS = "PHOTO_EN_COURS.txt"
FICHIER_INFO = "PHOTO_INFO.txt"
FICHIER_DERNIERE = "derniere_photo.txt"
PREFIXE_SUPPR = "_a_supprimer_"

# Extensions copiees pour de vrai (fichiers que des scripts ou Unreal modifient sur place)
EXT_COPIE = {
    ".txt", ".ini", ".csv", ".json", ".py", ".md", ".xml", ".cfg", ".log", ".bat", ".ps1",
    ".html", ".htm", ".js", ".css", ".yaml", ".yml", ".tsv",
}
# Dans un plugin, ces dossiers ne font pas partie de la photo
PLUGIN_EXCLUS = {"binaries", "intermediate", "source", ".vs", "saved"}

ICI = os.path.dirname(os.path.abspath(__file__))

_etat = {"derniere": 0.0, "protection": False}
_verrou = threading.Lock()
_garde = threading.local()


# ---------------------------------------------------------------- outils

def _log(texte):
    ligne = "[SauvegardeAuto] " + texte
    try:
        import unreal  # noqa
        unreal.log(ligne)
    except Exception:
        print(ligne)
        sys.stdout.flush()


def _long(chemin):
    """Chemin Windows long (\\\\?\\) : certains assets depassent 260 caracteres."""
    chemin = os.path.abspath(chemin)
    if os.name == "nt" and not chemin.startswith("\\\\?\\"):
        if chemin.startswith("\\\\"):
            return "\\\\?\\UNC\\" + chemin[2:]
        return "\\\\?\\" + chemin
    return chemin


def trouver_projet():
    d = ICI
    while d:
        try:
            if any(n.lower().endswith(".uproject") for n in os.listdir(d)):
                return d
        except OSError:
            pass
        parent = os.path.dirname(d)
        d = None if parent == d else parent
    try:
        import unreal
        return os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    except Exception:
        return None


def lire_reglages():
    r = {"actif": True, "garder": 5, "delai_minutes": 3.0, "exclure": set()}
    chemin = os.path.join(ICI, "sauvegarde_auto.ini")
    try:
        with io.open(chemin, "r", encoding="utf-8-sig", errors="replace") as f:
            for ligne in f:
                ligne = ligne.strip()
                if not ligne or ligne[0] in "#;[" or "=" not in ligne:
                    continue
                cle, val = [x.strip() for x in ligne.split("=", 1)]
                cle = cle.lower()
                if cle == "actif":
                    r["actif"] = val not in ("0", "non", "NON", "false", "Non")
                elif cle == "garder":
                    r["garder"] = max(1, int(val))
                elif cle == "delai_minutes":
                    r["delai_minutes"] = max(0.0, float(val.replace(",", ".")))
                elif cle == "exclure":
                    r["exclure"] = {x.strip().lower().replace(".py", "") for x in val.split(",") if x.strip()}
    except (OSError, ValueError):
        pass
    return r


def dossier_racine(projet):
    return os.path.join(os.path.dirname(projet), NOM_RACINE)


def _nom_propre(texte):
    garde = "".join(c if (c.isalnum() or c in "-_") else "_" for c in texte)
    return garde.strip("_")[:40] or "script"


# ---------------------------------------------------------------- liste des fichiers de la photo

def _parcourir(base, exclure_haut=None):
    """Rend (chemin relatif, DirEntry) pour chaque fichier sous base."""
    pile = [""]
    while pile:
        rel = pile.pop()
        try:
            it = os.scandir(_long(os.path.join(base, rel)) if rel else _long(base))
        except OSError:
            continue
        with it:
            for e in it:
                r = os.path.join(rel, e.name) if rel else e.name
                try:
                    if e.is_dir(follow_symlinks=False):
                        if exclure_haut and not rel and e.name.lower() in exclure_haut:
                            continue
                        yield r, None
                        pile.append(r)
                    elif e.is_file(follow_symlinks=False):
                        yield r, e
                except OSError:
                    continue


def sources_photo(projet):
    """Rend (chemin relatif au projet, DirEntry ou None pour un dossier)."""
    try:
        for e in os.scandir(projet):
            if e.is_file() and e.name.lower().endswith(".uproject"):
                yield e.name, e
    except OSError:
        pass
    for haut in ("Config", "Content"):
        if os.path.isdir(os.path.join(projet, haut)):
            yield haut, None
            for r, e in _parcourir(os.path.join(projet, haut)):
                yield os.path.join(haut, r), e
    plugins = os.path.join(projet, "Plugins")
    if os.path.isdir(plugins):
        yield "Plugins", None
        try:
            noms = sorted(e.name for e in os.scandir(plugins) if e.is_dir())
        except OSError:
            noms = []
        for nom in noms:
            base = os.path.join(plugins, nom)
            yield os.path.join("Plugins", nom), None
            for r, e in _parcourir(base, PLUGIN_EXCLUS):
                yield os.path.join("Plugins", nom, r), e


def est_copie(rel):
    """True = vraie copie, False = lien dur."""
    if not rel.lower().startswith("content") and not _dans_content_plugin(rel):
        return True
    return os.path.splitext(rel)[1].lower() in EXT_COPIE


def _dans_content_plugin(rel):
    parts = rel.replace("/", "\\").split("\\")
    return len(parts) > 3 and parts[0].lower() == "plugins" and parts[2].lower() == "content"


# ---------------------------------------------------------------- photo

def _copier(src, dst):
    shutil.copy2(_long(src), _long(dst))


def prendre_photo(nom_script, projet=None, nettoyer=True, barre=None):
    """Prend la photo. Rend le dossier cree (ou None)."""
    projet = projet or trouver_projet()
    if not projet:
        _log("dossier du projet introuvable, pas de sauvegarde.")
        return None
    reglages = lire_reglages()
    racine = dossier_racine(projet)
    os.makedirs(racine, exist_ok=True)
    nom = time.strftime("%Y-%m-%d_%Hh%Mm%S") + "_" + _nom_propre(nom_script)
    dossier = os.path.join(racine, nom)
    os.makedirs(dossier, exist_ok=True)
    with io.open(os.path.join(dossier, MARQUEUR_EN_COURS), "w", encoding="utf-8") as f:
        f.write(u"Photo en cours ou interrompue : ne pas utiliser.\n")

    debut = time.time()
    attendu = _lire_total_precedent(racine) or 90000
    nb_liens = nb_copies = nb_echecs = 0
    echecs = []
    vu = 0
    for rel, e in sources_photo(projet):
        dst = os.path.join(dossier, rel)
        if e is None:
            try:
                os.mkdir(_long(dst))
            except FileExistsError:
                pass
            except OSError as err:
                echecs.append("%s : %s" % (rel, err))
            continue
        src = os.path.join(projet, rel)
        vu += 1
        if barre and vu % 2000 == 0:
            barre(vu, attendu)
        try:
            if est_copie(rel):
                _copier(src, dst)
                nb_copies += 1
            else:
                try:
                    os.link(_long(src), _long(dst))
                    nb_liens += 1
                except OSError:
                    # autre disque ou fichier bloque : vraie copie si pas trop gros
                    if e.stat().st_size <= 64 * 1024 * 1024:
                        _copier(src, dst)
                        nb_copies += 1
                    else:
                        raise
        except Exception as err:
            nb_echecs += 1
            if len(echecs) < 200:
                echecs.append("%s : %s" % (rel, err))

    duree = time.time() - debut
    with io.open(os.path.join(dossier, FICHIER_INFO), "w", encoding="utf-8") as f:
        f.write(u"PROJET=%s\n" % projet)
        f.write(u"SCRIPT=%s\n" % nom_script)
        f.write(u"DATE=%s\n" % time.strftime("%d/%m/%Y %H:%M:%S"))
        f.write(u"FICHIERS=%d\n" % (nb_liens + nb_copies))
        f.write(u"LIENS=%d\nCOPIES=%d\nECHECS=%d\n" % (nb_liens, nb_copies, nb_echecs))
        f.write(u"DUREE_SECONDES=%.0f\n" % duree)
        for ligne in echecs:
            f.write(u"ECHEC=%s\n" % ligne)
    try:
        os.remove(os.path.join(dossier, MARQUEUR_EN_COURS))
    except OSError:
        pass
    with io.open(os.path.join(racine, FICHIER_DERNIERE), "w", encoding="utf-8") as f:
        f.write(u"%f\n%d\n%s\n" % (time.time(), nb_liens + nb_copies, nom))
    _log("photo prise en %.0f s : %d fichiers (%d echecs) -> %s" % (duree, nb_liens + nb_copies, nb_echecs, dossier))
    if nb_echecs:
        _log("fichiers non sauvegardes : voir %s" % os.path.join(dossier, FICHIER_INFO))
    if nettoyer:
        nettoyer_vieilles(racine, reglages["garder"])
    return dossier


def _lire_total_precedent(racine):
    try:
        with io.open(os.path.join(racine, FICHIER_DERNIERE), "r", encoding="utf-8") as f:
            lignes = f.read().split("\n")
        return int(lignes[1])
    except (OSError, ValueError, IndexError):
        return 0


def _derniere_date(racine):
    try:
        with io.open(os.path.join(racine, FICHIER_DERNIERE), "r", encoding="utf-8") as f:
            return float(f.readline().strip())
    except (OSError, ValueError):
        return 0.0


def lister_photos(racine):
    """Photos completes, la plus recente d'abord : liste de (dossier, infos dict)."""
    res = []
    try:
        noms = os.listdir(racine)
    except OSError:
        return res
    for nom in noms:
        d = os.path.join(racine, nom)
        if nom.startswith(PREFIXE_SUPPR) or nom.startswith("_") or not os.path.isdir(d):
            continue
        if os.path.exists(os.path.join(d, MARQUEUR_EN_COURS)):
            continue
        infos = {}
        try:
            with io.open(os.path.join(d, FICHIER_INFO), "r", encoding="utf-8", errors="replace") as f:
                for ligne in f:
                    if "=" in ligne:
                        k, v = ligne.rstrip("\n").split("=", 1)
                        if k != "ECHEC":
                            infos[k] = v
        except OSError:
            continue
        try:
            quand = os.path.getmtime(os.path.join(d, FICHIER_INFO))
        except OSError:
            quand = 0.0
        res.append((quand, d, infos))
    res.sort(key=lambda x: (x[0], os.path.basename(x[1])), reverse=True)
    res = [(d, infos) for _, d, infos in res]
    return res


def _supprimer(chemin):
    def _forcer(fonction, p, _exc):
        try:
            os.chmod(p, 0o666)
            fonction(p)
        except OSError:
            pass
    shutil.rmtree(_long(chemin), onerror=_forcer)


def nettoyer_vieilles(racine, garder):
    """Garde les `garder` photos les plus recentes ; les autres sont effacees en arriere-plan."""
    photos = lister_photos(racine)
    a_jeter = []
    for d, _ in photos[garder:]:
        nouveau = os.path.join(racine, PREFIXE_SUPPR + os.path.basename(d))
        try:
            os.rename(d, nouveau)
            a_jeter.append(nouveau)
        except OSError:
            pass
    # restes d'un effacement interrompu + photos interrompues de plus d'un jour
    try:
        for nom in os.listdir(racine):
            d = os.path.join(racine, nom)
            if nom.startswith(PREFIXE_SUPPR) and d not in a_jeter:
                a_jeter.append(d)
            elif os.path.exists(os.path.join(d, MARQUEUR_EN_COURS)) and time.time() - os.path.getmtime(d) > 86400:
                a_jeter.append(d)
    except OSError:
        pass
    if not a_jeter:
        return

    def _travail():
        for d in a_jeter:
            _supprimer(d)
    t = threading.Thread(target=_travail, name="ChellesNettoyagePhotos")
    t.daemon = True
    t.start()


# ---------------------------------------------------------------- protection des liens durs

def _dans_projet(chemin, projet):
    try:
        a = os.path.normcase(os.path.abspath(chemin))
        return a.startswith(os.path.normcase(projet) + os.sep)
    except Exception:
        return False


def _casser_lien(chemin):
    """Si le fichier est partage avec une photo, il est d'abord separe (copie neuve)."""
    try:
        if os.stat(chemin).st_nlink <= 1:
            return
    except OSError:
        return
    if getattr(_garde, "actif", False):
        return
    _garde.actif = True
    try:
        tmp = chemin + ".chelles_tmp"
        shutil.copy2(_long(chemin), _long(tmp))
        os.replace(_long(tmp), _long(chemin))
    except OSError as err:
        _log("impossible de separer %s : %s" % (chemin, err))
    finally:
        _garde.actif = False


def installer_protection(projet):
    """Un script qui reecrit un fichier SUR PLACE (open 'w', shutil.copy...) abimerait la photo.
    On separe donc le fichier de la photo juste avant l'ecriture."""
    if _etat["protection"]:
        return
    _etat["protection"] = True
    open_origine = builtins.open

    def open_protege(file, mode="r", *args, **kwargs):
        try:
            if isinstance(mode, str) and any(c in mode for c in "wax+") and isinstance(file, (str, bytes, os.PathLike)):
                chemin = os.fsdecode(os.fspath(file))
                if _dans_projet(chemin, projet):
                    _casser_lien(chemin)
        except Exception:
            pass
        return open_origine(file, mode, *args, **kwargs)

    open_protege.__doc__ = open_origine.__doc__
    builtins.open = open_protege
    io.open = open_protege

    os_open_origine = os.open

    def os_open_protege(path, flags, *args, **kwargs):
        try:
            if flags & (os.O_WRONLY | os.O_RDWR | os.O_APPEND | os.O_TRUNC):
                chemin = os.fsdecode(os.fspath(path))
                if _dans_projet(chemin, projet):
                    _casser_lien(chemin)
        except Exception:
            pass
        return os_open_origine(path, flags, *args, **kwargs)

    os.open = os_open_protege


# ---------------------------------------------------------------- appel depuis les scripts

def _barre_unreal():
    try:
        import unreal
        tache = unreal.ScopedSlowTask(100.0, u"Sauvegarde automatique avant le script (retour en arrière possible)...")
        tache.__enter__()
        try:
            tache.make_dialog(False)
        except Exception:
            pass
        etat = {"fait": 0.0}

        def avancer(vu, attendu):
            cible = min(99.0, 100.0 * vu / max(1, attendu))
            if cible > etat["fait"]:
                try:
                    tache.enter_progress_frame(cible - etat["fait"])
                except Exception:
                    pass
                etat["fait"] = cible
        return tache, avancer
    except Exception:
        return None, None


def avant(fichier_script="script"):
    """A appeler tout en haut d'un script qui modifie le projet. Ne plante jamais le script."""
    try:
        nom = os.path.splitext(os.path.basename(str(fichier_script)))[0] or "script"
        reglages = lire_reglages()
        if not reglages["actif"]:
            return None
        if nom.lower() in reglages["exclure"]:
            return None
        projet = trouver_projet()
        if not projet:
            return None
        with _verrou:
            racine = dossier_racine(projet)
            ecart = time.time() - max(_etat["derniere"], _derniere_date(racine))
            if ecart < reglages["delai_minutes"] * 60.0:
                _log("photo deja prise il y a %d s, pas de nouvelle photo pour %s." % (ecart, nom))
                installer_protection(projet)
                return None
            _log("photo du projet avant %s (environ 1 minute, Unreal peut sembler fige)..." % nom)
            tache, barre = _barre_unreal()
            try:
                dossier = prendre_photo(nom, projet, nettoyer=True, barre=barre)
            finally:
                if tache is not None:
                    try:
                        tache.__exit__(None, None, None)
                    except Exception:
                        pass
            _etat["derniere"] = time.time()
            installer_protection(projet)
            return dossier
    except Exception as err:
        _log("sauvegarde automatique impossible : %s (le script continue)" % err)
        return None


if __name__ == "__main__":
    if "--maintenant" in sys.argv:
        i = sys.argv.index("--maintenant")
        nom_arg = sys.argv[i + 1] if len(sys.argv) > i + 1 else "manuel"
        dernier_vu = {"t": 0.0}

        def _barre_console(vu, attendu):
            if time.time() - dernier_vu["t"] > 2:
                dernier_vu["t"] = time.time()
                print("   ... %d fichiers" % vu)
                sys.stdout.flush()
        resultat = prendre_photo(nom_arg, barre=_barre_console)
        # laisse le nettoyage en arriere-plan finir avant de quitter
        for t in threading.enumerate():
            if t.name == "ChellesNettoyagePhotos":
                t.join()
        sys.exit(0 if resultat else 1)
    print(__doc__)
