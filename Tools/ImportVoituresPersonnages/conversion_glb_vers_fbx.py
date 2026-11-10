"""Conversion GLB/FBX -> FBX propre pour Unreal 4.27 (lance avec bpy, dans le cloud).
usage: python conv.py <kind car|statue|skel> <src> <outdir> <nom> <taille_m> [anims_a_garder,...]
"""
import bpy, sys, os, math, re
import numpy as np
from mathutils import Matrix, Vector

kind, src, outdir, nom, taille = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], float(sys.argv[5])
garder = [a for a in (sys.argv[6].split(',') if len(sys.argv) > 6 else []) if a]
os.makedirs(outdir, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
if src.lower().endswith('.fbx'):
    bpy.ops.import_scene.fbx(filepath=src)
else:
    bpy.ops.import_scene.gltf(filepath=src)
sc = bpy.context.scene

# --- objets parasites : formes d'os (Icosphere), faisceaux lumineux, arme d'un autre heros
for o in list(sc.objects):
    n = o.name.lower()
    mats = " ".join((s.material.name.lower() if s.material else "") for s in getattr(o, "material_slots", []))
    if o.type == 'MESH' and (n.startswith('icosphere') or 'glow' in n or 'glow' in mats or 'kingpin' in n or 'kingpin' in mats):
        bpy.data.objects.remove(o, do_unlink=True)
for o in list(bpy.data.objects):
    if o.name.lower().startswith('icosphere'):
        bpy.data.objects.remove(o, do_unlink=True)

# --- textures introuvables (chemin d'un autre PC) : on les cherche dans ../textures
texdir = os.path.join(os.path.dirname(os.path.dirname(src)), 'textures')
if os.path.isdir(texdir):
    dispo = os.listdir(texdir)
    for img in list(bpy.data.images):
        if img.packed_file or not img.filepath or os.path.exists(bpy.path.abspath(img.filepath)):
            continue
        base = os.path.basename(img.filepath.replace('\\', '/'))
        cand = [f for f in dispo if f.startswith(base)]
        if cand:
            img.filepath = os.path.join(texdir, cand[0]); img.reload()
            print('texture retrouvee', cand[0])
            pref = base.rsplit('_', 1)[0]
            normal = [f for f in dispo if f.startswith(pref + '_N.')]
            for mat in bpy.data.materials:
                if not mat.node_tree: continue
                nt = mat.node_tree
                if any(n.type == 'TEX_IMAGE' and n.image == img for n in nt.nodes):
                    bsdf = next((n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED'), None)
                    if bsdf and normal and not bsdf.inputs['Normal'].is_linked:
                        t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(os.path.join(texdir, normal[0]))
                        t.image.colorspace_settings.name = 'Non-Color'
                        nm = nt.nodes.new('ShaderNodeNormalMap')
                        nt.links.new(t.outputs['Color'], nm.inputs['Color']); nt.links.new(nm.outputs['Normal'], bsdf.inputs['Normal'])
                        print('normal map ajoutee', normal[0])

meshes = [o for o in sc.objects if o.type == 'MESH']
arm = next((o for o in sc.objects if o.type == 'ARMATURE'), None)


def bbox(objs):
    dg = bpy.context.evaluated_depsgraph_get()
    pts = []
    for o in objs:
        oe = o.evaluated_get(dg)
        m = oe.to_mesh()
        if len(m.vertices):
            co = np.empty(len(m.vertices) * 3); m.vertices.foreach_get('co', co)
            co = co.reshape(-1, 3)
            M = np.array(o.matrix_world)
            co = co @ M[:3, :3].T + M[:3, 3]
            pts.append(co)
        oe.to_mesh_clear()
    a = np.concatenate(pts)
    return a.min(0), a.max(0)


def select_only(objs, active):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = active


if kind in ('car', 'statue'):
    # tout figer en un seul mesh statique
    for o in meshes:
        for md in list(o.modifiers):
            if md.type == 'ARMATURE':
                o.modifiers.remove(md)
    select_only(meshes, meshes[0])
    bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
    for o in list(sc.objects):
        if o.type != 'MESH':
            bpy.data.objects.remove(o, do_unlink=True)
    select_only(meshes, meshes[0])
    bpy.ops.object.make_single_user(object=True, obdata=True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = nom
    obj.data.name = nom
    if kind == 'car':
        # glTF : l'avant regarde -Y dans Blender -> on le tourne vers +X (= +X Unreal, l'avant des voitures)
        obj.matrix_world = Matrix.Rotation(math.radians(90), 4, 'Z') @ obj.matrix_world
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        mn, mx = bbox([obj]); k = taille / (mx[0] - mn[0])
    else:
        mn, mx = bbox([obj]); k = taille / (mx[2] - mn[2])
        if len(obj.data.polygons) > 120000:
            d = obj.modifiers.new('dec', 'DECIMATE'); d.ratio = 100000 / len(obj.data.polygons)
            bpy.ops.object.modifier_apply(modifier='dec')
    obj.scale = (k, k, k)
    bpy.ops.object.transform_apply(scale=True)
    mn, mx = bbox([obj])
    obj.data.transform(Matrix.Translation(Vector((-(mn[0] + mx[0]) / 2, -(mn[1] + mx[1]) / 2, -mn[2]))))
    export_objs = [obj]
else:
    # personnage avec squelette : on garde l'armature et ses animations
    if garder:
        for act in list(bpy.data.actions):
            if not any(g in act.name.lower() for g in garder):
                bpy.data.actions.remove(act)
    for act in list(bpy.data.actions):  # noms simples : Walk, Idle...
        m = re.search(r'@([a-z0-9_\-]+)$', act.name, re.I)
        base = m.group(1) if m else act.name.split('|')[-1]
        act.name = nom.replace('SK_', '') + '_' + {'wait': 'Idle'}.get(base.lower(), base.capitalize())
    arm.name = 'Armature'  # Unreal retire ce noeud racine
    if arm.animation_data:
        arm.animation_data.action = None
    sc.frame_set(0)
    if not bpy.data.actions:
        # sans animation : la pose affichee devient la pose de repos (celle de Sketchfab est cassee)
        select_only(meshes, meshes[0])
        bpy.ops.object.make_single_user(object=True, obdata=True)
        for o in meshes:
            for md in [m for m in o.modifiers if m.type == 'ARMATURE']:
                select_only([o], o)
                bpy.ops.object.modifier_apply(modifier=md.name)
        select_only([arm], arm)
        bpy.ops.object.mode_set(mode='POSE')
        bpy.ops.pose.select_all(action='SELECT')
        bpy.ops.pose.armature_apply(selected=False)
        bpy.ops.object.mode_set(mode='OBJECT')
        for o in meshes:
            md = o.modifiers.new('Armature', 'ARMATURE'); md.object = arm
        # puis on aplatit la hierarchie (echelles 0.01 / 100 de Sketchfab) et on fige
        select_only([arm] + meshes, arm)
        bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
        for o in list(sc.objects):
            if o.type not in ('MESH', 'ARMATURE'):
                bpy.data.objects.remove(o, do_unlink=True)
        select_only([arm] + meshes, arm)
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        for o in meshes:
            o.parent = arm
            o.matrix_parent_inverse = arm.matrix_world.inverted()
    # hauteur en pose de repos
    arm.data.pose_position = 'REST'
    bpy.context.view_layer.update()
    mn, mx = bbox(meshes); k = taille / (mx[2] - mn[2])
    arm.data.pose_position = 'POSE'
    for o in sc.objects:
        if o.parent is None:
            o.matrix_world = Matrix.Scale(k, 4) @ o.matrix_world
            o.location.z -= mn[2] * k
    bpy.context.view_layer.update()
    export_objs = [arm] + meshes

# --- textures a cote du FBX (Unreal les retrouve par leur nom)
maxpx = 4096 if kind == 'car' else 2048
for img in bpy.data.images:
    if img.type != 'IMAGE' or not (img.packed_file or img.filepath):
        continue
    try:
        if img.size[0] == 0:
            continue
        if max(img.size) > maxpx:
            f = maxpx / max(img.size); img.scale(int(img.size[0] * f), int(img.size[1] * f))
        nm = 'T_%s_%d.png' % (nom.replace('SM_', '').replace('SK_', ''), len(os.listdir(outdir)))
        img.filepath_raw = os.path.join(outdir, nm)
        img.file_format = 'PNG'
        img.save()
        if img.packed_file:
            img.unpack(method='REMOVE')
        img.filepath = os.path.join(outdir, nm)
    except Exception as e:
        print('TEXTURE ignoree', img.name, e)

select_only(export_objs, export_objs[0])
bpy.ops.export_scene.fbx(
    filepath=os.path.join(outdir, nom + '.fbx'), use_selection=True,
    apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
    object_types={'MESH', 'ARMATURE'}, mesh_smooth_type='FACE', use_tspace=True,
    add_leaf_bones=False, bake_anim=(kind == 'skel'), bake_anim_use_all_actions=True,
    bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0.0,
    path_mode='STRIP', embed_textures=False)
mn, mx = bbox([o for o in export_objs if o.type == 'MESH'])
print('RESULTAT', nom, 'taille m', (mx - mn).round(2), 'min', mn.round(2), 'tris',
      sum(len(o.data.polygons) for o in export_objs if o.type == 'MESH'),
      'anims', [a.name for a in bpy.data.actions])
