import bpy
import json
import re
from pathlib import Path
from mathutils import Matrix

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'Exports'
OUT.mkdir(exist_ok=True)
assert not (OUT / 'manifest.json').exists(), 'Export already exists; inspect before replacing'
scene = bpy.context.scene
assert abs(scene.unit_settings.scale_length - 1.0) < 1e-6
objects = sorted([o for o in scene.objects if o.type == 'MESH'], key=lambda o: o.name)
assert len(objects) == 25, len(objects)
report = {'unit_scale': scene.unit_settings.scale_length, 'materials': {}, 'objects': []}
used = {s.material for o in objects for s in o.material_slots}
assert None not in used
for material in sorted(used, key=lambda m: m.name):
    bsdf = next((n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None) if material.use_nodes else None
    if bsdf:
        assert not any(bsdf.inputs[k].is_linked for k in ['Base Color', 'Metallic', 'Roughness']), material.name
    report['materials'][material.name] = {'asset': 'M_' + re.sub(r'[^A-Za-z0-9_]', '_', material.name), 'color': list(bsdf.inputs['Base Color'].default_value if bsdf else material.diffuse_color), 'metallic': float(bsdf.inputs['Metallic'].default_value if bsdf else material.metallic), 'roughness': float(bsdf.inputs['Roughness'].default_value if bsdf else material.roughness)}
for index, obj in enumerate(objects):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    world = obj.matrix_world.copy()
    location = world.translation.copy()
    original = [list(row) for row in world]
    obj.data = obj.data.copy()
    obj.data.transform(Matrix.Translation(-location) @ world)
    obj.parent = None
    obj.matrix_world = Matrix.Identity(4)
    name = 'SM_%02d_%s' % (index, re.sub(r'[^A-Za-z0-9_]', '_', obj.name))
    bounds = [[min(v.co[i] for v in obj.data.vertices) for i in range(3)], [max(v.co[i] for v in obj.data.vertices) for i in range(3)]]
    obj.data.calc_loop_triangles()
    slots = [s.material.name for s in obj.material_slots]
    bpy.ops.export_scene.fbx(filepath=str(OUT / (name + '.fbx')), use_selection=True, object_types={'MESH'}, axis_forward='-Y', axis_up='Z', global_scale=1.0, apply_unit_scale=False, apply_scale_options='FBX_SCALE_NONE', use_mesh_modifiers=True, mesh_smooth_type='FACE', use_triangles=True, bake_anim=False, add_leaf_bones=False, path_mode='AUTO')
    report['objects'].append({'name': name, 'source_name': obj.name, 'source_matrix': original, 'location_cm': [location.x*100, -location.y*100, location.z*100], 'slots': slots, 'triangles': len(obj.data.loop_triangles), 'bounds_local_m': bounds})
assert sum(len(o['slots']) for o in report['objects']) == 72
(OUT / 'manifest.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print('HALL_EXPORT_PASS', len(objects), len(used), sum(o['triangles'] for o in report['objects']), flush=True)
# Never save the in-memory export transforms back to the source blend.
