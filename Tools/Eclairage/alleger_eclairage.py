"""SCRIPT A LANCER DANS UNREAL 4.27, carte Maps/Rachid ouverte : File > Execute Python Script... > ce fichier.

ALLEGE L'ECLAIRAGE, LES OMBRES ET LA NEIGE (gros gain la nuit, quand les lampadaires s'allument).

Ce que le script regarde, puis propose (il DEMANDE Oui/Non avant de changer quoi que ce soit) :
  1. Soleil (DirectionalLight du cycle jour/nuit) : ombres dynamiques jusqu'a 80 m au lieu de 200 m,
     3 cascades au lieu de 4. Le cycle jour/nuit GoodSky continue a piloter la lumiere : on ne touche
     ni a son heure, ni a sa couleur, ni a son intensite.
  2. Lampes mobiles (Movable) : elles n'ont plus d'ombre (c'est ce qui coute le plus la nuit)
     et s'eteignent doucement au-dela de 60 m (on ne les voit pas de si loin).
     Les lampes fixes (Static/Stationary) ne sont pas touchees.
  3. Brouillard volumetrique : coupe s'il est active (tres couteux, le ciel GoodSky garde ses couleurs).
  4. Neige : la couche "Lointaine" (gros flocons, 300 m) passe a 70 % de densite. La couche "Proche"
     ne change pas, les empreintes non plus. Le sol cartoon n'est pas touche.

Tout est reversible : les anciennes valeurs sont notees dans Tools/Eclairage/journal_eclairage.json
(les premieres valeurs sont gardees si on relance), et une copie de Rachid.umap est faite dans
Tools/Eclairage/sauvegarde/. remettre_eclairage.py remet tout comme avant.
Journal : Tools/out/eclairage.log
"""
import datetime, json, os, shutil, traceback
import unreal

# ----------------------------------------------------------------------------------------------- reglages
SOLEIL_DISTANCE_OMBRES = 8000.0     # cm (80 m). Valeur Unreal par defaut : 20000 (200 m)
SOLEIL_CASCADES = 3                 # Unreal par defaut : 3 ; beaucoup de packs mettent 4 ou 5
LAMPES_DISTANCE_MAX = 6000.0        # cm (60 m) : au-dela, les lampes mobiles ne sont plus calculees
LAMPES_FONDU = 1500.0               # cm : elles s'eteignent progressivement sur 15 m
NEIGE_INSTANCE = "/Game/Chelles/Neige/MI_Neige_Lointaine"
NEIGE_PARAMETRE = "Densite"
NEIGE_FACTEUR = 0.7

PROJET = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DOSSIER = os.path.join(PROJET, "Tools", "Eclairage")
JOURNAL_JSON = os.path.join(DOSSIER, "journal_eclairage.json")
FICHIER_LOG = os.path.join(PROJET, "Tools", "out", "eclairage.log")
_lignes = []


def log(msg):
    unreal.log("[Eclairage] " + msg)
    _lignes.append(msg)


