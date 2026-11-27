# -*- coding: utf-8 -*-
"""PLUIE DE CHELLES 3D - lance par INSTALLER_PLUIE.bat (Unreal s'ouvre puis se ferme tout seul).

Cree UNE FOIS, dans le dossier /Game/Chelles/Pluie :
  - T_Pluie_Goutte   : petite image d'une goutte (trait qui s'efface en haut) ;
  - T_Pluie_Flaques  : 4 formes de flaques (image 2 x 2) ;
  - T_Pluie_Vagues   : petites vagues pour faire trembler l'eau des flaques ;
  - M_Pluie_Gouttes  : materiau des gouttes (le plugin ChellesPluie les fait tomber) ;
  - M_Pluie_Flaque   : decal de flaque qui brille (reflets), pose par le plugin sur les routes MI_AsphaltRoad ;
  - M_Pluie_SolMouille : decal "sol mouille" (plus sombre et brillant) autour du joueur ;
  - S_Pluie_Boucle   : bruit de pluie (fabrique par ce script, en boucle).

Ne touche NI a la carte Rachid NI a MI_AsphaltRoad. Peut etre relance (remplace ses propres fichiers).
Journal : Tools/Pluie/installer_pluie.log
"""
import math
import os
import random
import struct
import traceback
import wave
import zlib

import unreal

DOSSIER = "/Game/Chelles/Pluie"
PROJET = unreal.SystemLibrary.get_project_directory()
ICI = os.path.join(PROJET, "Tools", "Pluie")
SORTIE = os.path.join(ICI, "fichiers")
JOURNAL = os.path.join(ICI, "installer_pluie.log")
os.makedirs(SORTIE, exist_ok=True)
open(JOURNAL, "w").write("")

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
outils = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log("[Pluie] " + str(msg))
    with open(JOURNAL, "a", encoding="utf-8") as f:
        f.write(str(msg) + "\n")


# ------------------------------------------------------------------ images (PNG ecrit a la main, sans module en plus)

