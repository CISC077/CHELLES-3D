# -*- coding: utf-8 -*-
# CHELLES 3D - Agent SKINS / HABILLAGE (25/11/2026)
# Verifie et repare les apparences de la touche K et du menu HABILLAGE (Plugins\ChellesWraith\Wraith.txt) :
#   - chaque corps existe, a un squelette, et ses materiaux sont bien la (sinon : perso tout blanc / gris)
#   - les animations ont le MEME squelette que le corps (sinon : T-pose)
#   - LENA / SOFIA / LUCIA : versions allegees (LOD) si elles n'en ont pas (plus fluide de loin et en multi)
#   - LUCIA : supprime les corps en double de l'import a la main (accord de Rachid le 24/11), seulement
#     s'ils ne servent nulle part ailleurs
# Lance par Tools\Skins\REPARER_SKINS.bat (Unreal ferme). Ecrit Tools\Skins\RAPPORT_SKINS.txt.
# Relancable sans risque.
import io
import os
import unreal

EAL = unreal.EditorAssetLibrary
PROJET = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
WRAITH_TXT = os.path.join(PROJET, "Plugins", "ChellesWraith", "Wraith.txt")
RAPPORT = os.path.join(PROJET, "Tools", "Skins", "RAPPORT_SKINS.txt")

lignes_rapport = []
problemes = []
reparations = []


def dire(texte):
    unreal.log("[Skins] " + texte)
    lignes_rapport.append(texte)


def probleme(texte):
    unreal.log_warning("[Skins] PROBLEME : " + texte)
    problemes.append(texte)


def repare(texte):
    unreal.log("[Skins] REPARE : " + texte)
    reparations.append(texte)


def memoire():
    try:
        unreal.SystemLibrary.collect_garbage()
    except Exception:
        pass


def paquet(chemin):
    """/Game/A/B.B ou /Game/A/B.B_C -> /Game/A/B"""
    return chemin.strip().split(".")[0]


def nom_de(obj):
    try:
        return obj.get_path_name() if obj else "(aucun)"
    except Exception:
        return "(?)"


def charger(chemin):
    p = paquet(chemin)
    if not p.startswith("/Game/") or not EAL.does_asset_exist(p):
        return None
    try:
        return EAL.load_asset(p)
    except Exception:
        return None


# ---------------------------------------------------------------- Wraith.txt
def lire_wraith():
    heros, skins, danses = {}, [], {}
    with io.open(WRAITH_TXT, "r", encoding="latin-1") as f:
        for ligne in f:
            ligne = ligne.strip()
            if not ligne or ligne.startswith(";") or "=" not in ligne:
                continue
            cle, valeur = ligne.split("=", 1)
            cle = cle.strip().lower()
            if cle == "heros":
                m = [x.strip() for x in valeur.split("|")]
                while len(m) < 11:
                    m.append("")
                h = {"nom": m[0], "anim": m[1], "simples": [], "mort": m[3], "montage": m[2]}
                if m[1].lower().startswith("anims:"):
                    for morceau in m[1][6:].split(";"):
                        morceau = morceau.split("@")[0].strip()
                        if morceau:
                            h["simples"].append(morceau)
                heros[m[0].lower()] = h
            elif cle == "skin":
                m = [x.strip() for x in valeur.split("|")]
                while len(m) < 7:
                    m.append("")
                skins.append({"nom": m[0], "corps": m[1], "heros": m[5], "tenue": m[6]})
            elif cle == "danses" and "|" in valeur:
                nom, liste = valeur.split("|", 1)
                danses.setdefault(nom.strip().lower(), []).extend([x.strip() for x in liste.split(";") if x.strip()])
    return heros, skins, danses


# ---------------------------------------------------------------- verifications
MATERIAUX_PAR_DEFAUT = ("WorldGridMaterial", "DefaultMaterial", "DefaultDeferredDecalMaterial")


def materiaux_manquants(corps):
    manquants = []
    try:
        for i, m in enumerate(corps.get_editor_property("materials")):
            mi = m.get_editor_property("material_interface")
            nom = nom_de(mi)
            if mi is None or any(d in nom for d in MATERIAUX_PAR_DEFAUT):
                manquants.append(i)
    except Exception as e:
        unreal.log_warning("[Skins] materiaux illisibles : %s" % e)
    return manquants


