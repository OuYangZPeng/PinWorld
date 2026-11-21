"""Isolated native import. Run fresh editor process, normal rendering backend."""
from pathlib import Path
import unreal
reference = (Path(unreal.Paths.project_dir())/'Art/TempleOfHeaven/import_hall_unreal.py').read_text(encoding='utf-8')
reference = reference.replace('Art/TempleOfHeaven/Exports', 'Art/Tiananmen/AssemblyExports')
reference = reference.replace('/Game/Environment/TempleOfHeaven/HallOfPrayer', '/Game/Environment/Tiananmen')
reference = reference.replace('HallOfPrayer', 'Tiananmen')
reference = reference.replace('== 25', '== 31').replace('HALL_IMPORT_PASS', 'TIANANMEN_IMPORT_PASS')
exec(compile(reference, str(__file__) + ':shared_import', 'exec'))
