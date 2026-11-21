"""Export unique assembly FBXs using the verified HallOfPrayer exporter."""
from pathlib import Path
ROOT = Path(__file__).resolve().parent
reference = (ROOT.parent/'TempleOfHeaven/export_hall_unreal.py').read_text(encoding='utf-8')
reference = reference.replace("ROOT / 'Exports'", "ROOT / 'AssemblyExports'")
reference = reference.replace('len(objects) == 25', 'len(objects) == 33')
reference = reference.replace("assert sum(len(o['slots']) for o in report['objects']) == 72", "assert len(used) == 17")
reference = reference.replace('HALL_EXPORT_PASS', 'TAIHE_EXPORT_PASS')
exec(compile(reference, str(__file__) + ':shared_export', 'exec'))