def squelette_anim_bp(chemin_classe, cache):
    p = paquet(chemin_classe)
    if p in cache:
        return cache[p]
    sq = ""
    bp = charger(p)
    if bp is None:
        cache[p] = None
        return None
    try:
        sq = nom_de(bp.get_editor_property("target_skeleton"))
    except Exception:
        sq = ""
    cache[p] = sq
    return sq


def squelette_anim(chemin):
    a = charger(chemin)
    if a is None:
        return None
    try:
        return nom_de(a.get_editor_property("skeleton"))
    except Exception:
        return ""


# Materiau a remettre si LENA / SOFIA / LUCIA sont blanches (memes choix que Tools\Personnages\creer_personnages.py)
MATERIAU_PERSO = {
    "lena": ["/Game/Personnages/LENA/texture_Mat", "/Game/Personnages/LENA/shaded_Mat"],
    "sofia": ["/Game/Personnages/SOFIA/Joueur/M_Sofia", "/Game/Personnages/SOFIA/M_Sofia"],
    "lucia": ["/Game/Personnages/LUCIA/texture_Mat", "/Game/Personnages/LUCIA/shaded_Mat"],
}


def reparer_materiaux(nom_heros, corps, manquants):
    for chemin in MATERIAU_PERSO.get(nom_heros.lower(), []):
        mat = charger(chemin)
        if mat is None:
            continue
        try:
            liste = corps.get_editor_property("materials")
            for i in manquants:
                liste[i].set_editor_property("material_interface", mat)
            corps.set_editor_property("materials", liste)
            EAL.save_loaded_asset(corps, False)
            return chemin
        except Exception as e:
            unreal.log_warning("[Skins] materiau non pose : %s" % e)
            return ""
    return ""


def alleger(nom, corps):
    """LENA / SOFIA / LUCIA : 4 niveaux de detail si le corps n'en a qu'un."""
    try:
        lib = unreal.EditorSkeletalMeshLibrary
        if lib.get_lod_count(corps) > 1:
            return False
        dire("%s : creation des versions allegees (LOD), 1 a 5 minutes..." % nom)
        if lib.regenerate_lod(corps, 4, False, False):
            EAL.save_loaded_asset(corps, False)
            return True
    except Exception as e:
        unreal.log_warning("[Skins] %s : LOD non crees (%s)" % (nom, e))
    return False


# ---------------------------------------------------------------- menage LUCIA
DOUBLONS = {
    "LUCIA": ("Lucia", ["Walking", "Crouching", "Fast_Run", "Flying", "Flying_1_", "Flying_Kick",
                        "Hip_Hop_Dancing", "Jump", "T-Pose", "lod_basic_shaded"]),
}
SUFFIXES = ["", "_Anim", "_Skeleton", "_PhysicsAsset"]


def classe(chemin):
    try:
        return str(EAL.find_asset_data(chemin).asset_class)
    except Exception:
        return ""


def nettoyer_doublons(dossier, nom_perso, noms):
    base = "/Game/Personnages/" + dossier
    if not EAL.does_asset_exist("%s/Joueur/SK_%s" % (base, nom_perso)) or \
            not EAL.does_asset_exist("%s/Joueur/Anims/A_%s_Marche_Anim" % (base, nom_perso)):
        dire("%s pas encore prete : doublons gardes." % nom_perso)
        return
    candidats = []
    for n in noms:
        for suf in SUFFIXES:
            c = "%s/%s%s" % (base, n, suf)
            if EAL.does_asset_exist(c):
                candidats.append(c)
    if not candidats:
        return
    groupe = set(candidats)
    for c in candidats:
        try:
            refs = EAL.find_package_referencers_for_asset(c, False) or []
        except Exception:
            refs = []
        autres = [r for r in refs if paquet(r) not in groupe]
        if autres:
            probleme("%s : doublons gardes, %s sert encore a %s" % (dossier, c, autres[0]))
            return
    candidats.sort(key=lambda x: 1 if classe(x) == "SkeletalMesh" else 0)
    supprimes = 0
    for c in candidats:
        try:
            if EAL.does_asset_exist(c) and EAL.delete_asset(c):
                supprimes += 1
        except Exception as e:
            unreal.log_warning("[Skins] %s non supprime : %s" % (c, e))
        memoire()  # le 24/11 Unreal a plante (memoire pleine) en les effacant d'affilee
    if supprimes:
        repare("%s : %d fichiers en double supprimes (corps inutilisables de l'import a la main, ~400 Mo)" % (dossier, supprimes))


