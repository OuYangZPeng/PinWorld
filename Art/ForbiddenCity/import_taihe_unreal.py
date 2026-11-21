import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()) / 'Art/ForbiddenCity/Exports'
DATA = json.loads((ROOT / 'manifest.json').read_text(encoding='utf-8'))
BASE = '/Game/Environment/ForbiddenCity/TaiheHall'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
materials = {}
for source, info in DATA['materials'].items():
    path = BASE + '/Materials/' + info['asset']
    mat = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(info['asset'], BASE + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(mat)
    mat.set_editor_property('two_sided', True)
    color = LIB.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, 0)
    color.set_editor_property('constant', unreal.LinearColor(*info['color']))
    assert LIB.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    for key, prop, y in [('metallic', unreal.MaterialProperty.MP_METALLIC, 150), ('roughness', unreal.MaterialProperty.MP_ROUGHNESS, 300)]:
        node = LIB.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, y)
        node.set_editor_property('r', info[key])
        assert LIB.connect_material_property(node, '', prop)
    LIB.recompile_material(mat)
    assert EAL.save_loaded_asset(mat)
    materials[source] = mat

unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
meshes = {}
for obj in DATA['objects']:
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(ROOT / (obj['name'] + '.fbx')))
    task.set_editor_property('destination_path', BASE + '/Meshes')
    task.set_editor_property('destination_name', obj['name'])
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    options = unreal.FbxImportUI()
    options.set_editor_property('import_mesh', True)
    options.set_editor_property('import_as_skeletal', False)
    options.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property('automated_import_should_detect_type', False)
    options.set_editor_property('import_materials', False)
    options.set_editor_property('import_textures', False)
    options.set_editor_property('import_animations', False)
    settings = options.static_mesh_import_data
    settings.set_editor_property('combine_meshes', True)
    settings.set_editor_property('import_uniform_scale', 1.0)
    settings.set_editor_property('convert_scene', True)
    settings.set_editor_property('convert_scene_unit', True)
    settings.set_editor_property('force_front_x_axis', False)
    settings.set_editor_property('auto_generate_collision', False)
    settings.set_editor_property('generate_lightmap_u_vs', False)
    settings.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS)
    task.set_editor_property('options', options)
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(BASE + '/Meshes/' + obj['name'])
    assert isinstance(mesh, unreal.StaticMesh), obj['name']
    slots = mesh.get_editor_property('static_materials')
    assert len(slots) == len(obj['slots']), (obj['name'], len(slots), obj['slots'])
    for i, source in enumerate(obj['slots']):
        mesh.set_material(i, materials[source])
    assert EAL.save_loaded_asset(mesh)
    meshes[obj['name']] = mesh

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
map_path = BASE + '/Maps/TaiheHallPreview'
# Dedicated generated map only. No game/default map or project setting is changed.
if EAL.does_asset_exist(map_path):
    assert levels.load_level(map_path)
    for existing in actors.get_all_level_actors():
        if isinstance(existing, (unreal.StaticMeshActor, unreal.Light, unreal.CameraActor, unreal.SkyAtmosphere)) or existing.get_actor_label() == 'TaiheHall_Assembly':
            actors.destroy_actor(existing)
else:
    assert levels.new_level(map_path, False)
root = actors.spawn_actor_from_class(unreal.Actor, unreal.Vector())
root.set_actor_label('TaiheHall_Assembly')
for obj in DATA['objects']:
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*obj['location_cm']))
    actor.set_actor_label(obj['source_name'])
    actor.set_folder_path('TaiheHall/Architecture')
    actor.static_mesh_component.set_static_mesh(meshes[obj['name']])
    actor.attach_to_actor(root, '', unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)

sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 6000), unreal.Rotator(-48, -35, 0))
sun.set_actor_label('TaiheHall_Sun')
sun.light_component.set_editor_property('intensity', 4.0)
sun.light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 5000))
sky.light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property('intensity', 1.0)
sky.light_component.set_editor_property('real_time_capture', True)
actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector())
camera_pos = unreal.Vector(7800, 11200, 7100)
rotation = unreal.MathLibrary.find_look_at_rotation(camera_pos, unreal.Vector(0, 100, 720))
camera = actors.spawn_actor_from_class(unreal.CameraActor, camera_pos, rotation)
camera.set_actor_label('TaiheHall_PresentationCamera')
camera.camera_component.set_editor_property('field_of_view', 50.0)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(camera_pos, rotation)
assert levels.save_current_level()
assert EAL.save_directory(BASE, only_if_is_dirty=False, recursive=True)
assert levels.load_level(map_path)
verification = []
for actor in actors.get_all_level_actors():
    if not isinstance(actor, unreal.StaticMeshActor):
        continue
    mesh = actor.static_mesh_component.static_mesh
    obj = next(o for o in DATA['objects'] if o['name'] == mesh.get_name())
    bounds = mesh.get_bounding_box()
    actual = [bounds.max.x-bounds.min.x, bounds.max.y-bounds.min.y, bounds.max.z-bounds.min.z]
    expected = [(obj['bounds_local_m'][1][i]-obj['bounds_local_m'][0][i])*100 for i in range(3)]
    assert all(abs(a-b) < max(1.0, abs(b)*0.001) for a,b in zip(actual, expected)), (obj['name'], actual, expected)
    loc = actor.get_actor_location()
    assert all(abs(a-b)<0.01 for a,b in zip([loc.x,loc.y,loc.z],obj['location_cm']))
    assigned = [str(actor.static_mesh_component.get_material(i).get_path_name()) for i in range(len(obj['slots']))]
    assert assigned == [materials[s].get_path_name() for s in obj['slots']]
    verification.append({'mesh':mesh.get_path_name(), 'size_cm':actual, 'location_cm':[loc.x,loc.y,loc.z], 'materials':assigned})
assert len(verification) == len(DATA['objects'])
(ROOT / 'ue_validation.json').write_text(json.dumps({'status':'PASS', 'map':map_path, 'mesh_count':len(meshes), 'material_count':len(materials), 'objects':verification},indent=2),encoding='utf-8')
unreal.log('TAIHE_IMPORT_PASS meshes=%d materials=%d map=%s' % (len(meshes),len(materials),map_path))