def ecrire_log():
    try:
        os.makedirs(os.path.dirname(FICHIER_LOG), exist_ok=True)
        with open(FICHIER_LOG, "a", encoding="utf-8") as f:
            f.write("\n===== %s =====\n%s\n" % (datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"), "\n".join(_lignes)))
    except Exception:
        pass


def lire(obj, prop):
    try:
        return obj.get_editor_property(prop)
    except Exception:
        return None


def mobilite(comp):
    m = str(lire(comp, "mobility") or "").upper()
    return "MOVABLE" if "MOVABLE" in m else ("STATIONARY" if "STATIONARY" in m else "STATIC")


# ----------------------------------------------------------------------------------------------- inventaire
def inventaire():
    """Liste des changements proposes : (objet, propriete, ancienne valeur, nouvelle valeur, cle, texte)."""
    changements = []
    compte = {"STATIC": 0, "STATIONARY": 0, "MOVABLE": 0}
    lampes_ombre = 0
    for acteur in unreal.EditorLevelLibrary.get_all_level_actors():
        try:
            comps = acteur.get_components_by_class(unreal.LightComponent)
        except Exception:
            comps = []
        for c in comps:
            mob = mobilite(c)
            compte[mob] += 1
            cle = "%s|%s" % (acteur.get_path_name(), c.get_name())
            nom = acteur.get_actor_label()
            if isinstance(c, unreal.DirectionalLightComponent):
                if mob != "MOVABLE":
                    continue
                d = lire(c, "dynamic_shadow_distance_movable_light")
                if d is not None and d > SOLEIL_DISTANCE_OMBRES:
                    changements.append((c, "dynamic_shadow_distance_movable_light", float(d), SOLEIL_DISTANCE_OMBRES,
                                        cle, "Soleil '%s' : ombres jusqu'a %.0f m (au lieu de %.0f m)" % (nom, SOLEIL_DISTANCE_OMBRES / 100, d / 100)))
                n = lire(c, "dynamic_shadow_cascades")
                if n is not None and n > SOLEIL_CASCADES:
                    changements.append((c, "dynamic_shadow_cascades", int(n), SOLEIL_CASCADES,
                                        cle, "Soleil '%s' : %d cascades d'ombre (au lieu de %d)" % (nom, SOLEIL_CASCADES, n)))
                continue
            if mob != "MOVABLE":
                continue   # lampes fixes : leur lumiere est deja "cuite", elles ne coutent presque rien
            if lire(c, "cast_shadows"):
                lampes_ombre += 1
                changements.append((c, "cast_shadows", True, False, cle, "Lampe '%s' : sans ombre" % nom))
            md = lire(c, "max_draw_distance")
            if md is not None and (md <= 0 or md > LAMPES_DISTANCE_MAX):
                changements.append((c, "max_draw_distance", float(md), LAMPES_DISTANCE_MAX, cle,
                                    "Lampe '%s' : calculee jusqu'a %.0f m" % (nom, LAMPES_DISTANCE_MAX / 100)))
                fr = lire(c, "max_distance_fade_range")
                if fr is not None and fr < LAMPES_FONDU:
                    changements.append((c, "max_distance_fade_range", float(fr), LAMPES_FONDU, cle, None))
        try:
            brouillards = acteur.get_components_by_class(unreal.ExponentialHeightFogComponent)
        except Exception:
            brouillards = []
        for b in brouillards:
            if lire(b, "volumetric_fog"):
                changements.append((b, "volumetric_fog", True, False, "%s|%s" % (acteur.get_path_name(), b.get_name()),
                                    "Brouillard '%s' : volumetrique coupe" % acteur.get_actor_label()))
    log("Lampes dans la carte : %d fixes (Static), %d Stationary, %d mobiles (Movable) dont %d avec ombres."
        % (compte["STATIC"], compte["STATIONARY"], compte["MOVABLE"], lampes_ombre))
    return changements, compte


def changement_neige():
    mi = unreal.load_asset(NEIGE_INSTANCE) if unreal.EditorAssetLibrary.does_asset_exist(NEIGE_INSTANCE) else None
    if not mi:
        log("Neige : %s introuvable (neige pas installee ?) : rien a faire." % NEIGE_INSTANCE)
        return None
    try:
        v = unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(mi, NEIGE_PARAMETRE)
    except Exception:
        return None
    return (mi, float(v), round(float(v) * NEIGE_FACTEUR, 3))


# ----------------------------------------------------------------------------------------------- application
def charger_journal():
    try:
        with open(JOURNAL_JSON, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return {"composants": {}, "neige": None}


def sauvegarder_carte():
    monde = unreal.EditorLevelLibrary.get_editor_world()
    paquet = monde.get_outermost().get_name()            # /Game/Maps/Rachid
    rel = paquet.replace("/Game/", "", 1).replace("/", os.sep) + ".umap"
    src = os.path.join(PROJET, "Content", rel)
    if os.path.exists(src):
        dst_dir = os.path.join(DOSSIER, "sauvegarde")
        os.makedirs(dst_dir, exist_ok=True)
        dst = os.path.join(dst_dir, "%s_%s.umap" % (os.path.basename(rel)[:-5], datetime.datetime.now().strftime("%Y%m%d_%H%M%S")))
        shutil.copy2(src, dst)
        log("Copie de la carte : %s" % dst)


def appliquer(changements, neige):
    journal = charger_journal()
    faits = 0
    for obj, prop, ancien, nouveau, cle, _ in changements:
        try:
            journal["composants"].setdefault(cle, {}).setdefault(prop, ancien)
            obj.set_editor_property(prop, nouveau)
            faits += 1
        except Exception as e:
            log("  ECHEC %s %s : %s" % (cle, prop, e))
    if neige:
        mi, ancien, nouveau = neige
        try:
            unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, NEIGE_PARAMETRE, nouveau)
            unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
            if journal.get("neige") is None:
                journal["neige"] = {"instance": NEIGE_INSTANCE, "parametre": NEIGE_PARAMETRE, "valeur": ancien}
            faits += 1
        except Exception as e:
            log("  ECHEC neige : %s" % e)
    os.makedirs(DOSSIER, exist_ok=True)
    with open(JOURNAL_JSON, "w", encoding="utf-8") as f:
        json.dump(journal, f, indent=1, ensure_ascii=False)
    if unreal.EditorLevelLibrary.save_current_level():
        log("Carte enregistree.")
    else:
        log("Faites Ctrl+S pour enregistrer la carte.")
    return faits


def main():
    changements, compte = inventaire()
    neige = changement_neige()
    lignes = [t for *_, t in changements if t]
    if neige:
        lignes.append("Neige lointaine : densite %.2f -> %.2f" % (neige[1], neige[2]))
    for l in lignes[:60]:
        log("  " + l)
    if len(lignes) > 60:
        log("  ... et %d autre(s)." % (len(lignes) - 60))
    if not lignes:
        unreal.EditorDialog.show_message("Eclairage", "Rien a alleger : c'est deja fait.", unreal.AppMsgType.OK)
        return
    soleil = sum(1 for c in changements if c[1].startswith("dynamic_shadow"))
    lampes = sum(1 for c in changements if c[1] == "cast_shadows")
    q = ("Changements proposes :\n"
         "  - soleil : %s\n"
         "  - %d lampe(s) mobile(s) sans ombre, calculees jusqu'a 60 m\n"
         "  - brouillard volumetrique : %s\n"
         "  - neige lointaine : %s\n\n"
         "Le cycle jour/nuit, le sol et les empreintes de neige ne sont pas touches.\n"
         "remettre_eclairage.py remet tout comme avant.\n\nAppliquer ?"
         % ("ombres a 80 m, 3 cascades" if soleil else "deja leger",
            lampes,
            "coupe" if any(c[1] == "volumetric_fog" for c in changements) else "deja coupe",
            ("densite %.2f -> %.2f" % (neige[1], neige[2])) if neige else "rien"))
    if unreal.EditorDialog.show_message("Alleger l'eclairage", q, unreal.AppMsgType.YES_NO) != unreal.AppReturnType.YES:
        log("Annule : rien n'a ete modifie.")
        return
    sauvegarder_carte()
    faits = appliquer(changements, neige)
    log("Termine : %d reglage(s) changes." % faits)
    conseil = ""
    if compte["STATIC"] + compte["STATIONARY"] > 0:
        conseil = ("\n\nPour finir, rebatir l'eclairage fixe : menu Build > Lighting Quality > Preview,\n"
                   "puis Build > Build Lighting Only (peut prendre du temps sur une grande carte).")
    unreal.EditorDialog.show_message("Eclairage", "%d reglage(s) changes. Lance Play pour verifier, de jour et de nuit.%s"
                                     % (faits, conseil), unreal.AppMsgType.OK)


try:
    main()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())
    unreal.log_error(traceback.format_exc())
finally:
    ecrire_log()
