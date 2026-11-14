#!/usr/bin/env python3
"""Prepare unchanged HD resources locally. No IPA/executable is copied.

This offline tool is not a runtime dependency. The app build consumes only
the already prepared directory. Original SHA and every output are recorded.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ALLOWED = {'.png', '.jpg', '.wav', '.mp3', '.mcb', '.tbl', '.tble'}
HD103_SHA256 = '7eeb36590b3c19a28741e21671379029d9023864ceb241a5b8560eef045112d2'

def prepare(output):
    output = output.resolve()
    if not output.is_relative_to(ROOT / '.local'):
        raise ValueError('Prepared original resources must remain in this repo .local directory')
    manifest = json.loads((ROOT / 'docs/source-provenance.json').read_text())
    candidates = [file for file in manifest['files'] if file['sha256'] == HD103_SHA256]
    if len(candidates) != 1:
        raise ValueError('Expected exactly one approved HD1.0.3 original in provenance')
    original = candidates[0]
    source = Path(manifest['source_root']) / original['relative_path']
    digest = hashlib.file_digest(source.open('rb'), 'sha256').hexdigest()
    if digest != original['sha256']:
        raise ValueError('HD source IPA SHA-256 mismatch')
    output.mkdir(parents=True, exist_ok=True)
    files = []
    names = set()
    with zipfile.ZipFile(source) as archive:
        for member in archive.infolist():
            name = Path(member.filename)
            if name.suffix.lower() not in ALLOWED:
                continue
            if len(name.parts) != 3 or name.parts[:2] != ('Payload', 'HM2HD.app'):
                raise ValueError('Unexpected nested original resource')
            if name.name in names:
                raise ValueError('Duplicate resource name')
            names.add(name.name)
            data = archive.read(member)
            target = output / name.name
            if target.is_symlink():
                raise ValueError('Prepared resources must not link to another source')
            sha = hashlib.sha256(data).hexdigest()
            if target.exists() and hashlib.sha256(target.read_bytes()).hexdigest() != sha:
                raise ValueError('Existing prepared resource differs; refusing overwrite: ' + name.name)
            if not target.exists():
                target.write_bytes(data)
            record = {'name': name.name, 'bytes': len(data), 'sha256': sha}
            if name.suffix.lower() == '.png':
                if data[:8] != b'\x89PNG\r\n\x1a\n':
                    raise ValueError('Invalid PNG signature: ' + name.name)
                # CgBI may precede IHDR. Never assume dimensions at fixed offset16.
                offset = 8
                while offset + 12 <= len(data):
                    size = struct.unpack_from('>I', data, offset)[0]
                    if offset + 12 + size > len(data):
                        raise ValueError('PNG chunk exceeds file')
                    if data[offset+4:offset+8] == b'IHDR':
                        record['pixels'] = list(struct.unpack_from('>II', data, offset+8))
                        break
                    offset += size + 12
                if 'pixels' not in record:
                    raise ValueError('PNG IHDR missing')
            files.append(record)
    if {file.name for file in output.iterdir()} - names - {'manifest.json', 'PhoneMenu'}:
        raise ValueError('Unexpected prepared files; refusing to package unlisted data')
    if (output / 'PhoneMenu').exists():
        from prepare_phone_menu_assets import verify_prepared
        verify_prepared(output / 'PhoneMenu')
    record = {'profile': 'hd103', 'ipa_sha256': digest, 'unchanged_resource_bytes': True,
              'original_executable_included': False, 'files': sorted(files, key=lambda x: x['name'])}
    (output / 'manifest.json').write_text(json.dumps(record, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'output': str(output), 'files': len(files), 'bytes': sum(f['bytes'] for f in files)}))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=ROOT / '.local/prepared/HD')
    prepare(parser.parse_args().output)
