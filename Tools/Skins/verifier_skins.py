# -*- coding: utf-8 -*-
# >>> SAUVEGARDE AUTO CHELLES 3D (pose par Tools\Sauvegarde\brancher_sauvegarde_auto.py, ne pas modifier)
try:
    import os as _sa_os, sys as _sa_sys
    _sa_d = _sa_os.path.dirname(_sa_os.path.abspath(globals().get('__file__') or '.'))
    _sa_m = None
    while _sa_d:
        if _sa_os.path.isfile(_sa_os.path.join(_sa_d, 'Tools', 'Sauvegarde', 'sauvegarde_auto.py')):
            _sa_m = _sa_os.path.join(_sa_d, 'Tools', 'Sauvegarde')
            break
        _sa_p = _sa_os.path.dirname(_sa_d)
        _sa_d = None if _sa_p == _sa_d else _sa_p
    if _sa_m is None:
        import unreal as _sa_u
        _sa_m = _sa_os.path.join(_sa_u.Paths.convert_relative_path_to_full(_sa_u.Paths.project_dir()), 'Tools', 'Sauvegarde')
    if _sa_m not in _sa_sys.path:
        _sa_sys.path.insert(0, _sa_m)
    import sauvegarde_auto as _sa_mod
    _sa_mod.avant(globals().get('__file__') or 'script')
except Exception as _sa_e:
    print('[SauvegardeAuto] non faite : %s' % _sa_e)
# <<< SAUVEGARDE AUTO CHELLES 3D
# CHELLES 3D - Agent SKINS / HABILLAGE (25/11/2026)
# Verifie et repare les apparences de la touche K et du menu HABILLAGE (Plugins\ChellesWraith\Wraith.txt) :
#   - chaque corps existe, a un squelette, et ses materiaux sont bien la (sinon : perso tout blanc / gris)
#   - les animations ont le MEME squelette que le corps (sinon : T-pose)
#   - LENA / SOFIA / LUCIA : versions allegees (LOD) si elles n'en ont pas (plus fluide de loin et en multi)
#   - LUCIA : supprime les corps en double de l'import a la main (accord de Rachid le 24/11), seulement
#     s'ils ne servent nulle part ailleurs
#   - heros PARAGON (25/11) : ajoute ceux de Content dans Wraith.txt avec leurs skins et leurs emotes (danses J)
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


CACHE_SQ_ANIM = {}


def squelette_anim(chemin):
    if chemin in CACHE_SQ_ANIM:
        return CACHE_SQ_ANIM[chemin]
    CACHE_SQ_ANIM[chemin] = _squelette_anim(chemin)
    return CACHE_SQ_ANIM[chemin]


def _squelette_anim(chemin):
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
# ---------------------------------------------------------------- heros Paragon (25/11)
# Ajoute dans Wraith.txt les heros Paragon presents dans Content (Greystone, Zinx, Terra, Iggy, Yin, Wukong, Kwang,
# Shinbi, Phase, Countess, Steel...) : ligne Heros=, leurs skins (Skin=) et leurs emotes (Danses=, touche J et apercu
# du menu). Lit seulement l'Asset Registry : AUCUN modele n'est charge ici (pas de memoire pleine).
SKINS_PAR_HEROS = 5        # corps de base + 4 skins au plus par heros (sinon la touche K devient tres longue)
DANSES_PAR_HEROS = 6
COULEURS_CARTES = ["0.55,0.45,0.30", "0.20,0.45,0.75", "0.35,0.55,0.20", "0.75,0.30,0.20", "0.55,0.25,0.65",
                   "0.80,0.60,0.10", "0.25,0.60,0.60", "0.70,0.20,0.45", "0.40,0.40,0.80", "0.60,0.15,0.15",
                   "0.45,0.65,0.35", "0.30,0.30,0.30"]


def _chemin_tag(texte):
    """Skeleton'/Game/A/B.B'  ->  /Game/A/B.B"""
    texte = str(texte or "")
    if "'" in texte:
        morceaux = texte.split("'")
        if len(morceaux) >= 2:
            texte = morceaux[1]
    return texte.strip()


def _tag(ad, nom):
    v = None
    try:
        v = ad.get_tag_value(nom)
    except Exception:
        try:
            v = unreal.AssetRegistryHelpers.get_tag_value(ad, nom)
        except Exception:
            v = None
    if isinstance(v, tuple):
        v = v[-1] if v and v[0] else None
    return _chemin_tag(v) if v else ""


