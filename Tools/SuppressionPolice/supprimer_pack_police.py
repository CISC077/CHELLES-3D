# -*- coding: utf-8 -*-
"""SUPPRESSION du pack "Sports and Police Cars - Drivable" (/Game/SportVehiclesPack) - CHELLES 3D.

Lance par SUPPRIMER_PACK_POLICE.bat (Unreal ferme au depart). Ce script, dans Unreal :
  1. retire de la carte Rachid tous les acteurs qui utilisent le pack (voitures, morceaux de voiture), puis enregistre ;
  2. corrige les reglages texte qui pointent vers le pack (copie de chaque fichier avant, dans le dossier de sauvegarde) :
       Config/DefaultGame.ini (cuisson), Config/DefaultEngine.ini (redirections),
       ChellesVolVoiture/VolVoiture.txt, ChellesPolice/Police.txt, ChellesSonsVehicules/SonsVehicules.txt ;
  3. liste les fichiers du projet qui utilisent encore le pack (rapport) ;
  4. ecrit OK dans etat.txt. Le .bat efface ensuite le dossier Content/SportVehiclesPack, Unreal ferme.
Journal : Tools/SuppressionPolice/suppression.log
"""
import os
import shutil
import time
import traceback

import unreal

PACK = "/Game/SportVehiclesPack"
CARTE = "/Game/Maps/Rachid"
VOITURE_SPORT = "/Game/VehicleVarietyPack/Blueprints/SportsCar/BP_SportsCar.BP_SportsCar_C"
SON_MOTEUR = "/Game/DV_Car_Horo/Sounds/Cues/SC_Car_Hatchback_Horo_Engine"

ell = unreal.EditorLevelLibrary
PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ICI = os.path.join(PROJET, "Tools", "SuppressionPolice")
JOURNAL = os.path.join(ICI, "suppression.log")
ETAT = os.path.join(ICI, "etat.txt")
SAUVEGARDE = os.environ.get("CHELLES_SAUVEGARDE") or os.path.join(PROJET, "_Sauvegardes", "SuppressionPolice_" + time.strftime("%Y-%m-%d_%Hh%M"))


def log(msg):
    unreal.log("[SuppressionPolice] " + str(msg))
    try:
        os.makedirs(ICI, exist_ok=True)
        with open(JOURNAL, "a", encoding="utf-8") as f:
            f.write(time.strftime("%H:%M:%S ") + str(msg) + "\n")
    except Exception:
        pass


def chemin(obj):
    try:
        return obj.get_path_name() if obj else ""
    except Exception:
        return ""


def dans_pack(p):
    return p.lower().startswith(PACK.lower() + "/")


# ---------------------------------------------------------------- 1. carte Rachid
def references_acteur(acteur):
    refs = [chemin(acteur.get_class())]
    composants = []
    try:
        composants = acteur.get_components_by_class(unreal.ActorComponent)
    except Exception:
        pass
    for c in composants:
        refs.append(chemin(c.get_class()))
        for prop in ("static_mesh", "skeletal_mesh", "sound", "template", "child_actor_class"):
            try:
                refs.append(chemin(c.get_editor_property(prop)))
            except Exception:
                pass
        try:
            for m in c.get_materials():
                refs.append(chemin(m))
        except Exception:
            pass
    return [r for r in refs if r]


def nettoyer_carte():
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        ell.load_level(CARTE)
    a_retirer = []
    for a in ell.get_all_level_actors():
        try:
            refs = [r for r in references_acteur(a) if dans_pack(r)]
        except Exception:
            continue
        if refs:
            a_retirer.append((a, refs[0]))
    for a, r in a_retirer:
        log("  retire de Rachid : %s (%s)" % (a.get_actor_label(), r))
        ell.destroy_actor(a)
    log("%d acteur(s) du pack retire(s) de Rachid." % len(a_retirer))
    if a_retirer:
        if ell.save_current_level():
            log("Rachid enregistree.")
        else:
            raise RuntimeError("Rachid n'a pas pu etre enregistree (un autre Unreal est ouvert ?)")


# ---------------------------------------------------------------- 2. reglages texte
def sauver_copie(fichier):
    rel = os.path.relpath(fichier, PROJET)
    dst = os.path.join(SAUVEGARDE, rel)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if not os.path.exists(dst):
        shutil.copy2(fichier, dst)


