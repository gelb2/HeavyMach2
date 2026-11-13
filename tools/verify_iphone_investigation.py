#!/usr/bin/env python3
"""Verify retained metadata and referenced files without running or changing games."""
from pathlib import Path
import hashlib
import json
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    references = {}
    count = 0
    image_rows = []

    def walk(value):
        nonlocal count
        if isinstance(value, dict):
            if isinstance(value.get('path'), str) and isinstance(value.get('sha256'), str):
                if value['path'].startswith('app/'):
                    image_rows.append(value)
                else:
                    file = Path(value['path'])
                    file = file if file.is_absolute() else ROOT / file
                    if not file.is_file() or sha(file) != value['sha256']:
                        raise ValueError('Missing or changed referenced file: ' + str(file))
                    count += 1
                    references[str(file)] = value['sha256']
            for child in value.values():
                walk(child)
        elif isinstance(value, list):
            for child in value:
                walk(child)

    inputs = sorted((ROOT / 'docs/evidence').glob('iphone-*.json'))
    for path in inputs:
        walk(json.loads(path.read_text()))
    manifest = json.loads((ROOT / 'docs/source-provenance.json').read_text())
    images = 0
    for version, position in [('phone201', 3), ('hd103', 5)]:
        source = manifest['files'][position]
        path = Path(manifest['source_root']) / source['relative_path']
        before = sha(path)
        if before != source['sha256']:
            raise ValueError('Original IPA SHA differs from source provenance')
        with zipfile.ZipFile(path) as archive:
            candidates = [name for name in archive.namelist()
                          if name.startswith('Payload/') and name.endswith('.app/Info.plist')]
            if len(candidates) != 1:
                raise ValueError('Expected one original app prefix')
            prefix = candidates[0][:-len('Info.plist')]
            for row in image_rows:
                if row['version'] != version:
                    continue
                data = archive.read(prefix + row['path'][4:])
                if hashlib.sha256(data).hexdigest() != row['sha256']:
                    raise ValueError('Original image SHA differs: ' + row['path'])
                images += 1
        if sha(path) != before:
            raise ValueError('Original IPA changed while reading')
    result = {'metadata_json_files': len(inputs), 'file_references_checked': count,
              'unique_files_checked': len(references), 'referenced_package_images_checked': images,
              'all_hashes_match': True, 'game_or_device_executed': False, 'originals_unchanged': True}
    output = ROOT / '.local/evidence/iphone-universal-integration-20261113/evidence-verification.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