def _ascii(texte):
    import unicodedata
    texte = unicodedata.normalize("NFKD", texte).encode("ascii", "ignore").decode("ascii")
    return "".join(c for c in texte if c not in "|;=").strip()


def _majuscules(nom):
    """WhiteTiger -> WHITE TIGER, Tier_1 -> TIER 1"""
    sortie = ""
    for i, c in enumerate(nom.replace("_", " ")):
        if i and c.isupper() and nom[i - 1].islower():
            sortie += " "
        sortie += c
    return _ascii(" ".join(sortie.split()).upper())


def _assets(ar, chemin, recursif=True):
    try:
        return list(ar.get_assets_by_path(chemin, recursive=recursif))
    except Exception:
        return []


def _classe(ad):
    try:
        return str(ad.asset_class)
    except Exception:
        return ""


def _objet(ad):
    return "%s.%s" % (ad.package_name, ad.asset_name)


def chercher_heros_paragon():
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    try:
        ar.search_all_assets(True)
    except Exception:
        pass
    contenu = os.path.join(PROJET, "Content")
    packs = ["/Game/" + d for d in os.listdir(contenu) if os.path.isdir(os.path.join(contenu, d))] if os.path.isdir(contenu) else []
    trouves = []
    for pack in sorted(packs):
        if not pack.split("/")[-1].lower().startswith("paragon"):
            continue
        base_heros = pack + "/Characters/Heroes"
        tout = _assets(ar, base_heros)
        dossiers = {}
        for ad in tout:
            chemin = str(ad.package_path)
            if not chemin.startswith(base_heros + "/"):
                continue
            nom_dossier = chemin[len(base_heros) + 1:].split("/")[0]
            dossiers.setdefault(nom_dossier, []).append(ad)
        for nom, liste in sorted(dossiers.items()):
            racine = base_heros + "/" + nom
            bps = [a for a in liste if _classe(a) == "AnimBlueprint" and str(a.package_path) == racine]
            bps.sort(key=lambda a: (not str(a.asset_name).lower().startswith(nom.lower()), "anim" not in str(a.asset_name).lower(), str(a.asset_name)))
            if not bps:
                continue
            bp = bps[0]
            sq = _tag(bp, "TargetSkeleton")

            def meme_squelette(a):
                s = _tag(a, "Skeleton")
                return not sq or not s or s == sq

            maillages = [a for a in liste if _classe(a) == "SkeletalMesh" and meme_squelette(a)]
            base = [a for a in maillages if str(a.package_path) == racine + "/Meshes"]
            base.sort(key=lambda a: (str(a.asset_name).lower() != nom.lower(), len(str(a.asset_name))))
            if not base:
                continue
            skins = {}
            for a in maillages:
                p = str(a.package_path)
                if "/Skins/" not in p + "/":
                    continue
                dossier = p.split("/Skins/", 1)[1].split("/")
                dossier = [d for d in dossier if d.lower() not in ("meshes", "mesh") and not d.lower().startswith("tier")]
                cle = dossier[-1] if dossier else str(a.asset_name)
                skins.setdefault(cle, []).append(a)
            liste_skins = []
            for cle in sorted(skins):
                choix = sorted(skins[cle], key=lambda a: (not str(a.asset_name).lower().startswith(nom.lower()), len(str(a.asset_name))))
                liste_skins.append((cle, choix[0]))
            anims = [a for a in liste if _classe(a) == "AnimSequence" and meme_squelette(a)]
            montages = [a for a in liste if _classe(a) == "AnimMontage" and meme_squelette(a)]
            mort = sorted([a for a in anims if str(a.asset_name).lower().startswith("death")], key=lambda a: len(str(a.asset_name)))
            tir = sorted([a for a in montages if "primary" in str(a.asset_name).lower() or "attack" in str(a.asset_name).lower()],
                         key=lambda a: ("primary" not in str(a.asset_name).lower(), str(a.asset_name)))
            emotes = sorted([a for a in anims if str(a.asset_name).lower().startswith("emote")
                             and "additive" not in str(a.asset_name).lower()], key=lambda a: str(a.asset_name))
            if not emotes:
                emotes = sorted([a for a in anims if any(m in str(a.asset_name).lower() for m in ("taunt", "levelstart", "recall"))],
                                key=lambda a: str(a.asset_name))
            voix = pack + "/Audio/Cues/"
            if not _assets(ar, voix.rstrip("/"), False):
                voix = ""
            trouves.append({
                "nom": nom, "bp": _objet(bp) + "_C", "corps": _objet(base[0]),
                "skins": [(c, _objet(a)) for c, a in liste_skins],
                "tir": _objet(tir[0]) if tir else "", "mort": _objet(mort[0]) if mort else "",
                "danses": [_objet(a) for a in emotes[:DANSES_PAR_HEROS]], "voix": voix,
            })
    return trouves