def corriger(rel, regle):
    """regle(ligne_sans_fin) -> nouvelle ligne, ou None pour la supprimer."""
    fichier = os.path.join(PROJET, rel)
    if not os.path.exists(fichier):
        log("  (absent) " + rel)
        return
    brut = open(fichier, "rb").read()
    bom = brut.startswith(b"\xef\xbb\xbf")
    texte = brut.decode("utf-8-sig")
    lignes = texte.splitlines(True)
    sortie, change = [], 0
    for l in lignes:
        fin = "\r\n" if l.endswith("\r\n") else ("\n" if l.endswith("\n") else "")
        corps = l[: len(l) - len(fin)]
        nouveau = regle(corps)
        if nouveau is None:
            change += 1
            continue
        if nouveau != corps:
            change += 1
        sortie.append(nouveau + fin)
    if change:
        sauver_copie(fichier)
        data = "".join(sortie).encode("utf-8")
        open(fichier, "wb").write((b"\xef\xbb\xbf" if bom else b"") + data)
    log("  %s : %d ligne(s) corrigee(s)" % (rel, change))


def cle(corps):
    s = corps.strip()
    if s.startswith("#") or s.startswith(";") or "=" not in s:
        return ""
    return s.split("=", 1)[0].strip().lower()


def regle_ini(corps):
    if "/game/sportvehiclespack" in corps.lower() and not corps.strip().startswith(";"):
        return None
    return corps


def regle_vol(corps):
    k = cle(corps)
    if k == "secours":
        return "Secours ="
    if k == "voiturepolice":
        return "VoiturePolice ="
    return corps


def regle_police(corps):
    k = cle(corps)
    if k == "classes":
        return "Classes = BP_SportsCar"
    if k == "voitureaposer":
        return "VoitureAPoser = " + VOITURE_SPORT
    return corps


def regle_sons(corps):
    if cle(corps) == "voiture.moteur" and "sportvehiclespack" in corps.lower():
        return "Voiture.Moteur=" + SON_MOTEUR
    return corps


def corriger_reglages():
    corriger(os.path.join("Config", "DefaultGame.ini"), regle_ini)
    corriger(os.path.join("Config", "DefaultEngine.ini"), regle_ini)
    corriger(os.path.join("Plugins", "ChellesVolVoiture", "VolVoiture.txt"), regle_vol)
    corriger(os.path.join("Plugins", "ChellesPolice", "Police.txt"), regle_police)
    corriger(os.path.join("Plugins", "ChellesSonsVehicules", "SonsVehicules.txt"), regle_sons)
    # Autres reglages qui citeraient encore le pack : juste signales
    for dossier in ("Config", "Plugins"):
        base = os.path.join(PROJET, dossier)
        for racine, dirs, fichiers in os.walk(base):
            dirs[:] = [d for d in dirs if d not in ("Binaries", "Intermediate", "Source")]
            for f in fichiers:
                if not f.lower().endswith((".ini", ".txt", ".csv")):
                    continue
                p = os.path.join(racine, f)
                try:
                    if "sportvehiclespack" in open(p, encoding="utf-8-sig", errors="ignore").read().lower():
                        log("  A VERIFIER (cite encore le pack) : " + os.path.relpath(p, PROJET))
                except Exception:
                    pass


# ---------------------------------------------------------------- 3. qui utilise encore le pack
def rapport_utilisateurs():
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    try:
        ar.scan_paths_synchronous(["/Game/Maps"], True)
    except Exception:
        pass
    opts = unreal.AssetRegistryDependencyOptions()
    for nom, val in (("include_soft_package_references", True), ("include_hard_package_references", True),
                     ("include_searchable_names", False), ("include_soft_management_references", False),
                     ("include_hard_management_references", False)):
        try:
            opts.set_editor_property(nom, val)
        except Exception:
            pass
    paquets = sorted({str(a.package_name) for a in ar.get_assets_by_path(PACK, recursive=True)})
    log("Le pack contient %d fichier(s)." % len(paquets))
    dehors = {}
    for p in paquets:
        try:
            refs = ar.get_referencers(p, opts) or []
        except Exception:
            refs = []
        for r in refs:
            r = str(r)
            if not dans_pack(r) and r != PACK:
                dehors.setdefault(r, []).append(p)
    if not dehors:
        log("Plus aucun fichier hors du pack ne l'utilise.")
    for r, ps in sorted(dehors.items()):
        log("  UTILISE ENCORE LE PACK : %s -> %s%s" % (r, ps[0], " (+%d)" % (len(ps) - 1) if len(ps) > 1 else ""))
    return dehors


def main():
    log("==== SUPPRESSION DU PACK SPORTS AND POLICE CARS ====")
    log("Sauvegarde : " + SAUVEGARDE)
    if os.path.exists(ETAT):
        os.remove(ETAT)
    nettoyer_carte()
    corriger_reglages()
    dehors = rapport_utilisateurs()
    if CARTE in dehors:
        raise RuntimeError("Rachid utilise encore le pack : suppression annulee (rien n'est efface).")
    with open(ETAT, "w", encoding="utf-8") as f:
        f.write("OK\n")
    log("Pret : le dossier du pack va etre efface.")


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())

if os.environ.get("CHELLES_QUITTER") == "1":
    unreal.SystemLibrary.quit_editor()