def ecrire_png(chemin, largeur, hauteur, pixel):
    """pixel(x, y) -> (r, g, b, a) de 0 a 255."""
    lignes = bytearray()
    for y in range(hauteur):
        lignes.append(0)
        for x in range(largeur):
            r, g, b, a = pixel(x, y)
            lignes += bytes((max(0, min(255, int(r))), max(0, min(255, int(g))),
                             max(0, min(255, int(b))), max(0, min(255, int(a)))))

    def bloc(type_, donnees):
        c = struct.pack(">I", len(donnees)) + type_ + donnees
        return c + struct.pack(">I", zlib.crc32(type_ + donnees) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += bloc(b"IHDR", struct.pack(">IIBBBBB", largeur, hauteur, 8, 6, 0, 0, 0))
    png += bloc(b"IDAT", zlib.compress(bytes(lignes), 9))
    png += bloc(b"IEND", b"")
    with open(chemin, "wb") as f:
        f.write(png)


def image_goutte(chemin):
    # 16 x 128 : un trait blanc, plus fort en bas (la tete de la goutte), qui s'efface en haut et sur les cotes
    def p(x, y):
        u = (x + 0.5) / 16.0
        v = (y + 0.5) / 128.0
        travers = max(0.0, 1.0 - abs(u * 2.0 - 1.0) ** 1.5)
        long_ = (v ** 1.6) * min(1.0, (1.0 - v) * 12.0)
        a = 255.0 * travers * long_
        return (255, 255, 255, a)
    ecrire_png(chemin, 16, 128, p)


def image_flaques(chemin):
    # 512 x 512 = 4 flaques de 256 x 256 ; valeur = profondeur (1 au centre, 0 au bord), formes irregulieres
    rnd = random.Random(77500)
    cases = []
    for _ in range(4):
        taches = []
        for _ in range(rnd.randint(3, 6)):
            taches.append((rnd.uniform(0.3, 0.7), rnd.uniform(0.3, 0.7), rnd.uniform(0.12, 0.25)))
        ondes = [(rnd.uniform(4, 11), rnd.uniform(0, 6.28), rnd.uniform(4, 11), rnd.uniform(0, 6.28)) for _ in range(3)]
        cases.append((taches, ondes))
    taille = 256

    def p(x, y):
        cx, cy = x // taille, y // taille
        u = (x % taille + 0.5) / taille
        v = (y % taille + 0.5) / taille
        taches, ondes = cases[cy * 2 + cx]
        champ = 0.0
        for (tx, ty, r) in taches:
            d2 = ((u - tx) ** 2 + (v - ty) ** 2) / (r * r)
            champ += math.exp(-d2)
        bruit = 0.0
        for (fx, px, fy, py) in ondes:
            bruit += math.sin(u * fx + px) * math.sin(v * fy + py)
        champ = champ * 0.75 + bruit * 0.06
        bord = min(u, v, 1.0 - u, 1.0 - v)
        champ *= min(1.0, bord * 10.0)  # toujours 0 au bord de la case
        val = max(0.0, min(1.0, (champ - 0.15) / 0.85))
        g = 255.0 * val
        return (g, g, g, 255)
    ecrire_png(chemin, 512, 512, p)


def image_vagues(chemin):
    # 128 x 128 qui se repete sans raccord : carte de normales (petites rides de pluie)
    n = 128
    rnd = random.Random(9)
    vagues = [(rnd.randint(1, 6), rnd.randint(1, 6), rnd.uniform(0, 6.28), rnd.uniform(0.4, 1.0)) for _ in range(8)]

    def hauteur(x, y):
        h = 0.0
        for (kx, ky, ph, a) in vagues:
            h += a * math.sin(2 * math.pi * (kx * x + ky * y) / n + ph) / (kx + ky)
        return h

    hs = [[hauteur(x, y) for x in range(n)] for y in range(n)]

    def p(x, y):
        dx = hs[y][(x + 1) % n] - hs[y][(x - 1) % n]
        dy = hs[(y + 1) % n][x] - hs[(y - 1) % n][x]
        nx, ny, nz = -dx * 3.0, -dy * 3.0, 1.0
        l = math.sqrt(nx * nx + ny * ny + nz * nz)
        return ((nx / l * 0.5 + 0.5) * 255, (ny / l * 0.5 + 0.5) * 255, (nz / l * 0.5 + 0.5) * 255, 255)
    ecrire_png(chemin, n, n, p)


# ------------------------------------------------------------------ son de pluie en boucle (WAV stereo 44,1 kHz)

def son_pluie(chemin, secondes=16.0, frequence=44100):
    rnd = random.Random(2611)
    total = int(secondes * frequence)
    fondu = int(1.5 * frequence)
    n = total + fondu
    canaux = []
    for c in range(2):
        out = [0.0] * n
        b0 = b1 = b2 = 0.0      # bruit rose (filtre de Paul Kellet, version courte)
        bas = 0.0               # coupe les graves
        lisse = 0.0             # adoucit les aigus
        mod = 0.0
        for i in range(n):
            w = rnd.uniform(-1.0, 1.0)
            b0 = 0.99765 * b0 + w * 0.0990460
            b1 = 0.96300 * b1 + w * 0.2965164
            b2 = 0.57000 * b2 + w * 1.0526913
            rose = (b0 + b1 + b2 + w * 0.1848) * 0.18
            lisse += (rose - lisse) * 0.55
            bas += (lisse - bas) * 0.004
            s = lisse - bas
            if i % 2205 == 0:
                mod = 0.85 + 0.15 * rnd.random()
            out[i] = s * mod
        # gouttes qui claquent (petits "tic" qui s'eteignent vite)
        for _ in range(int(n / frequence * 260)):
            debut = rnd.randrange(n)
            force = rnd.uniform(0.04, 0.22)
            dur = rnd.randint(60, 380)
            ton = rnd.uniform(0.25, 0.9)
            phase = 0.0
            for k in range(dur):
                j = debut + k
                if j >= n:
                    break
                phase += ton
                out[j] += force * math.sin(phase) * math.exp(-6.0 * k / dur)
        canaux.append(out)
    # boucle sans "clic" : la fin se fond dans le debut
    for out in canaux:
        for k in range(fondu):
            a = k / float(fondu)
            out[k] = out[k] * a + out[total + k] * (1.0 - a)
        del out[total:]
    crete = max(max(abs(v) for v in out) for out in canaux) or 1.0
    gain = 0.7 / crete
    donnees = bytearray()
    for i in range(total):
        for out in canaux:
            donnees += struct.pack("<h", int(max(-1.0, min(1.0, out[i] * gain)) * 32767))
    with wave.open(chemin, "wb") as f:
        f.setnchannels(2)
        f.setsampwidth(2)
        f.setframerate(frequence)
        f.writeframes(bytes(donnees))


# ------------------------------------------------------------------ import

def importer(fichier, nom):
    tache = unreal.AssetImportTask()
    tache.set_editor_property("filename", fichier)
    tache.set_editor_property("destination_path", DOSSIER)
    tache.set_editor_property("destination_name", nom)
    tache.set_editor_property("automated", True)
    tache.set_editor_property("replace_existing", True)
    tache.set_editor_property("save", False)
    outils.import_asset_tasks([tache])
    a = eal.load_asset("%s/%s" % (DOSSIER, nom))
    if a is None:
        raise RuntimeError("import rate : " + fichier)
    return a


def reglage(objet, prop, valeur):
    try:
        objet.set_editor_property(prop, valeur)
        return True
    except Exception as e:
        log("  (reglage %s ignore : %s)" % (prop, e))
        return False


# ------------------------------------------------------------------ materiaux

def nouveau_materiau(nom):
    chemin = "%s/%s" % (DOSSIER, nom)
    if eal.does_asset_exist(chemin):
        m = eal.load_asset(chemin)
        mel.delete_all_material_expressions(m)
        return m
    return outils.create_asset(nom, DOSSIER, unreal.Material, unreal.MaterialFactoryNew())


class Graphe(object):
    def __init__(self, m):
        self.m = m
        self.y = 0

    def e(self, classe, x=-400, y=None, **props):
        if y is None:
            self.y += 110
            y = self.y
        n = mel.create_material_expression(self.m, classe, x, y)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def lier(self, a, b, entree="", sortie=""):
        if not mel.connect_material_expressions(a, sortie, b, entree):
            raise RuntimeError("liaison ratee %s -> %s.%s" % (a.get_name(), b.get_name(), entree))

    def sortie(self, a, prop, sortie=""):
        if not mel.connect_material_property(a, sortie, prop):
            raise RuntimeError("sortie ratee vers %s" % prop)

    def scalaire(self, nom, val):
        return self.e(unreal.MaterialExpressionScalarParameter, -900, parameter_name=nom, default_value=float(val), group="Pluie")

    def couleur(self, nom, rgb):
        return self.e(unreal.MaterialExpressionVectorParameter, -900, parameter_name=nom,
                      default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0), group="Pluie")

    def mul(self, a, b=None, const_b=None, sa="", sb=""):
        n = self.e(unreal.MaterialExpressionMultiply)
        self.lier(a, n, "A", sa)
        if b is not None:
            self.lier(b, n, "B", sb)
        else:
            n.set_editor_property("const_b", float(const_b))
        return n

    def sub(self, a, b=None, const_b=None, sa="", sb=""):
        n = self.e(unreal.MaterialExpressionSubtract)
        self.lier(a, n, "A", sa)
        if b is not None:
            self.lier(b, n, "B", sb)
        else:
            n.set_editor_property("const_b", float(const_b))
        return n

    def div(self, a, b=None, const_b=None):
        n = self.e(unreal.MaterialExpressionDivide)
        self.lier(a, n, "A")
        if b is not None:
            self.lier(b, n, "B")
        else:
            n.set_editor_property("const_b", float(const_b))
        return n

    def sat(self, a):
        n = self.e(unreal.MaterialExpressionSaturate)
        self.lier(a, n, "")
        return n

    def un_moins(self, a):
        n = self.e(unreal.MaterialExpressionOneMinus)
        self.lier(a, n, "")
        return n

    def texture(self, tex, type_, uv=None):
        n = self.e(unreal.MaterialExpressionTextureSample, -700, texture=tex, sampler_type=type_)
        if uv is not None:
            self.lier(uv, n, "UVs")
        return n


