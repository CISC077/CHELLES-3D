"""SCRIPT A LANCER DANS UNREAL 4.27 : File > Execute Python Script... > ce fichier.
(la carte Rachid doit etre ouverte ; fais Ctrl+S AVANT, la sauvegarde copie le fichier enregistre)

Corrige les avertissements du "Map Check" de Rachid, sans rien supprimer :

 1. Ombres des grands sols (Chelles_Tile_Road_*, Chelles_Trottoir_*, Chelles_Tile_Ground) :
    "Large actor receives a pre-shadow" -> on coupe seulement leur OMBRE PORTEE dynamique.
    Une route ou un trottoir est plat : son ombre ne se voit pas. Ils RECOIVENT toujours
    l'ombre des batiments, des passants, etc.
 2. Lampadaires / bancs sans modele ("NULL StaticMesh") : on remet le modele que portent
    les autres acteurs du meme nom (CH_lamp_* voisins, SM_Bench_*...).
 3. Sprites Paper2D (facades en image) etires : leur collision ne pouvait de toute facon pas
    etre creee ("collision creation will fail") -> on la coupe proprement. L'image ne change pas.
 4. BP_Hotel : 2 materiaux poses sur un modele qui n'en a qu'un -> on enleve celui en trop
    (il n'etait pas affiche).
 5. FACULTATIF (question separee, "Non" conseille) : ombre des blocs de batiments
    (Chelles_Tile_X_Y) et de la Tour2025. Avec le soleil qui bouge (cycle jour/nuit), cet
    avertissement ne coute presque rien, et sans ombre les batiments paraissent "poses".

Ne touche PAS : lumieres, GoodSky / cycle jour-nuit, reglages de Tools\\Eclairage, neige,
NavMesh_Chelles, ni les Blueprints du Content.

Avant de modifier : copie du .umap dans Tools\\MapCheck\\sauvegarde\\.
Tout est note dans Tools\\MapCheck\\journal_mapcheck.json ; annuler_mapcheck.py remet tout.
Ctrl+Z marche aussi juste apres.

Ensuite : Build > Build Paths (le NavMesh doit voir les lampadaires remis), puis Ctrl+S.
"""
import datetime
import json
import os
import re
import shutil
import traceback

import unreal

PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DOSSIER = os.path.join(PROJET, "Tools", "MapCheck")
FICHIER_JOURNAL = os.path.join(DOSSIER, "journal_mapcheck.json")
FICHIER_LOG = os.path.join(DOSSIER, "mapcheck.log")

RAYON_GRAND = 2000.0  # meme seuil que le moteur (StaticMeshComponent.cpp)
PREFIXES_SOL = ("Chelles_Tile_Road", "Chelles_Trottoir", "Chelles_Tile_Ground")
PREFIXE_BATIMENTS = "Chelles_Tile_"
MESH_TOURS = ("Tour2025",)

# Modeles prevus par Tools\chelles_complet.py, si aucun voisin n'a de modele
MODELES_SECOURS = {
    "CH_lamp": ["/Game/Quartal/Static_Meshes/SM_Street_lamp_1", "/Game/Quartal/Static_Meshes/SM_Street_lamp_2",
                "/Game/Quartal/Static_Meshes/SM_Street_lamp_3",
                "/Game/Venice_fast_building/Meshes/Props/SM_street_lamp_01"],
    "CH_bench": ["/Game/Quartal/Static_Meshes/SM_bench", "/Game/AssetsvilleTown/Meshes/StreetProps/SM_bench"],
    "SM_Bench": ["/Game/Quartal/Static_Meshes/SM_bench", "/Game/AssetsvilleTown/Meshes/StreetProps/SM_bench",
                 "/Game/Venice_fast_building/Meshes/Props/SM_bench"],
}


