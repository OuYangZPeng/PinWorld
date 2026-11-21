import json
from pathlib import Path
import unreal
import sys

preview_only = '-PreviewOnly' in sys.argv or '-TutorialOnly' in sys.argv

# Editor-only asset generation; runtime module has no UnrealEd dependency.
root = Path(unreal.Paths.project_dir())
assets = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.MaterialEditingLibrary
path = '/Game/UnityMigration/M_Migration'
mat = unreal.load_asset(path)
if not mat:
    mat = assets.create_asset('M_Migration', '/Game/UnityMigration', unreal.Material, unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(mat)
mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
mat.set_editor_property('two_sided', True)

def node(cls, x, y):
    return lib.create_material_expression(mat, cls, x, y)

def link(a, output, b, pin):
    assert lib.connect_material_expressions(a, output, b, pin)

tex = node(unreal.MaterialExpressionTextureSampleParameter2D, 0, 0)
tex.set_editor_property('parameter_name', 'MainTexture')
tex.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
color = node(unreal.MaterialExpressionVectorParameter, 0, 300)
color.set_editor_property('parameter_name', 'Color')
color.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
uv = node(unreal.MaterialExpressionTextureCoordinate, -800, 0)
scale = node(unreal.MaterialExpressionVectorParameter, -800, 200)
scale.set_editor_property('parameter_name', 'UVScale')
scale.set_editor_property('default_value', unreal.LinearColor(1, 1, 0, 0))
offset = node(unreal.MaterialExpressionVectorParameter, -600, 400)
offset.set_editor_property('parameter_name', 'UVOffset')
offset.set_editor_property('default_value', unreal.LinearColor(0, 0, 0, 0))
# Masks ensure float2 UV inputs rather than implicit float3 conversions.
def rg(source, x, y):
    mask = node(unreal.MaterialExpressionComponentMask, x, y)
    mask.set_editor_property('r', True)
    mask.set_editor_property('g', True)
    link(source, '', mask, '')
    return mask
muluv = node(unreal.MaterialExpressionMultiply, -400, 0)
link(uv, '', muluv, 'A')
link(rg(scale, -600, 200), '', muluv, 'B')
add = node(unreal.MaterialExpressionAdd, -200, 0)
link(muluv, '', add, 'A')
link(rg(offset, -400, 400), '', add, 'B')
link(add, '', tex, 'UVs')
mul = node(unreal.MaterialExpressionMultiply, 300, 0)
link(tex, 'RGB', mul, 'A')
link(color, '', mul, 'B')
assert lib.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR)
roughness = node(unreal.MaterialExpressionConstant, 300, 200)
roughness.set_editor_property('r', 0.8)
assert lib.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
lib.recompile_material(mat)
assert 'MainTexture' in [str(n) for n in lib.get_texture_parameter_names(mat)]
unreal.EditorAssetLibrary.save_loaded_asset(mat)
# Capture RGB retains opaque self-depth; capture alpha is inverse coverage.
# Composite only at HUD stage, without touching any world material/depth state.
composite = unreal.load_asset('/Game/UnityMigration/M_PreviewComposite')
if not composite:
    composite = assets.create_asset('M_PreviewComposite', '/Game/UnityMigration', unreal.Material, unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(composite)
composite.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
composite.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
sample = lib.create_material_expression(composite, unreal.MaterialExpressionTextureSampleParameter2D, 0, 0)
sample.set_editor_property('parameter_name', 'Preview')
sample.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
inverse = lib.create_material_expression(composite, unreal.MaterialExpressionOneMinus, 200, 200)
assert lib.connect_material_expressions(sample, 'A', inverse, '')
assert lib.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert lib.connect_material_property(inverse, '', unreal.MaterialProperty.MP_OPACITY)
lib.recompile_material(composite)
unreal.EditorAssetLibrary.save_loaded_asset(composite)
for level, folder in ([] if preview_only else [('Room', ''), ('Foxy', 'Foxy')]):
    source = root / 'Saved/UnityMigration' / folder
    manifest = json.loads((source / 'manifest.json').read_text(encoding='utf-8-sig'))
    paths = sorted({m['mainTexturePath'] for m in manifest['materials'] if m['mainTexturePath']})
    for relative in paths:
        filename = (source / relative).resolve()
        assert filename.is_relative_to(source.resolve())
        assert filename.read_bytes()[:8] == b'\x89PNG\r\n\x1a\n'
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', str(filename))
        task.set_editor_property('destination_path', '/Game/UnityMigration/' + level)
        task.set_editor_property('destination_name', 'T_' + filename.stem)
        task.set_editor_property('automated', True)
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('save', True)
        assets.import_asset_tasks([task])
        texture = unreal.load_asset('/Game/UnityMigration/' + level + '/T_' + filename.stem)
        assert isinstance(texture, unreal.Texture2D), str(filename)
    unreal.log('MIGRATION_ASSETS level={} imported={} material=MainTexture,Color,UVScale,UVOffset'.format(level, len(paths)))
unreal.EditorAssetLibrary.save_directory('/Game/UnityMigration', only_if_is_dirty=False, recursive=True)
# Dedicated empty worlds; never load or overwrite user-authored maps.
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
for map_path in ['/Game/Maps/PinWorldBoot', '/Game/Maps/PinWorldGameplay']:
    if not unreal.EditorAssetLibrary.does_asset_exist(map_path):
        assert levels.new_level(map_path, False), map_path
        assert levels.save_current_level(), map_path
tutorial_path = '/Game/Maps/PinWorldTutorial'
if not unreal.EditorAssetLibrary.does_asset_exist(tutorial_path):
    assert levels.new_level(tutorial_path, False)
else:
    assert levels.load_level(tutorial_path)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.load_class(None, '/Script/PinWorld.TutorialGameMode'))
assert levels.save_current_level()
for filename in (root / 'Content/Resource/Login').glob('*.png'):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(filename))
    task.set_editor_property('destination_path', '/Game/Resource/Login')
    task.set_editor_property('destination_name', filename.stem)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    assets.import_asset_tasks([task])
unreal.log('TUTORIAL_MAP_AND_PNG PASS')
unreal.log('MIGRATION_ASSETS PASS')
