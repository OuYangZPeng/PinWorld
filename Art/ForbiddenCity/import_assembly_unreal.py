"""Import only into isolated Taihe assembly destination; refuse overwrites."""
from pathlib import Path
import unreal
reference = (Path(unreal.Paths.project_dir())/'Art/TempleOfHeaven/import_hall_unreal.py').read_text(encoding='utf-8')
reference = reference.replace('Art/TempleOfHeaven/Exports', 'Art/ForbiddenCity/AssemblyExports')
reference = reference.replace('/Game/Environment/TempleOfHeaven/HallOfPrayer', '/Game/Environment/ForbiddenCity/TaiheHall/Assembly')
reference = reference.replace('HallOfPrayerPreview', 'TaiheHallAssemblyPreview').replace('HallOfPrayer', 'TaiheHall')
reference = reference.replace('== 25', '== 33').replace('HALL_IMPORT_PASS', 'TAIHE_IMPORT_PASS')
exec(compile(reference, str(__file__) + ':shared_import', 'exec'))