def materiau_gouttes(tex):
    m = nouveau_materiau("M_Pluie_Gouttes")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    reglage(m, "used_with_static_lighting", False)
    g = Graphe(m)
    t = g.texture(tex, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    coul = g.couleur("Couleur", (0.75, 0.8, 0.9))
    lum = g.scalaire("Luminosite", 0.6)
    op = g.scalaire("Opacite", 0.35)
    emis = g.mul(g.mul(coul, lum), t, sb="R")
    # pas de goutte collee a l'oeil (moins de 0,6 m) et fondu au loin (9 a 14 m)
    prof = g.e(unreal.MaterialExpressionPixelDepth)
    pres = g.sat(g.div(g.sub(prof, const_b=60.0), const_b=200.0))
    loin = g.un_moins(g.sat(g.div(g.sub(prof, const_b=900.0), const_b=500.0)))
    opac = g.mul(g.mul(g.mul(op, t, sb="A"), pres), loin)
    g.sortie(emis, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.sortie(opac, unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def materiau_flaque(tex_flaques, tex_vagues):
    m = nouveau_materiau("M_Pluie_Flaque")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)  # obligatoire pour un decal
    m.set_editor_property("decal_blend_mode", unreal.DecalBlendMode.DBM_TRANSLUCENT)
    g = Graphe(m)
    # quelle flaque des 4 (VariationU / VariationV = 0 ou 0.5)
    uv = g.e(unreal.MaterialExpressionTextureCoordinate)
    vu = g.scalaire("VariationU", 0.0)
    vv = g.scalaire("VariationV", 0.0)
    decal = g.e(unreal.MaterialExpressionAppendVector)
    g.lier(vu, decal, "A")
    g.lier(vv, decal, "B")
    uv_case = g.e(unreal.MaterialExpressionAdd)
    g.lier(g.mul(uv, const_b=0.5), uv_case, "A")
    g.lier(decal, uv_case, "B")
    forme = g.texture(tex_flaques, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, uv_case)
    hum = g.scalaire("Humidite", 1.0)
    # la flaque grandit avec l'humidite : d'abord le creux (centre), puis les bords
    remplie = g.sat(g.mul(g.sub(forme, g.un_moins(hum), sa="R"), const_b=4.0))
    opac = g.mul(remplie, g.scalaire("Opacite", 0.95))
    g.sortie(opac, unreal.MaterialProperty.MP_OPACITY)
    g.sortie(g.couleur("Couleur", (0.018, 0.02, 0.024)), unreal.MaterialProperty.MP_BASE_COLOR)
    g.sortie(g.scalaire("Rugosite", 0.03), unreal.MaterialProperty.MP_ROUGHNESS)
    g.sortie(g.scalaire("Speculaire", 0.9), unreal.MaterialProperty.MP_SPECULAR)
    g.sortie(g.scalaire("Metal", 0.0), unreal.MaterialProperty.MP_METALLIC)
    # petites rides qui bougent (deux couches qui glissent)
    try:
        uv2 = g.mul(uv, const_b=3.0)
        p1 = g.e(unreal.MaterialExpressionPanner, speed_x=0.05, speed_y=0.02)
        g.lier(uv2, p1, "Coordinate")
        p2 = g.e(unreal.MaterialExpressionPanner, speed_x=-0.03, speed_y=0.045)
        g.lier(g.mul(uv, const_b=2.1), p2, "Coordinate")
        n1 = g.texture(tex_vagues, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, p1)
        n2 = g.texture(tex_vagues, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, p2)
        somme = g.e(unreal.MaterialExpressionAdd)
        g.lier(n1, somme, "A", "RGB")
        g.lier(n2, somme, "B", "RGB")
        plat = g.e(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
        force = g.scalaire("Rides", 0.35)
        lerp = g.e(unreal.MaterialExpressionLinearInterpolate)
        g.lier(plat, lerp, "A")
        g.lier(somme, lerp, "B")
        g.lier(force, lerp, "Alpha")
        g.sortie(lerp, unreal.MaterialProperty.MP_NORMAL)
    except Exception as e:
        log("  rides des flaques ignorees : %s" % e)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def materiau_sol_mouille():
    m = nouveau_materiau("M_Pluie_SolMouille")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)  # obligatoire pour un decal
    m.set_editor_property("decal_blend_mode", unreal.DecalBlendMode.DBM_STAIN)
    g = Graphe(m)
    # plus sombre (couleur multipliee) et plus brillant, qui s'efface doucement au loin
    pos = g.e(unreal.MaterialExpressionWorldPosition)
    cam = g.e(unreal.MaterialExpressionCameraPositionWS)
    dist = g.e(unreal.MaterialExpressionDistance)
    g.lier(pos, dist, "A")
    g.lier(cam, dist, "B")
    rayon = g.scalaire("Rayon", 7000.0)
    fondu = g.sat(g.div(g.sub(rayon, dist), g.mul(rayon, const_b=0.35)))
    hum = g.scalaire("Humidite", 1.0)
    opac = g.mul(g.mul(hum, fondu), g.scalaire("Force", 0.8))
    g.sortie(opac, unreal.MaterialProperty.MP_OPACITY)
    g.sortie(g.couleur("Teinte", (0.68, 0.7, 0.73)), unreal.MaterialProperty.MP_BASE_COLOR)
    g.sortie(g.scalaire("Rugosite", 0.2), unreal.MaterialProperty.MP_ROUGHNESS)
    g.sortie(g.scalaire("Speculaire", 0.6), unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


# ------------------------------------------------------------------ principal

def principal():
    log("Creation de la pluie dans %s (la carte et MI_AsphaltRoad ne sont pas touches)." % DOSSIER)
    if not eal.does_directory_exist(DOSSIER):
        eal.make_directory(DOSSIER)

    f_goutte = os.path.join(SORTIE, "T_Pluie_Goutte.png")
    f_flaques = os.path.join(SORTIE, "T_Pluie_Flaques.png")
    f_vagues = os.path.join(SORTIE, "T_Pluie_Vagues.png")
    f_son = os.path.join(SORTIE, "S_Pluie_Boucle.wav")
    image_goutte(f_goutte)
    image_flaques(f_flaques)
    image_vagues(f_vagues)
    log("1/4 images fabriquees")

    t_goutte = importer(f_goutte, "T_Pluie_Goutte")
    t_flaques = importer(f_flaques, "T_Pluie_Flaques")
    reglage(t_flaques, "srgb", False)
    t_vagues = importer(f_vagues, "T_Pluie_Vagues")
    reglage(t_vagues, "srgb", False)
    reglage(t_vagues, "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    for t in (t_goutte, t_flaques, t_vagues):
        eal.save_loaded_asset(t)
    log("2/4 images importees")

    materiau_gouttes(t_goutte)
    log("   M_Pluie_Gouttes ok")
    materiau_flaque(t_flaques, t_vagues)
    log("   M_Pluie_Flaque ok")
    materiau_sol_mouille()
    log("3/4 materiaux crees (M_Pluie_Gouttes, M_Pluie_Flaque, M_Pluie_SolMouille)")

    son_pluie(f_son)
    s = importer(f_son, "S_Pluie_Boucle")
    reglage(s, "looping", True)
    eal.save_loaded_asset(s)
    log("4/4 bruit de pluie cree (S_Pluie_Boucle, en boucle)")
    log("TERMINE. Ouvrez le projet, Play, puis console (touche ²) : Chelles.Pluie Oui pour voir la pluie tout de suite.")


try:
    principal()
except Exception:
    log("ERREUR :\n" + traceback.format_exc())
    log("Envoyez ce journal a Claude.")

if os.environ.get("CHELLES_QUITTER") == "1":
    try:
        unreal.SystemLibrary.quit_editor()
    except Exception:
        pass