def journal_texte(texte):
    unreal.log("[MapCheck] " + texte)
    try:
        os.makedirs(DOSSIER, exist_ok=True)
        with open(FICHIER_LOG, "a", encoding="utf-8") as f:
            f.write(datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S ") + texte + "\n")
    except Exception:
        pass


def oui_non(titre, message):
    r = unreal.EditorDialog.show_message(titre, message, unreal.AppMsgType.YES_NO)
    return r == unreal.AppReturnType.YES


def info(titre, message):
    unreal.EditorDialog.show_message(titre, message, unreal.AppMsgType.OK)


def label(a):
    try:
        return a.get_actor_label()
    except Exception:
        return a.get_name()


def prefixe(nom):
    """CH_lamp_21 -> CH_lamp ; SM_Bench_365 -> SM_Bench ; BP_building2 -> BP_building"""
    return re.sub(r"[_ ]?\d+$", "", nom)


def marquer(obj):
    try:
        obj.modify()
    except Exception:
        pass


def rayon(comp):
    try:
        return unreal.SystemLibrary.get_component_bounds(comp)[2]
    except Exception:
        return 0.0


def echelle_monde(comp):
    for f in (lambda: comp.get_world_transform().scale3d, lambda: comp.get_world_scale()):
        try:
            return f()
        except Exception:
            pass
    r = comp.get_editor_property("relative_scale3d")
    s = comp.get_owner().get_actor_scale3d()
    return unreal.Vector(r.x * s.x, r.y * s.y, r.z * s.z)


def est_uniforme(v):
    t = 1e-4
    return abs(v.x - v.y) <= t and abs(v.y - v.z) <= t


def chemin_asset(obj):
    if obj is None:
        return None
    p = obj.get_path_name()
    return p.split(".")[0] if "." in p else p


def nom_enum(v):
    return str(v).split(".")[-1].rstrip(">").strip()


# ---------------------------------------------------------------- recherche

def chercher(acteurs):
    trouve = {"sol": [], "bat": [], "tour": [], "vides": [], "vides_bp": [], "sprites": [], "hotel": []}
    for a in acteurs:
        lbl = label(a)
        try:
            comps = a.get_components_by_class(unreal.StaticMeshComponent)
        except Exception:
            comps = []
        mesh_nuls = 0
        for c in comps:
            sm = c.get_editor_property("static_mesh")
            if sm is None:
                mesh_nuls += 1
                continue
            # 1 / 5 : ombres
            try:
                mobile = c.get_editor_property("mobility") == unreal.ComponentMobility.MOVABLE
                ombre = c.get_editor_property("cast_shadow")
                ombre_dyn = c.get_editor_property("cast_dynamic_shadow")
            except Exception:
                mobile, ombre, ombre_dyn = False, False, False
            if sm.get_name() in MESH_TOURS and ombre:
                trouve["tour"].append((a, c))
            elif mobile and ombre and ombre_dyn and rayon(c) > RAYON_GRAND:
                if lbl.startswith(PREFIXES_SOL):
                    trouve["sol"].append((a, c))
                elif lbl.startswith(PREFIXE_BATIMENTS):
                    trouve["bat"].append((a, c))
            # 4 : materiaux en trop
            try:
                surcharges = list(c.get_editor_property("override_materials"))
                n = len(sm.get_editor_property("static_materials"))
                if len(surcharges) > n:
                    trouve["hotel"].append((a, c, n))
            except Exception:
                pass
        if mesh_nuls:
            if a.get_class().get_name() == "StaticMeshActor":
                trouve["vides"].append(a)
            else:
                trouve["vides_bp"].append((a, mesh_nuls, len(comps)))
        # 3 : sprites
        if hasattr(unreal, "PaperSpriteComponent"):
            try:
                sprites = a.get_components_by_class(unreal.PaperSpriteComponent)
            except Exception:
                sprites = []
            for c in sprites:
                try:
                    if c.get_editor_property("source_sprite") is None or not c.is_collision_enabled():
                        continue
                    if not est_uniforme(echelle_monde(c)):
                        trouve["sprites"].append((a, c))
                except Exception:
                    pass
    return trouve


def modeles_par_famille(acteurs):
    """Pour chaque famille (CH_lamp, SM_Bench...), les acteurs qui ONT un modele."""
    fam = {}
    for a in acteurs:
        if a.get_class().get_name() != "StaticMeshActor":
            continue
        c = a.static_mesh_component
        sm = c.get_editor_property("static_mesh") if c else None
        if sm is not None:
            fam.setdefault(prefixe(label(a)), []).append(a)
    return fam


def modele_secours(famille):
    for p in MODELES_SECOURS.get(famille, []):
        if unreal.EditorAssetLibrary.does_asset_exist(p):
            m = unreal.EditorAssetLibrary.load_asset(p)
            if isinstance(m, unreal.StaticMesh):
                return m
    return None


def remplacant(a, familles):
    """(modele, acteur voisin ou None) pour un StaticMeshActor sans modele."""
    fam = prefixe(label(a))
    voisins = familles.get(fam, [])
    if voisins:
        p = a.get_actor_location()
        def dist2(b):
            q = b.get_actor_location()
            return (q.x - p.x) ** 2 + (q.y - p.y) ** 2 + (q.z - p.z) ** 2
        v = min(voisins, key=dist2)
        return v.static_mesh_component.get_editor_property("static_mesh"), v
    return modele_secours(fam), None


# ---------------------------------------------------------------- corrections

def vec(v):
    return [v.x, v.y, v.z]


def entree(a, c, quoi, avant, apres=None):
    return {"acteur": a.get_path_name(), "label": label(a), "composant": c.get_name() if c else None,
            "quoi": quoi, "avant": avant, "apres": apres}


def couper_ombre_dyn(liste, notes):
    for a, c in liste:
        marquer(a); marquer(c)
        c.set_editor_property("cast_dynamic_shadow", False)
        notes.append(entree(a, c, "cast_dynamic_shadow", True, False))


def couper_ombre(liste, notes):
    for a, c in liste:
        marquer(a); marquer(c)
        c.set_editor_property("cast_shadow", False)
        notes.append(entree(a, c, "cast_shadow", True, False))


def remettre_modeles(vides, familles, notes, rapport):
    for a in vides:
        mesh, voisin = remplacant(a, familles)
        lbl = label(a)
        if mesh is None:
            rapport.append("  - %s : aucun modele trouve, laisse tel quel" % lbl)
            continue
        c = a.static_mesh_component
        loc, ech = a.get_actor_location(), a.get_actor_scale3d()
        marquer(a); marquer(c)
        c.set_editor_property("static_mesh", mesh)
        notes.append(entree(a, c, "static_mesh", None, chemin_asset(mesh)))
        if lbl.startswith("CH_") and voisin is not None:
            # chelles_complet.py posait la base du modele au sol : on garde ce sol
            # et on prend la taille du voisin (meme modele, meme hauteur).
            s = voisin.get_actor_scale3d()
            zmin = mesh.get_bounding_box().min.z
            nouv = unreal.Vector(loc.x, loc.y, loc.z - zmin * s.z)
            a.set_actor_scale3d(s)
            a.set_actor_location(nouv, False, False)
            notes.append(entree(a, None, "transform", {"loc": vec(loc), "ech": vec(ech)},
                                {"loc": vec(nouv), "ech": vec(s)}))
        rapport.append("  - %s <- %s%s" % (lbl, mesh.get_name(),
                                          (" (comme %s)" % label(voisin)) if voisin else " (modele d'origine du script)"))


def couper_collision_sprites(liste, notes):
    for a, c in liste:
        avant = nom_enum(c.get_collision_enabled())
        marquer(a); marquer(c)
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        notes.append(entree(a, c, "collision", avant, "NO_COLLISION"))


def enlever_materiaux_en_trop(liste, notes):
    for a, c, n in liste:
        surcharges = list(c.get_editor_property("override_materials"))
        marquer(a); marquer(c)
        c.set_editor_property("override_materials", surcharges[:n])
        notes.append(entree(a, c, "override_materials", [chemin_asset(m) for m in surcharges],
                            [chemin_asset(m) for m in surcharges[:n]]))


# ---------------------------------------------------------------- principal

def sauvegarder_umap(monde):
    paquet = monde.get_path_name().split(".")[0]          # /Game/Maps/Rachid
    rel = paquet.replace("/Game/", "", 1) + ".umap"
    src = os.path.join(PROJET, "Content", *rel.split("/"))
    if not os.path.isfile(src):
        return None
    dest_dir = os.path.join(DOSSIER, "sauvegarde")
    os.makedirs(dest_dir, exist_ok=True)
    dest = os.path.join(dest_dir, "%s_%s.umap" % (os.path.splitext(os.path.basename(src))[0],
                                                datetime.datetime.now().strftime("%Y-%m-%d_%Hh%M")))
    shutil.copy2(src, dest)
    return dest


def main():
    monde = unreal.EditorLevelLibrary.get_editor_world()
    acteurs = unreal.EditorLevelLibrary.get_all_level_actors()
    t = chercher(acteurs)
    familles = modeles_par_famille(acteurs)

    lignes_vides = []
    for a in t["vides"]:
        mesh, voisin = remplacant(a, familles)
        lignes_vides.append("   %s -> %s" % (label(a), mesh.get_name() if mesh else "AUCUN modele trouve"))

    msg = ("Carte : %s\n\n" % monde.get_name()
           + "Corrections prevues :\n"
           + " 1. Ombre portee coupee sur %d sols plats (routes, trottoirs)\n" % len(t["sol"])
           + " 2. Modele remis sur %d lampadaires/bancs vides :\n" % len(t["vides"])
           + "\n".join(lignes_vides[:12]) + ("\n   ..." if len(lignes_vides) > 12 else "") + "\n"
           + " 3. Collision coupee sur %d sprites-facades etires\n" % len(t["sprites"])
           + " 4. Materiau en trop enleve sur %d composants (BP_Hotel)\n" % len(t["hotel"])
           + "\nRien n'est supprime. Copie du .umap avant, journal pour tout annuler.\n"
           + "As-tu fait Ctrl+S avant ? Continuer ?")
    if not oui_non("Map Check - corrections", msg):
        journal_texte("Annule par l'utilisateur.")
        return

    option_bat = False
    if t["bat"] or t["tour"]:
        option_bat = oui_non(
            "Map Check - ombre des batiments (facultatif)",
            "Couper AUSSI l'ombre de %d blocs de batiments (Chelles_Tile_X_Y) et de %d Tour2025 ?\n\n"
            "Conseil : NON.\nAvec le soleil qui bouge (cycle jour/nuit), cet avertissement ne coute "
            "presque rien, et sans ombre les batiments paraissent poses sur la route.\n"
            "Ces %d avertissements resteront dans le Map Check, c'est normal."
            % (len(t["bat"]), len(t["tour"]), len(t["bat"]) + len(t["tour"])))

    copie = sauvegarder_umap(monde)
    notes, rapport = [], []
    with unreal.ScopedEditorTransaction("Corrections Map Check Chelles"):
        couper_ombre_dyn(t["sol"], notes)
        remettre_modeles(t["vides"], familles, notes, rapport)
        couper_collision_sprites(t["sprites"], notes)
        enlever_materiaux_en_trop(t["hotel"], notes)
        if option_bat:
            couper_ombre_dyn(t["bat"], notes)
            couper_ombre(t["tour"], notes)

    ancien = []
    if os.path.isfile(FICHIER_JOURNAL):
        try:
            ancien = json.load(open(FICHIER_JOURNAL, encoding="utf-8")).get("passages", [])
        except Exception:
            ancien = []
    ancien.append({"date": datetime.datetime.now().isoformat(timespec="seconds"),
                   "carte": monde.get_path_name(), "copie_umap": copie, "modifs": notes})
    with open(FICHIER_JOURNAL, "w", encoding="utf-8") as f:
        json.dump({"passages": ancien}, f, ensure_ascii=False, indent=1)

    for l in rapport:
        journal_texte(l)
    bp = ["   %s : %d composant(s) vide(s) sur %d" % (label(a), n, tot) for a, n, tot in t["vides_bp"]]
    for l in bp:
        journal_texte("BP non modifie :" + l)
    journal_texte("%d modifications ; copie : %s" % (len(notes), copie))

    info("Map Check - termine",
         "%d modifications faites.\n\n" % len(notes)
         + ("Non modifies (Blueprints du pack Venice, a laisser) :\n" + "\n".join(bp) + "\n\n" if bp else "")
         + "Maintenant :\n 1. Build > Build Paths (attendre la fin)\n 2. Ctrl+S\n"
         " 3. Window > Developer Tools > Map Check pour verifier.\n\n"
         "Tout annuler : Tools\\MapCheck\\annuler_mapcheck.py")


try:
    main()
except Exception:
    journal_texte("ERREUR :\n" + traceback.format_exc())
    info("Map Check - erreur", "Le script s'est arrete :\n\n" + traceback.format_exc()[-1500:])
