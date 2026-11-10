# -*- coding: utf-8 -*-
"""INSTALLATION DE L'EVENEMENT DE NOEL - lance par INSTALLER_NOEL.bat (Unreal ferme).

Ce que fait ce script, dans la carte /Game/Maps/Rachid :
  1. si la neige n'est pas encore installee : lance Tools/Neige/ajouter_neige.py (flocons, sol de neige,
     empreintes de pas ; il fait lui-meme une copie de sauvegarde de Rachid dans Saved/Sauvegardes/) ;
  2. ajoute le parametre "SaisonNeige" a /Game/Chelles/Neige/MPC_Empreintes (0 = sol normal, 1 = sol blanc) ;
  3. cree un nouveau materiau de sol /Game/Chelles/Noel/M_Sol_Noel_<nom> = COPIE du materiau d'origine
     du Landscape + la neige par-dessus, qui n'apparait que quand "SaisonNeige" vaut 1.
     Le materiau d'origine n'est PAS modifie ;
  4. donne ce materiau au Landscape et le tag "ChellesNoelSaison" aux acteurs Chelles_Neige_* :
     le plugin ChellesNoel les montre seulement du 1er decembre au 1er janvier ;
  5. enregistre Rachid.

Pour tout enlever : RETIRER_NOEL.bat (rend au Landscape son materiau d'origine).
Journal : Tools/Noel/installer_noel.log
"""
import os
import runpy
import traceback

import unreal

CARTE = "/Game/Maps/Rachid"
DOSSIER = "/Game/Chelles/Noel"
# le parametre de saison est range dans la collection des empreintes (deja lue par le sol de neige) :
# un materiau Unreal ne peut pas lire plus de 2 collections.
MPC = "/Game/Chelles/Neige/MPC_Empreintes"
PARAMETRE = "SaisonNeige"
SOL_NEIGE = "/Game/Chelles/Neige/M_Sol_Neige"
PREFIXE_NEIGE = "Chelles_Neige_"
TAG_SAISON = "ChellesNoelSaison"
TAG_ANCIEN = "ChellesNeigeAncienSol="   # pose par ajouter_neige.py : materiau d'origine du sol

eal = unreal.EditorAssetLibrary
ell = unreal.EditorLevelLibrary
DOSSIER_SCRIPT = os.path.join(unreal.SystemLibrary.get_project_directory(), "Tools", "Noel")
JOURNAL = os.path.join(DOSSIER_SCRIPT, "installer_noel.log")
open(JOURNAL, "w").write("")


def log(msg):
    unreal.log("[Noel] " + str(msg))
    with open(JOURNAL, "a", encoding="utf-8") as f:
        f.write(str(msg) + "\n")


def tags_de(acteur):
    return [str(t) for t in acteur.get_editor_property("tags")]


def mettre_tags(acteur, tags):
    acteur.set_editor_property("tags", [unreal.Name(t) for t in tags])


def paysages():
    return [a for a in ell.get_all_level_actors() if isinstance(a, unreal.LandscapeProxy)]


def neige_installee():
    labels = [a.get_actor_label() for a in ell.get_all_level_actors()]
    if PREFIXE_NEIGE + "Proche" not in labels or not eal.does_asset_exist(SOL_NEIGE):
        return False
    return all(any(t.startswith(TAG_ANCIEN) for t in tags_de(p)) for p in paysages())


def obtenir_mpc():
    chemin = MPC
    mpc = eal.load_asset(chemin) if eal.does_asset_exist(chemin) else None
    if mpc is None or not unreal.NoelOutils.ajouter_scalaire_collection(mpc, PARAMETRE, 0.0):
        raise RuntimeError("impossible d'ajouter %s a %s" % (PARAMETRE, chemin))
    eal.save_loaded_asset(mpc)
    return mpc


def dupliquer(source, nom):
    chemin = "%s/%s" % (DOSSIER, nom)
    if eal.does_asset_exist(chemin):
        eal.delete_asset(chemin)
    copie = eal.duplicate_asset(source.get_path_name(), chemin)
    if copie is None:
        raise RuntimeError("copie impossible : %s -> %s" % (source.get_path_name(), chemin))
    return copie


def changer_parent(instance, parent):
    try:
        unreal.MaterialEditingLibrary.set_material_instance_parent(instance, parent)
    except Exception:
        instance.set_editor_property("parent", parent)


