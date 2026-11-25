# CHELLES 3D - 25/11/2026 : LECTURE SEULE. Liste les objets de Rachid que les vehicules peuvent traverser
# (murs, batiments, sols sans collision). Resultat : Tools\out\collisions_rachid.txt. Rien n'est modifie.
import os
import unreal

PROJET = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SORTIE = os.path.join(PROJET, "Tools", "out", "collisions_rachid.txt")
unreal.EditorLevelLibrary.load_level("/Game/Maps/Rachid")
lib = unreal.EditorStaticMeshLibrary
meshes = {}       # chemin -> [nb acteurs, taille max cm, raison]
coupes = []       # acteurs dont la collision est coupee sur place
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        m = c.get_editor_property("static_mesh")
        if m is None or not c.is_visible():
            continue
        try:
            ext = c.get_editor_property("bounds").box_extent
        except Exception:
            o, ext = a.get_actor_bounds(False)
        taille = 2 * max(ext.x, ext.y, ext.z)
        if taille < 150:   # petits objets : sans importance pour une voiture
            continue
        if c.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION or \
           c.get_collision_profile_name() in ("NoCollision", "OverlapAll", "OverlapAllDynamic"):
            coupes.append("%s | %s | %s | %d cm" % (a.get_actor_label(), m.get_path_name(), c.get_collision_profile_name(), taille))
            continue
        simple = lib.get_simple_collision_count(m)
        flag = m.get_editor_property("body_setup").get_editor_property("collision_trace_flag") if m.get_editor_property("body_setup") else None
        if simple == 0 and flag != unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE:
            e = meshes.setdefault(m.get_path_name(), [0, 0, str(flag)])
            e[0] += 1
            e[1] = max(e[1], int(taille))
lignes = ["MESHES SANS COLLISION SIMPLE (les vehicules les traversent) : %d" % len(meshes),
          "chemin | nb dans Rachid | plus grande taille cm | trace"]
for k, v in sorted(meshes.items(), key=lambda kv: -kv[1][1]):
    lignes.append("%s | %d | %d | %s" % (k, v[0], v[1], v[2]))
lignes += ["", "ACTEURS AVEC COLLISION COUPEE SUR PLACE : %d" % len(coupes)] + coupes
open(SORTIE, "w", encoding="utf-8").write("\n".join(lignes))
unreal.log("[Collisions] %d meshes sans collision, %d acteurs coupes -> %s" % (len(meshes), len(coupes), SORTIE))