def ajouter_heros_paragon():
    """Complete Wraith.txt (copie Wraith.txt.avant_skins faite une fois). Rend le nombre de lignes ajoutees."""
    try:
        trouves = chercher_heros_paragon()
    except Exception as e:
        probleme("recherche des heros Paragon impossible : %s" % e)
        return 0
    dire("%d heros Paragon trouves dans Content : %s" % (len(trouves), ", ".join(h["nom"] for h in trouves)))
    if not trouves:
        return 0
    with io.open(WRAITH_TXT, "r", encoding="latin-1") as f:
        lignes = f.read().replace("\r\n", "\n").split("\n")
    while lignes and lignes[-1] == "":
        lignes.pop()
    heros_la, bp_la, corps_la, danses_la, noms_la, skins_par_heros = set(), set(), set(), set(), set(), {}
    for l in lignes:
        t = l.strip()
        if t.lower().startswith("heros="):
            m = [x.strip() for x in t[6:].split("|")]
            heros_la.add(m[0].lower())
            if len(m) > 1:
                bp_la.add(m[1].lower())
        elif t.startswith(";") and "Skin=" in t:
            # skin retire a la main ou par ce script (materiau manquant) : on ne le remet pas
            m = [x.strip() for x in t.split("Skin=", 1)[1].split("|")]
            noms_la.add(m[0].upper())
            if len(m) > 1:
                corps_la.add(m[1].lower())
        elif t.lower().startswith("skin="):
            m = [x.strip() for x in t[5:].split("|")]
            noms_la.add(m[0].upper())
            if len(m) > 1:
                corps_la.add(m[1].lower())
            if len(m) > 5:
                skins_par_heros[m[5].lower()] = skins_par_heros.get(m[5].lower(), 0) + 1
        elif t.lower().startswith("danses=") and "|" in t:
            danses_la.add(t[7:].split("|")[0].strip().lower())
    nouveaux_heros, nouveaux_skins, nouvelles_danses = [], [], []
    for i, h in enumerate(trouves):
        nom = h["nom"]
        cle = nom.lower()
        existe = cle in heros_la or h["bp"].lower() in bp_la
        if existe:
            # heros deja la (Wraith, Dekker, Aurora, Boris) : on retrouve son nom dans Wraith.txt
            for l in lignes:
                t = l.strip()
                if t.lower().startswith("heros=") and h["bp"].lower() in t.lower():
                    nom = t[6:].split("|")[0].strip()
                    cle = nom.lower()
        else:
            nouveaux_heros.append("Heros=%s|%s|%s|%s|%s|%s|||" % (nom, h["bp"], h["tir"], h["mort"], h["voix"], nom))
            heros_la.add(cle)
        if h["danses"] and cle not in danses_la:
            nouvelles_danses.append("Danses=%s|%s" % (nom, ";".join(h["danses"])))
            danses_la.add(cle)
        # skins : seulement pour les heros nouveaux ou qui n'en ont qu'un (Boris)
        if skins_par_heros.get(cle, 0) >= 2:
            continue
        couleur = COULEURS_CARTES[i % len(COULEURS_CARTES)]
        corps = [("", h["corps"])] + h["skins"]
        pris = skins_par_heros.get(cle, 0)
        for dossier, chemin in corps:
            if pris >= SKINS_PAR_HEROS:
                break
            if chemin.lower() in corps_la:
                continue
            affiche = _majuscules(nom) + ((" " + _majuscules(dossier)) if dossier else "")
            if affiche in noms_la:
                continue
            nouveaux_skins.append("Skin=%s|%s||1|%s|%s" % (affiche, chemin, couleur, nom))
            corps_la.add(chemin.lower())
            noms_la.add(affiche)
            pris += 1
    total = len(nouveaux_heros) + len(nouveaux_skins) + len(nouvelles_danses)
    if not total:
        dire("Wraith.txt a deja tous les heros Paragon.")
        return 0

    def dernier(prefixe, defaut):
        pos = defaut
        for j, l in enumerate(lignes):
            if l.strip().lower().startswith(prefixe):
                pos = j + 1
        return pos

    sauvegarde = WRAITH_TXT + ".avant_skins"
    if not os.path.isfile(sauvegarde):
        import shutil
        shutil.copy2(WRAITH_TXT, sauvegarde)
    # skins : avant SOLDAT D'ORIGINE (il reste le dernier de la touche K)
    pos = dernier("skin=", len(lignes))
    for j, l in enumerate(lignes):
        if l.strip().lower().startswith("skin=") and l.split("|")[0].strip().lower() in ("skin=soldat d'origine",):
            pos = j
    if nouveaux_skins:
        lignes[pos:pos] = ["; heros Paragon ajoutes par Tools\\Skins\\REPARER_SKINS.bat (25/11)"] + nouveaux_skins
    if nouvelles_danses:
        pos = dernier("danses=", dernier("dansearretseulement=", len(lignes)))
        lignes[pos:pos] = ["; emotes des heros Paragon (touche J et apercu du menu)"] + nouvelles_danses
    if nouveaux_heros:
        pos = dernier("heros=", len(lignes))
        lignes[pos:pos] = ["; heros Paragon ajoutes par Tools\\Skins\\REPARER_SKINS.bat (25/11)"] + nouveaux_heros
    with io.open(WRAITH_TXT, "w", encoding="latin-1", newline="") as f:
        f.write("\r\n".join(_ascii_ligne(l) for l in lignes) + "\r\n")
    repare("Wraith.txt : %d heros, %d skins et %d listes de danses Paragon ajoutes" % (len(nouveaux_heros), len(nouveaux_skins), len(nouvelles_danses)))
    for l in nouveaux_heros + nouveaux_skins:
        dire("  + " + l.split("|")[0])
    return total


