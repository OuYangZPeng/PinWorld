"""Reproducible assembly export; never overwrites the source blend or builder.
Run: blender --background --python build_assembly.py
Primitive ownership is captured before material batching in the original builder.
"""
import bpy
import json
import math
import inspect
import hashlib
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
PROJECT = ROOT.parents[1]
OUT = PROJECT / 'Content/UnityMigrationData/HallOfPrayer'
OUT.mkdir(parents=True, exist_ok=True)
(OUT / 'meshes').mkdir(exist_ok=True)
source = ROOT / 'build_hall_of_prayer.py'
text = source.read_text(encoding='utf-8')
# Execute only geometry definitions and construction, never source save/render code.
ns = {'__file__': str(source), '__name__': '__assembly__'}
exec(compile(text[:text.index('# Real paving gaps')], str(source), 'exec'), ns)
parts = {}
source_faces = 0

def emit(part, material, vertices, faces):
    bucket = parts.setdefault(part, {})
    vs, fs = bucket.setdefault(material, ([], []))
    base = len(vs)
    vs.extend(vertices)
    fs.extend(tuple(base + i for i in f) for f in faces)

def capture(material, vertices, faces):
    global source_faces
    source_faces += len(faces)
    group = ns['GROUP']
    line = next((f.lineno for f in inspect.stack() if f.filename == str(source) and f.function == '<module>'), 0)
    if group == 'Site':
        part = 'Plaza_Substrate' if material == 'grout' else 'Plaza_OuterPavers'
    elif group.startswith('Terrace_'):
        if line < 123:
            part = ('Base_' if line < 101 else 'Rail_') + group[-1]
        elif line < 134:
            part = 'Stairs_' + ['East', 'North', 'West', 'South'][ns['q']]
        else:
            part = 'ImperialWay_South'
    elif group.startswith('Roof_'):
        part = ('Bracket_' if line < 181 else 'Roof_') + group[-1]
    elif group == 'Circular_hall':
        if line >= 147:
            # Whole columns and complete bays get exactly one owner.
            part = 'Wall_' + str(((ns['i'] + 3) // 6) % 4 + 1)
        else:
            # Circular plinth and wall shell: partition faces, triangulate caps
            # without introducing cut surfaces or duplicating source surfaces.
            for face in faces:
                triangles = [face] if len(face) <= 4 else [(face[0], face[j], face[j+1]) for j in range(1, len(face)-1)]
                for polygon in triangles:
                    center = sum((Vector(vertices[i]) for i in polygon), Vector()) / len(polygon)
                    quarter = int(((math.atan2(center.y, center.x) + math.pi/4) % math.tau) / (math.pi/2)) + 1
                    emit('Wall_' + str(quarter), material, [vertices[i] for i in polygon], [tuple(range(len(polygon)))])
            return
    else:
        part = 'Finial' if group == 'Golden_finial' else 'Plaque'
    emit(part, material, vertices, faces)

ns['mesh'] = capture
start = text.index('# Real paving gaps')
end = text.index('# Material-batched geometry')
# Preserve source line numbers for semantic primitive provenance.
exec(compile('\n' * text[:start].count('\n') + text[start:end], str(source), 'exec'), ns)
for obj in list(bpy.context.scene.objects):
    if obj.type == 'FONT':
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        bpy.ops.object.convert(target='MESH')
        obj = bpy.context.object
        emit('Plaque', 'gold', [tuple(obj.matrix_world @ v.co) for v in obj.data.vertices], [tuple(p.vertices) for p in obj.data.polygons])
        bpy.data.objects.remove(obj, do_unlink=True)

assert len(parts) == 25, list(parts)
manifest = {'schemaVersion': 1, 'level': 'HallOfPrayer', 'panelEntries': [], 'objects': [], 'materials': [], 'doors': []}
for name, material in ns['M'].items():
    rgba = list(material.diffuse_color)
    # Warm stone distinct from the neutral target white; source material untouched.
    if name == 'marble': rgba = [.54, .48, .38, 1]
    if name == 'carving': rgba = [.68, .59, .45, 1]
    material.diffuse_color = rgba
    material.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = rgba
    manifest['materials'].append({'id': name, 'color': dict(zip('rgba', rgba))})
report = {'sourceBuilderSha256': hashlib.sha256(source.read_bytes()).hexdigest(), 'sourceBlendSha256': hashlib.sha256((ROOT/'HallOfPrayer.blend').read_bytes()).hexdigest(), 'sourcePrimitiveFaces': source_faces, 'parts': [], 'adjustment': 'Plaza substrate + outer pavers, not fabricated inner paving ring. Hall circular shell caps triangulated without added cut faces.', 'equivalence': 'Four cardinal stairs are identical under quarter-turn rotation; imperial way remains separate. Walls are not equivalent due to shell cap partition.'}
for name, materials in sorted(parts.items()):
    vertices, polygons, slots = [], [], []
    for slot, (mat, (vs, fs)) in enumerate(materials.items()):
        base = len(vertices)
        vertices.extend(vs)
        polygons.extend(tuple(base+i for i in f) for f in fs)
        slots.extend([slot]*len(fs))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], polygons)
    mesh.update()
    for mat in materials: mesh.materials.append(ns['M'][mat])
    for p, slot in zip(mesh.polygons, slots):
        p.material_index = slot
        p.use_smooth = name.startswith(('Roof_', 'Finial'))
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    mesh.calc_loop_triangles()
    # Unified position/normal indices, expected by MigrationScene parser.
    # Export in Unity-like Y-up coordinates; loader's ZXY then yields UE X,Y,Z.
    with (OUT/'meshes'/f'{name}.obj').open('w', encoding='utf-8') as stream:
        # Split polygon corners to preserve hard stone/wood edges, while retaining
        # smooth roof normals. OBJ loader requires matching position/normal indices.
        corner_indices = {}
        index = 1
        for p in mesh.polygons:
            for vi in p.vertices:
                v = mesh.vertices[vi]
                n = v.normal if p.use_smooth else p.normal
                stream.write(f'v {v.co.y:.8f} {v.co.z:.8f} {v.co.x:.8f}\n')
                stream.write(f'vn {n.y:.8f} {n.z:.8f} {n.x:.8f}\n')
                corner_indices[(p.index, vi)] = index
                index += 1
        for slot in range(len(materials)):
            stream.write(f'g submesh_{slot}\nusemtl slot_{slot}\n')
            for tri in mesh.loop_triangles:
                if tri.material_index == slot:
                    ids = [corner_indices[(tri.polygon_index, i)] for i in tri.vertices]
                    stream.write('f ' + ' '.join(f'{i}//{i}' for i in ids) + '\n')
    manifest['panelEntries'].append({'id': name, 'name': name.replace('_', ' '), 'rendererObjectIds': [name], 'memberPlaceableIds': [name], 'equivalenceGroup': 'CardinalStairs' if name.startswith('Stairs_') else ''})
    manifest['objects'].append({'id': name, 'name': name, 'hierarchyPath': 'HallOfPrayer/'+name, 'active': True, 'rendererEnabled': True, 'meshId': name, 'materialIds': list(materials), 'worldMatrix': [1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]})
    report['parts'].append({'id': name, 'faces': len(polygons), 'triangles': len(mesh.loop_triangles), 'materialSlots': list(materials)})
