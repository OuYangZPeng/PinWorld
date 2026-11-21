"""Rebuild Taihe assembly without changing original assets.
Run Blender --background --python this_file. Shared export/coverage code is read
from the checked-in TempleOfHeaven assembly builder, never executed wholesale.
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
OUT = PROJECT / 'Content/UnityMigrationData/TaiheHall'
OUT.mkdir(parents=True, exist_ok=True)
(OUT / 'meshes').mkdir(exist_ok=True)
source = ROOT / 'build_taihe.py'
text = source.read_text(encoding='utf-8')
ns = {'__file__': str(source), '__name__': '__assembly__'}
start = text.index('# Courtyard:')
end = text.index('# Finalize combined meshes')
exec(compile(text[:start], str(source), 'exec'), ns)
parts = {}
source_faces = 0

def emit(part, material, vertices, faces):
    vs, fs = parts.setdefault(part, {}).setdefault(material, ([], []))
    base = len(vs)
    vs.extend(vertices)
    fs.extend(tuple(base + i for i in face) for face in faces)

def capture(material, vertices, faces):
    global source_faces
    source_faces += len(faces)
    frames = inspect.stack()
    line = next(f.lineno for f in frames if f.filename == str(source) and f.function == '<module>')
    if line < 104:
        part = 'Plaza'
    elif line < 118:
        part = 'Terrace_' + str(ns['level'] + 1)
    elif line < 145:
        part = 'Balustrade_' + str(ns['level'] + 1)
    elif line < 154:
        part = 'Stairs_Front_' + ('Center' if ns['cx'] == 0 else 'West' if ns['cx'] < 0 else 'East')
    elif line < 159:
        part = 'Stairs_' + ('West' if ns['side'] < 0 else 'East')
    elif line < 171:
        part = 'Imperial_Ramp'
    elif line < 175:
        part = 'Hall_Platform'
    elif line == 175:
        part = 'Hall_WallShell'
    elif line < 184:
        part = 'Columns_' + ('South' if ns['y'] < 0 else 'North')
    elif line < 190:
        part = 'Columns_' + ('West' if ns['x'] < 0 else 'East')
    elif line < 207:
        part = 'Doors_' + ('South' if ns['y'] < 0 else 'North')
    elif line < 217:
        part = 'Windows_' + ('West' if ns['x'] < 0 else 'East')
    elif line < 227:
        part = 'Painted_Beams'
    elif line < 232:
        part = 'Dougong_Brackets'
    elif line < 241:
        part = 'Upper_Clerestory'
    elif line < 287:
        roof_frame = next(f for f in frames if f.filename == str(source) and f.function == 'roof')
        # All tile patches, ribs and eave trim remain a coherent roof layer.
        part = ('Roof_' if roof_frame.lineno < 268 else 'Ridge_Ornaments_') + roof_frame.frame.f_locals['label']
    elif line < 303:
        part = 'Plaque'
    elif line < 315:
        part = 'Incense_Burners'
    else:
        part = 'Courtyard_Vats'
    emit(part, material, vertices, faces)

ns['meshpart'] = capture
exec(compile('\n' * text[:start].count('\n') + text[start:end], str(source), 'exec'), ns)
for obj in list(bpy.context.scene.objects):
    if obj.type == 'FONT':
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        bpy.ops.object.convert(target='MESH')
        obj = bpy.context.object
        emit('Plaque', 'gold', [tuple(obj.matrix_world @ v.co) for v in obj.data.vertices], [tuple(p.vertices) for p in obj.data.polygons])
        bpy.data.objects.remove(obj, do_unlink=True)
assert len(parts) == 33, list(parts)
manifest = {'schemaVersion': 1, 'level': 'TaiheHall', 'panelEntries': [], 'objects': [], 'materials': [], 'doors': []}
for name, material in ns['M'].items():
    manifest['materials'].append({'id': name, 'color': dict(zip('rgba', material.diffuse_color))})
report = {'sourceBuilderSha256': hashlib.sha256(source.read_bytes()).hexdigest(), 'sourceBlendSha256': hashlib.sha256((ROOT/'TaiheHall.blend').read_bytes()).hexdigest(), 'sourcePrimitiveFaces': source_faces, 'parts': [], 'equivalence': 'None: no equivalence assumed for asymmetric rectangular assemblies.', 'ownership': 'Every source primitive has exactly one owner; font converted once. No added or removed surfaces.'}
# Reuse proven OBJ corner-normal exporter and source-blend coverage checks.
# Slice markers are asserted, so reference changes fail loudly rather than silently.
reference = (ROOT.parent/'TempleOfHeaven/build_assembly.py').read_text(encoding='utf-8')
a = reference.index('for name, materials in sorted(parts.items()):')
b = reference.index("scene = bpy.context.scene\nscene.world.color")
shared = reference[a:b].replace("'HallOfPrayer/'", "'TaiheHall/'").replace("ROOT/'HallOfPrayer.blend'", "ROOT/'TaiheHall.blend'")
shared = shared.replace("'CardinalStairs' if name.startswith('Stairs_') else ''", "''")
shared = shared.replace("name.startswith(('Roof_', 'Finial'))", "False")
exec(compile(shared, str(__file__) + ':shared_export', 'exec'), globals())
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.world.color = (.35,.35,.35)
scene.render.engine = 'CYCLES'
scene.cycles.samples = 16
bpy.ops.object.light_add(type='SUN')
bpy.context.object.rotation_euler = (.4,-.3,-.5)
bpy.context.object.data.energy = 3
bpy.ops.object.camera_add(location=(78,-112,71))
camera = bpy.context.object
camera.rotation_euler = (Vector((0,0,8))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type = 'ORTHO'
camera.data.ortho_scale = 145
scene.camera = camera
scene.render.resolution_x = 1200
scene.render.resolution_y = 1000
scene.render.resolution_percentage = 100
scene.render.filepath = str(ROOT/'assembly_preview.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'TaiheHall_Assembly.blend'))
bpy.ops.render.render(write_still=True)
print('TAIHE_ASSEMBLY_COMPLETE parts=' + str(len(parts)), flush=True)
