"""Prepare only manifest-referenced runtime data; run after GenerateMigrationAssets.py."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def contained(root, relative):
    relative = Path(relative)
    if relative.is_absolute() or ':' in str(relative):
        raise ValueError(f'Absolute reference: {relative}')
    target = (root / relative).resolve()
    if not target.is_relative_to(root.resolve()):
        raise ValueError(f'Reference escapes root: {relative}')
    return target


def main():
    project = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=project / 'Saved/UnityMigration')
    args = parser.parse_args()
    destination = project / 'Content/UnityMigrationData'
    planned = []
    for level, folder in [('Room', ''), ('Foxy', 'Foxy')]:
        source = (args.source / folder).resolve()
        manifest = source / 'manifest.json'
        data = json.loads(manifest.read_text(encoding='utf-8-sig'))
        references = {'manifest.json'}
        references.update('meshes/' + obj['meshId'] + '.obj' for obj in data['objects'] if obj.get('meshId'))
        textures = {mat['mainTexturePath'] for mat in data['materials'] if mat.get('mainTexturePath')}
        fallback = 0
        for texture in sorted(textures):
            source_texture = contained(source, texture)
            if not source_texture.is_file():
                raise FileNotFoundError(source_texture)
            asset = project / 'Content/UnityMigration' / level / ('T_' + source_texture.stem + '.uasset')
            # Cooked texture is preferred. Only absent assets need loose fallback PNGs.
            if not asset.is_file():
                references.add(texture)
                fallback += 1
        for relative in sorted(references):
            src = contained(source, relative)
            dst = contained(destination / folder, relative)
            if not src.is_file():
                raise FileNotFoundError(src)
            planned.append((src, dst))
        print(f'DATA_PLAN level={level} entries={len(data["panelEntries"])} meshes={len(references)-1-fallback} textures={len(textures)} cooked={len(textures)-fallback} fallback={fallback}', flush=True)
    # Validate every reference before writing. No source mutation or directory copying.
    for src, dst in planned:
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        assert hashlib.sha256(src.read_bytes()).digest() == hashlib.sha256(dst.read_bytes()).digest()
    print(f'DATA_PREPARE PASS files={len(planned)} root={destination}', flush=True)


if __name__ == '__main__':
    main()
