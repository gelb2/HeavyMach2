#!/usr/bin/env python3
"""Package the pinned original HD artwork into local Apple app icon catalogs.

Original artwork and generated image bytes stay in .local, like HD resources.
The native renderer is an offline tool, never a game runtime dependency.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
IPA_SHA = '7eeb36590b3c19a28741e21671379029d9023864ceb241a5b8560eef045112d2'
ART_MEMBER = 'Payload/HM2HD.app/Main_BG.png'
ART_SHA = 'd34b109ac8ccdb73f90a419b60840d350ab7c3d53acad40194f0c8eb3b809a36'


def prepare(output):
    output = output.resolve()
    if not output.is_relative_to(ROOT / '.local'):
        raise ValueError('Generated original artwork must remain in this repo .local')
    provenance = json.loads((ROOT / 'docs/source-provenance.json').read_text())
    candidates = [f for f in provenance['files'] if f['sha256'] == IPA_SHA]
    if len(candidates) != 1:
        raise ValueError('Expected one pinned HD1.0.3 input')
    original = Path(provenance['source_root']) / candidates[0]['relative_path']
    with original.open('rb') as stream:
        if hashlib.file_digest(stream, 'sha256').hexdigest() != IPA_SHA:
            raise ValueError('Original IPA hash mismatch')
    with zipfile.ZipFile(original) as archive:
        artwork = archive.read(ART_MEMBER)
    if hashlib.sha256(artwork).hexdigest() != ART_SHA:
        raise ValueError('Original tap-to-start Main_BG.png hash mismatch')
    local = ROOT / '.local/app-icon-tools'
    local.mkdir(parents=True, exist_ok=True)
    input_file = local / 'Main_BG.png'
    input_file.write_bytes(artwork)
    renderer = local / 'render_app_icon'
    subprocess.run(['xcrun', 'clang++', '-std=c++20', '-O2', '-fobjc-arc',
                    '-I', str(ROOT / 'Sources'), '-I', str(ROOT / 'ThirdParty/stb'),
                    str(ROOT / 'tools/render_app_icon.mm'), str(ROOT / 'Sources/Assets/Image.cpp'),
                    '-framework', 'Foundation',
                    '-framework', 'CoreGraphics', '-framework', 'CoreText', '-framework', 'ImageIO',
                    '-o', str(renderer)], check=True)
    output.mkdir(parents=True, exist_ok=True)
    info = {'info': {'author': 'xcode', 'version': 1}}
    (output / 'Contents.json').write_text(json.dumps(info, indent=2) + '\n')
    records = []
    for name, variant in [('AppIcon', 'normal'), ('AppIconValidation', 'validation')]:
        folder = output / (name + '.appiconset')
        folder.mkdir(exist_ok=True)
        icon = folder / 'AppIcon1024.png'
        subprocess.run([str(renderer), str(input_file), str(icon), variant], check=True)
        metadata = dict(info, images=[{'filename': icon.name, 'idiom': 'universal',
                                     'platform': 'ios', 'size': '1024x1024'}])
        (folder / 'Contents.json').write_text(json.dumps(metadata, indent=2) + '\n')
        records.append({'catalog': name, 'variant': variant, 'pixels': [1024, 1024],
                        'sha256': hashlib.sha256(icon.read_bytes()).hexdigest()})
    manifest = {'source_ipa_sha256': IPA_SHA, 'source_member': ART_MEMBER,
                'source_artwork_sha256': ART_SHA, 'source_pixels': [768, 1024],
                'source_crop_top_left_xywh': [0, 128, 768, 768],
                'decoder': 'hm2::decodeImage, shared original CgBI reader',
                'normal': 'Original tap-to-start illustration, centered square crop, uniform resize to opaque 1024 sRGB PNG',
                'validation': 'Same illustration with a 160px opaque TEST footer',
                'in_game_assets_modified': False, 'icons': records}
    (local / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps({'catalog': str(output), 'icons': records}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=ROOT / '.local/prepared/AppIcons.xcassets')
    prepare(parser.parse_args().output)