def _ascii_ligne(l):
    try:
        l.encode("latin-1")
        return l
    except Exception:
        return _ascii(l)


def retirer_skin(nom):
    """Met en commentaire la ligne Skin=NOM|... de Wraith.txt (25/11). LENA, SOFIA, LUCIA ne sont jamais retirees."""
    if nom.upper() in ("LENA", "SOFIA", "LUCIA"):
        return False
    try:
        with io.open(WRAITH_TXT, "r", encoding="latin-1") as f:
            lignes = f.read().replace("\r\n", "\n").split("\n")
        fait = False
        for j, l in enumerate(lignes):
            if l.strip().lower().startswith("skin=") and l.strip()[5:].split("|")[0].strip().upper() == nom.upper():
                lignes[j] = "; RETIRE (materiau manquant, perso blanc par endroits) " + l.strip()
                fait = True
        if not fait:
            return False
        sauvegarde = WRAITH_TXT + ".avant_skins"
        if not os.path.isfile(sauvegarde):
            import shutil
            shutil.copy2(WRAITH_TXT, sauvegarde)
        while lignes and lignes[-1] == "":
            lignes.pop()
        with io.open(WRAITH_TXT, "w", encoding="latin-1", newline="") as f:
            f.write("\r\n".join(lignes) + "\r\n")
        return True
    except Exception as e:
        unreal.log_warning("[Skins] %s non retire : %s" % (nom, e))
        return False


def main():
    if not os.path.isfile(WRAITH_TXT):
        probleme("Wraith.txt introuvable : " + WRAITH_TXT)
        return
    ajouter_heros_paragon()
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
                elif retirer_skin(nom):
                    repare("%s : %d materiau(x) manquant(s) -> skin retire de la liste (perso blanc par endroits)" % (nom, len(manquants)))
                    continue
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
            # danses (touche J / apercu) du heros Paragon : meme squelette que le corps, sinon T-pose pendant la danse
            mauvaises = 0
            for c in sorted(set(danses.get(h["nom"].lower(), []))):
                sa = squelette_anim(c)
                if sa is None or (sa and sa != sq_nom):
                    mauvaises += 1
            if danses.get(h["nom"].lower()):
                etat.append("%d danses" % len(set(danses[h["nom"].lower()])))
                if mauvaises:
                    probleme("%s : %d danse(s) introuvable(s) ou d'un autre squelette (T-pose pendant la danse)" % (nom, mauvaises))
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
