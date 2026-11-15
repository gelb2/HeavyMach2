#!/usr/bin/env python3
"""Hash decoded pixels through the unchanged common C++ reader; no image export."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSIONS = ['phone100', 'phone110', 'phone130', 'phone201', 'hd100', 'hd103']


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=ROOT / '.local/evidence/universal-image-pixels-20261113')
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / '.local'):
        parser.error('Pixel metadata must remain in this repository .local space')
    output.mkdir(parents=True, exist_ok=True)
    binary = output / 'audit-shared-decoder'
    command = ['xcrun', 'clang++', '-std=c++20', '-O2', '-ffp-contract=off', '-fno-fast-math',
               '-I', str(ROOT / 'Sources'), '-I', str(ROOT / 'ThirdParty/stb'),
               str(ROOT / 'tools/audit_universal_image_pixels.cpp'), str(ROOT / 'Sources/Assets/Image.cpp'),
               '-o', str(binary)]
    subprocess.run(command, cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    manifest = json.loads((ROOT / 'docs/source-provenance.json').read_text())
    cache, versions, errors = {}, {}, []
    with (output / 'decoder.stderr').open('w') as stderr:
        process = subprocess.Popen([str(binary)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=stderr)
        try:
            for version, source in zip(VERSIONS, manifest['files'], strict=True):
                path = Path(manifest['source_root']) / source['relative_path']
                with path.open('rb') as stream:
                    before = hashlib.file_digest(stream, 'sha256').hexdigest()
                if before != source['sha256']:
                    raise RuntimeError('Original IPA hash mismatch: ' + version)
                rows = {}
                with zipfile.ZipFile(path) as archive:
                    info_paths = [x.filename for x in archive.infolist()
                                  if x.filename.startswith('Payload/') and x.filename.endswith('.app/Info.plist')]
                    if len(info_paths) != 1:
                        raise RuntimeError('Expected one app Info.plist')
                    prefix = str(Path(info_paths[0]).parent) + '/'
                    for member in archive.infolist():
                        if member.is_dir() or Path(member.filename).suffix.lower() not in {'.png', '.jpg', '.jpeg'}:
                            continue
                        raw = archive.read(member)
                        encoded = sha(raw)
                        if encoded not in cache:
                            process.stdin.write(struct.pack('<Q', len(raw)))
                            process.stdin.write(raw)
                            process.stdin.flush()
                            line = process.stdout.readline()
                            if not line:
                                raise RuntimeError('Shared decoder exited without a response')
                            cache[encoded] = json.loads(line)
                        logical = ('app/' + member.filename[len(prefix):] if member.filename.startswith(prefix)
                                   else 'package/' + member.filename)
                        if logical in rows:
                            raise RuntimeError('Duplicate logical image path')
                        rows[logical] = {'encoded_sha256': encoded, **cache[encoded]}
                        if not cache[encoded]['ok']:
                            errors.append({'version': version, 'path': logical})
                with path.open('rb') as stream:
                    after = hashlib.file_digest(stream, 'sha256').hexdigest()
                if after != before:
                    raise RuntimeError('Original changed during pixel census: ' + version)
                versions[version] = {'ipa_sha256': before, 'original_unchanged': True, 'images': rows}
                print(json.dumps({'version': version, 'images': len(rows), 'unique_decodes_so_far': len(cache)}), flush=True)
            process.stdin.close()
            if process.wait() != 0:
                raise RuntimeError('Shared decoder process failed')
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait()
    left, right = versions['phone201']['images'], versions['hd103']['images']
    comparison, counts = [], Counter()
    for name in sorted(left.keys() & right.keys()):
        a, b = left[name], right[name]
        if not a['ok'] or not b['ok']:
            state = 'decode_error'
        elif (a['width'], a['height']) != (b['width'], b['height']):
            state = 'different_dimensions'
        elif a['premultiplied_rgba_sha256'] == b['premultiplied_rgba_sha256']:
            state = 'same_prepared_pixels'
        else:
            state = 'different_prepared_pixels'
        counts[state] += 1
        if a['encoded_sha256'] != b['encoded_sha256']:
            comparison.append({'path': name, 'state': state,
                               'left': {k: v for k, v in a.items() if k != 'encoded_sha256'},
                               'right': {k: v for k, v in b.items() if k != 'encoded_sha256'}})
    record = {'scope': 'All PNG/JPEG members of six approved IPAs using the unchanged game decoder',
              'analysis_program': {'source': 'tools/audit_universal_image_pixels.cpp',
                                   'source_sha256': sha((ROOT / 'tools/audit_universal_image_pixels.cpp').read_bytes()),
                                   'binary_sha256': sha(binary.read_bytes()),
                                   'compiler': subprocess.check_output(['xcrun', 'clang++', '--version'], text=True).splitlines()[0],
                                   'flags': ['-std=c++20', '-O2', '-ffp-contract=off', '-fno-fast-math']},
              'decoder_sources': {name: sha((ROOT / name).read_bytes()) for name in
                                  ['Sources/Assets/Image.cpp', 'Sources/Assets/Image.hpp', 'ThirdParty/stb/stb_image.h']},
              'original_bytes_exported': False, 'unique_encoded_images_decoded': len(cache),
              'image_counts': {key: len(value['images']) for key, value in versions.items()},
              'originals_unchanged': all(v['original_unchanged'] for v in versions.values()),
              'primary_common_image_states': dict(counts), 'primary_changed_encoding_images': comparison,
              'errors': errors, 'runtime_executed': False,
              'limits': ['Equality is prepared top-row-first premultiplied RGBA8 from the common decoder, not original GPU output.',
                         'Alpha bounds cover the whole file, not an assembled unit, animation frame or world collision shape.',
                         'All versions were decoded, but the detailed cross-product pixel comparison here is phone201 versus hd103.']}
    (output / 'all-image-pixel-metadata.json').write_text(json.dumps(versions, ensure_ascii=False, indent=2) + '\n')
    destination = ROOT / 'docs/evidence/universal-image-pixels-20261113.json'
    destination.write_text(json.dumps(record, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'output': str(destination), 'states': dict(counts), 'errors': len(errors)}))
    raise SystemExit(bool(errors))


if __name__ == '__main__':
    main()