def sol_de_noel(origine, neige, mpc):
    """Copie du materiau d'origine (et de ses Material Instances) + neige qui suit SaisonNeige."""
    base = origine.get_base_material()
    if base is None:
        raise RuntimeError("materiau de base introuvable pour %s" % origine.get_path_name())
    m = dupliquer(base, "M_Sol_Noel_" + base.get_name())
    erreur = unreal.NoelOutils.melanger_neige(m, neige, mpc, PARAMETRE)
    if erreur:
        raise RuntimeError("melange impossible : %s" % erreur)
    eal.save_loaded_asset(m)
    log("   materiau %s cree (copie de %s + neige)" % (m.get_path_name(), base.get_path_name()))
    # si le sol utilisait une Material Instance, on recopie la chaine d'instances sur le nouveau materiau
    chaine = []
    courant = origine
    while isinstance(courant, unreal.MaterialInstance):
        chaine.append(courant)
        courant = courant.get_editor_property("parent")
    parent = m
    for mi in reversed(chaine):
        copie = dupliquer(mi, "MI_Sol_Noel_" + mi.get_name())
        changer_parent(copie, parent)
        eal.save_loaded_asset(copie)
        log("   instance %s creee (copie de %s)" % (copie.get_path_name(), mi.get_path_name()))
        parent = copie
    return parent


def main():
    if not hasattr(unreal, "NoelOutils"):
        log("ABANDON : le plugin ChellesNoel n'est pas compile. Relancez INSTALLER_NOEL.bat (il le compile). "
            "Rien n'a ete modifie.")
        return
    monde = ell.get_editor_world()
    if monde is None or monde.get_name() != "Rachid":
        log("chargement de %s ..." % CARTE)
        ell.load_level(CARTE)
    log("Carte ouverte : %s" % ell.get_editor_world().get_path_name())

    # ---- 1. neige (script existant)
    if neige_installee():
        log("1/5 neige deja installee (flocons, sol, empreintes) : on la garde")
    else:
        log("1/5 neige pas encore installee : lancement de Tools/Neige/ajouter_neige.py ...")
        script = os.path.join(os.path.dirname(DOSSIER_SCRIPT), "Neige", "ajouter_neige.py")
        module = runpy.run_path(script, run_name="neige_pour_noel")
        module["executer_dans_unreal"]()
        journal_neige = os.path.join(os.path.dirname(DOSSIER_SCRIPT), "out", "neige.log")
        texte = open(journal_neige, encoding="utf-8", errors="replace").read() if os.path.exists(journal_neige) else ""
        for ligne in texte.strip().splitlines():
            log("   neige : " + ligne)
        if "TERMINE" not in texte or not neige_installee():
            log("ABANDON : la neige ne s'est pas installee (voir les lignes 'neige' ci-dessus). "
                "Envoyez ce journal a Claude.")
            return

    # ---- 2. collection
    mpc = obtenir_mpc()
    log("2/5 %s : parametre %s ajoute (0 par defaut : sol normal dans l'editeur)" % (MPC, PARAMETRE))

    # ---- 3 et 4. sol de Noel sur chaque Landscape
    neige = eal.load_asset(SOL_NEIGE)
    nb_sols = 0
    for p in paysages():
        tags = tags_de(p)
        note = [t for t in tags if t.startswith(TAG_ANCIEN)]
        chemin = note[0][len(TAG_ANCIEN):] if note else ""
        origine = eal.load_asset(chemin) if chemin else None
        if origine is None:
            log("3/5 Landscape '%s' : materiau d'origine inconnu (%s), sol laisse tel quel" % (p.get_actor_label(), chemin))
            continue
        log("3/5 Landscape '%s' : materiau d'origine %s" % (p.get_actor_label(), chemin))
        nouveau = sol_de_noel(origine, neige, mpc)
        p.set_editor_property("landscape_material", nouveau)
        if TAG_SAISON not in tags:
            mettre_tags(p, tags + [TAG_SAISON])
        log("4/5 Landscape '%s' : nouveau materiau %s" % (p.get_actor_label(), nouveau.get_path_name()))
        nb_sols += 1

    nb = 0
    for a in ell.get_all_level_actors():
        if a.get_actor_label().startswith(PREFIXE_NEIGE):
            tags = tags_de(a)
            if TAG_SAISON not in tags:
                mettre_tags(a, tags + [TAG_SAISON])
            nb += 1
    log("4/5 %d acteur(s) de neige marques '%s' (montres seulement pendant Noel)" % (nb, TAG_SAISON))

    # ---- 5. enregistrement
    if ell.save_current_level():
        log("5/5 Rachid enregistree.")
    else:
        log("5/5 ATTENTION : Rachid n'a pas pu etre enregistree.")
    log("TERMINE : %d sol(s) de Noel, %d acteur(s) de neige. Ouvrez le projet et cliquez sur Play. "
        "Pour tester avant decembre : Tools/Noel/TESTER_NOEL_MAINTENANT.bat" % (nb_sols, nb))


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())