# ---------------------------------------------------------------- programme
def main():
    if not os.path.isfile(WRAITH_TXT):
        probleme("Wraith.txt introuvable : " + WRAITH_TXT)
        return
    heros, skins, danses = lire_wraith()
    dire("%d apparences, %d heros dans Wraith.txt" % (len(skins), len(heros)))
    cache_bp = {}
    allegees = set()
    for s in skins:
        if not s["corps"] or s["corps"].lower() == "soldat":
            continue
        nom = s["nom"]
        corps = charger(s["corps"])
        if corps is None:
            probleme("%s : corps introuvable (%s). Le jeu garde le soldat d'origine pour ce skin." % (nom, paquet(s["corps"])))
            continue
        sq = corps.get_editor_property("skeleton")
        if sq is None:
            probleme("%s : le corps n'a PAS de squelette -> invisible ou T-pose. Relancez Tools\\Personnages\\creer_personnages.py." % nom)
            continue
        sq_nom = nom_de(sq)
        etat = []
        # materiaux
        if not s["tenue"]:
            manquants = materiaux_manquants(corps)
            if manquants:
                pose = reparer_materiaux(s["heros"], corps, manquants)
                if pose:
                    repare("%s : %d materiau(x) manquant(s) -> %s" % (nom, len(manquants), pose))
                else:
                    probleme("%s : %d materiau(x) manquant(s) (perso blanc/gris a ces endroits)" % (nom, len(manquants)))
        elif charger(s["tenue"]) is None:
            probleme("%s : materiau de tenue introuvable (%s)" % (nom, paquet(s["tenue"])))
        # animations
        h = heros.get(s["heros"].lower())
        if h is None:
            probleme("%s : heros '%s' absent des lignes Heros= -> soldat d'origine" % (nom, s["heros"]))
        elif h["simples"]:
            chemins = list(h["simples"]) + danses.get(h["nom"].lower(), [])
            for c in sorted(set(chemins)):
                sa = squelette_anim(c)
                if sa is None:
                    probleme("%s : animation introuvable %s" % (nom, paquet(c)))
                elif sa and sa != sq_nom:
                    probleme("%s : %s n'a pas le squelette du corps (T-pose). Relancez Tools\\Personnages\\creer_personnages.py." % (nom, paquet(c).split("/")[-1]))
            etat.append("%d animations" % len(set(chemins)))
        elif h["anim"].startswith("/Game/"):
            sb = squelette_anim_bp(h["anim"], cache_bp)
            if sb is None:
                probleme("%s : Animation Blueprint introuvable (%s)" % (nom, paquet(h["anim"])))
            elif sb and sb != sq_nom:
                probleme("%s : squelette du corps (%s) different de celui de l'Animation Blueprint (%s) -> T-pose" % (nom, sq_nom, sb))
            else:
                etat.append("Animation Blueprint OK")
        # fluidite
        try:
            lib = unreal.EditorSkeletalMeshLibrary
            lods = lib.get_lod_count(corps)
            points = lib.get_num_verts(corps, 0)
            etat.append("%d LOD, %d points" % (lods, points))
            if h is not None and h["simples"] and paquet(s["corps"]) not in allegees:
                allegees.add(paquet(s["corps"]))
                if alleger(nom, corps):
                    repare("%s : 4 versions allegees (LOD) ajoutees" % nom)
        except Exception:
            pass
        dire("OK  %-30s %s" % (nom, ", ".join(etat)))
        memoire()
    for dossier, (nom_perso, noms) in DOUBLONS.items():
        nettoyer_doublons(dossier, nom_perso, noms)
    try:
        EAL.save_directory("/Game/Personnages", only_if_is_dirty=True, recursive=True)
    except Exception:
        pass


try:
    main()
except Exception as e:
    probleme("erreur du script : %s" % e)

texte = ["CHELLES 3D - verification des skins et de l'habillage", ""]
texte.append("REPARE AUTOMATIQUEMENT : %d" % len(reparations))
texte += ["  - " + r for r in reparations]
texte.append("")
texte.append("PROBLEMES RESTANTS : %d" % len(problemes))
texte += ["  - " + p for p in problemes]
texte.append("")
texte.append("DETAIL :")
texte += ["  " + l for l in lignes_rapport]
try:
    with io.open(RAPPORT, "w", encoding="utf-8-sig") as f:
        f.write("\r\n".join(texte) + "\r\n")
except Exception as e:
    unreal.log_warning("[Skins] rapport non ecrit : %s" % e)
unreal.log("[Skins] TERMINE : %d repare(s), %d probleme(s). Rapport : %s" % (len(reparations), len(problemes), RAPPORT))