# Compare source-blend and assembly surface area per material, plus triangle totals.
# Source linked read-only; no source objects become assembly renderers.
def surface(objects):
    areas, triangles = {}, 0
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for ob in objects:
        if ob.type not in {'MESH', 'FONT'}: continue
        evaluated = ob.evaluated_get(depsgraph)
        data = evaluated.to_mesh()
        data.calc_loop_triangles()
        triangles += len(data.loop_triangles)
        for tri in data.loop_triangles:
            mat = data.materials[tri.material_index].name.split('.')[0]
            a,b,c = [ob.matrix_world @ data.vertices[i].co for i in tri.vertices]
            areas[mat] = areas.get(mat, 0) + (b-a).cross(c-a).length / 2
        evaluated.to_mesh_clear()
    return areas, triangles
assembly_objects = list(scene.objects) if 'scene' in globals() else list(bpy.context.scene.objects)
assembly_area, assembly_triangles = surface(assembly_objects)
with bpy.data.libraries.load(str(ROOT/'HallOfPrayer.blend'), link=False) as (available, loaded):
    loaded.objects = available.objects
source_objects = [o for o in loaded.objects if o]
for ob in source_objects: bpy.context.scene.collection.objects.link(ob)
bpy.context.view_layer.update()
original_area, original_triangles = surface(source_objects)
errors = {key: abs(original_area[key]-assembly_area.get(key, 0))/max(1, original_area[key]) for key in original_area}
assert max(errors.values()) < 1e-4, errors
assert original_triangles == assembly_triangles, (original_triangles, assembly_triangles)
for ob in source_objects: bpy.data.objects.remove(ob, do_unlink=True)
report['validation'] = {'sourceTriangles': original_triangles, 'assemblyTriangles': assembly_triangles, 'relativeSurfaceErrorByMaterial': errors, 'uniqueRendererOwners': len({e['rendererObjectIds'][0] for e in manifest['panelEntries']}), 'passed': True}
(OUT/'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
(ROOT/'assembly_report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
scene = bpy.context.scene
scene.world.color = (.35,.35,.35)
scene.render.engine = 'CYCLES'
scene.cycles.samples = 16
bpy.ops.object.light_add(type='SUN')
bpy.context.object.rotation_euler = (.4,-.3,-.5)
bpy.context.object.data.energy = 3
bpy.ops.object.camera_add(location=(70,-110,70))
camera = bpy.context.object
camera.rotation_euler = (Vector((0,0,12))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type = 'ORTHO'
camera.data.ortho_scale = 130
scene.camera = camera
scene.render.resolution_x = 1200
scene.render.resolution_y = 1000
scene.render.resolution_percentage = 100
scene.render.filepath = str(ROOT/'assembly_preview.png')
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type == 'VIEW_3D':
            area.spaces.active.region_3d.view_perspective = 'CAMERA'
            area.spaces.active.shading.color_type = 'MATERIAL'
            area.spaces.active.clip_end = 1000
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'HallOfPrayer_Assembly.blend'))
bpy.ops.render.render(write_still=True)
print('HALL_ASSEMBLY_COMPLETE parts=25 renderers=25', flush=True)
