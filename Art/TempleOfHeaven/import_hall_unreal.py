import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()) / 'Art/TempleOfHeaven/Exports'
DATA = json.loads((ROOT / 'manifest.json').read_text(encoding='utf-8'))
BASE = '/Game/Environment/TempleOfHeaven/HallOfPrayer'
MAP = BASE + '/Maps/HallOfPrayerPreview'
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
# Refuse to overwrite any pre-existing user assets, including partially completed runs.
existing = EAL.list_assets(BASE, recursive=True, include_folder=False)
assert not existing, 'Destination is not empty; inspect before importing: ' + str(existing)
assert len(DATA['objects']) == 25
materials = {}
for source, info in DATA['materials'].items():
    mat = TOOLS.create_asset(info['asset'], BASE + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    assert mat
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
    for key, value in {'filename': str(ROOT / (obj['name'] + '.fbx')), 'destination_path': BASE + '/Meshes', 'destination_name': obj['name'], 'automated': True, 'replace_existing': False, 'save': True}.items():
        task.set_editor_property(key, value)
    options = unreal.FbxImportUI()
    for key, value in {'import_mesh': True, 'import_as_skeletal': False, 'mesh_type_to_import': unreal.FBXImportType.FBXIT_STATIC_MESH, 'automated_import_should_detect_type': False, 'import_materials': False, 'import_textures': False, 'import_animations': False}.items():
        options.set_editor_property(key, value)
    settings = options.static_mesh_import_data
    for key, value in {'combine_meshes': True, 'import_uniform_scale': 1.0, 'convert_scene': True, 'convert_scene_unit': True, 'force_front_x_axis': False, 'auto_generate_collision': False, 'generate_lightmap_u_vs': False, 'normal_import_method': unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS}.items():
        settings.set_editor_property(key, value)
    task.set_editor_property('options', options)
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(BASE + '/Meshes/' + obj['name'])
    assert isinstance(mesh, unreal.StaticMesh), obj['name']
    slots = mesh.get_editor_property('static_materials')
    assert len(slots) == len(obj['slots']), (obj['name'], len(slots), obj['slots'])
    # Resolve imported slot names rather than assume FBX preserves slot ordering.
    assigned_sources = []
    for i, slot in enumerate(slots):
        source = str(slot.get_editor_property('imported_material_slot_name'))
        assert source in obj['slots'], (obj['name'], source, obj['slots'])
        mesh.set_material(i, materials[source])
        assigned_sources.append(source)
    obj['ue_slots'] = assigned_sources
    assert EAL.save_loaded_asset(mesh)
    meshes[obj['name']] = mesh

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.new_level(MAP, False)
root = actors.spawn_actor_from_class(unreal.Actor, unreal.Vector())
root.set_actor_label('HallOfPrayer_Assembly')
for obj in DATA['objects']:
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*obj['location_cm']))
    actor.set_actor_label(obj['source_name'])
    actor.set_folder_path('HallOfPrayer/Architecture')
    actor.static_mesh_component.set_static_mesh(meshes[obj['name']])
    actor.attach_to_actor(root, '', unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 6000), unreal.Rotator(-48, -35, 0))
sun.light_component.set_editor_property('intensity', 4.0)
sun.light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 5000))
sky.light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property('intensity', 1.0)
sky.light_component.set_editor_property('real_time_capture', True)
actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector())
camera_pos = unreal.Vector(8500, 11000, 7800)
rotation = unreal.MathLibrary.find_look_at_rotation(camera_pos, unreal.Vector(0, 0, 1800))
camera = actors.spawn_actor_from_class(unreal.CameraActor, camera_pos, rotation)
camera.set_actor_label('HallOfPrayer_PresentationCamera')
camera.camera_component.set_editor_property('field_of_view', 50.0)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(camera_pos, rotation)
assert levels.save_current_level()
assert EAL.save_directory(BASE, only_if_is_dirty=False, recursive=True)
assert levels.load_level(MAP)
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
    assigned = [actor.static_mesh_component.get_material(i).get_path_name() for i in range(len(obj['slots']))]
    assert assigned == [materials[s].get_path_name() for s in obj['ue_slots']]
    verification.append({'mesh': mesh.get_path_name(), 'size_cm': actual, 'location_cm': [loc.x,loc.y,loc.z], 'materials': assigned})
assert len(verification) == 25
assert len(EAL.list_assets(BASE + '/Meshes', recursive=True, include_folder=False)) == 25
assert len(EAL.list_assets(BASE + '/Materials', recursive=True, include_folder=False)) == len(materials)
(ROOT / 'ue_validation.json').write_text(json.dumps({'status': 'PASS', 'map': MAP, 'mesh_count': len(meshes), 'material_count': len(materials), 'slot_count': sum(len(o['slots']) for o in DATA['objects']), 'objects': verification}, indent=2), encoding='utf-8')
unreal.log('HALL_IMPORT_PASS meshes=%d materials=%d map=%s' % (len(meshes), len(materials), MAP))
