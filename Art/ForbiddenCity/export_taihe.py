import bpy
import json
import re
from pathlib import Path
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'Exports'
OUT.mkdir(exist_ok=True)
scene = bpy.context.scene
report = {'blender': bpy.app.version_string, 'unit_scale': scene.unit_settings.scale_length, 'materials': {}, 'objects': []}
def safe(name):
    return re.sub(r'[^A-Za-z0-9_]', '_', name)
for material in bpy.data.materials:
    bsdf = next((n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None) if material.use_nodes else None
    report['materials'][material.name] = {'asset': 'M_' + safe(material.name), 'color': list(bsdf.inputs['Base Color'].default_value if bsdf else material.diffuse_color), 'metallic': float(bsdf.inputs['Metallic'].default_value if bsdf else material.metallic), 'roughness': float(bsdf.inputs['Roughness'].default_value if bsdf else material.roughness)}
objects = [o for o in scene.objects if o.type in {'MESH', 'FONT', 'CURVE', 'SURFACE'}]
for index, obj in enumerate(objects):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    original = [list(row) for row in obj.matrix_world]
    bpy.ops.object.convert(target='MESH')
    obj = bpy.context.object
    # Bake rotation/scale into geometry; retain source world translation as assembly pivot.
    world = obj.matrix_world.copy()
    location = world.translation.copy()
    obj.data.transform(Matrix.Translation(-location) @ world)
    obj.parent = None
    obj.matrix_world = Matrix.Identity(4)
    name = 'SM_%02d_%s' % (index, safe(obj.name))
    bounds = [obj.data.vertices[0].co.copy(), obj.data.vertices[0].co.copy()]
    for v in obj.data.vertices:
        for axis in range(3):
            bounds[0][axis] = min(bounds[0][axis], v.co[axis])
            bounds[1][axis] = max(bounds[1][axis], v.co[axis])
    slots = [s.material.name if s.material else None for s in obj.material_slots]
    # FBX_SCALE_NONE retains meter geometry; UE scene conversion produces centimeters.
    bpy.ops.export_scene.fbx(filepath=str(OUT / (name + '.fbx')), use_selection=True, object_types={'MESH'}, axis_forward='-Y', axis_up='Z', global_scale=1.0, apply_unit_scale=False, apply_scale_options='FBX_SCALE_NONE', use_mesh_modifiers=True, mesh_smooth_type='FACE', use_triangles=True, bake_anim=False, add_leaf_bones=False, path_mode='AUTO')
    report['objects'].append({'name': name, 'source_name': obj.name, 'source_matrix': original, 'location_cm': [location.x*100, -location.y*100, location.z*100], 'slots': slots, 'vertices': len(obj.data.vertices), 'bounds_local_m': [list(v) for v in bounds]})
(OUT / 'manifest.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print('TAIHE_EXPORT_PASS', bpy.app.version_string, len(report['objects']), len(report['materials']), flush=True)
