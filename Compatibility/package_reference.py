#!/usr/bin/env python3
"""Package a preserved, hash-checked IPA into a LOCAL reference host only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / '.local/compat-source/upstream/fresh-port/20-touchHLE-src/touchHLE'
OUTPUT = Path.home() / 'Documents/Codex/HeavyMach2/ReferenceBuilds'


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def external_output(path, output_root=OUTPUT):
    path = Path(path).resolve()
    if not path.is_relative_to(output_root.resolve()) or path.is_relative_to(ROOT):
        raise ValueError('Original-containing output must stay in the external HM2 ReferenceBuilds directory')
    return path


def package(profile, target, xcode=False):
    config = json.loads((ROOT / 'Compatibility/profiles.json').read_text())[profile]
    manifest = json.loads((ROOT / 'docs/source-provenance.json').read_text())
    original = manifest['files'][config['source_index']]
    ipa = (Path(manifest['source_root']) / original['relative_path']).resolve()
    if ipa.is_relative_to(ROOT) or sha(ipa) != original['sha256']:
        raise ValueError('Original IPA location/hash mismatch')
    exe = ROOT / '.local/runtime-target' / target / 'release/touchHLE'
    if not exe.is_file():
        raise FileNotFoundError(exe)
    if xcode:
        app = external_output(Path(os.environ['TARGET_BUILD_DIR']) / os.environ['WRAPPER_NAME'])
    else:
        app = external_output(OUTPUT / target / (config['product'] + '.app'))
    app.mkdir(parents=True, exist_ok=True)
    shutil.copy2(exe, app / config['product'])
    os.utime(app / config['product'], None)
    # These two internal filenames are the inherited runtime entrypoint contract.
    # Each host packages exactly one corresponding HM2 original, never HeavyMach1.
    (app / 'HeavyMach_options.txt').write_text('\n'.join(config['options']) + '\n')
    shutil.copy2(ipa, app / 'HeavyMach.ipa')
    shutil.copytree(SOURCE / 'touchHLE_fonts', app / 'touchHLE_fonts', dirs_exist_ok=True)
    shutil.copy2(SOURCE / 'touchHLE_default_options.txt', app / 'touchHLE_default_options.txt')
    with zipfile.ZipFile(app / 'touchHLE_dylibs.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for p in sorted((SOURCE / 'touchHLE_dylibs').iterdir()):
            if p.is_file():
                archive.write(p, p.name)
    if not xcode:
        info = plistlib.loads((ROOT / 'Compatibility/ios/Info.plist').read_bytes())
        info.update(CFBundleExecutable=config['product'], CFBundleIdentifier=config['bundle_id'],
                    CFBundleName=config['product'], CFBundleDisplayName=config['display_name'],
                    CFBundleShortVersionString=config['version'], UIDeviceFamily=config['host_family'],
                    CFBundleSupportedPlatforms=['iPhoneSimulator' if target.endswith('-sim') else 'iPhoneOS'])
        (app / 'Info.plist').write_bytes(plistlib.dumps(info))
    if sha(ipa) != original['sha256'] or sha(app / 'HeavyMach.ipa') != original['sha256']:
        raise ValueError('IPA identity changed during packaging')
    evidence = {'profile': profile, 'target': target, 'ipa_sha256': original['sha256'],
                'runtime_sha256': sha(exe), 'bundle_id': config['bundle_id'],
                'options': config['options'], 'local_only': True}
    (app / 'ReferenceProvenance.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(app)
    return app


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('profile', choices=sorted(json.loads((ROOT / 'Compatibility/profiles.json').read_text())))
    parser.add_argument('--target', choices=['aarch64-apple-ios', 'aarch64-apple-ios-sim'], required=True)
    parser.add_argument('--xcode', action='store_true')
    args = parser.parse_args()
    package(args.profile, args.target, args.xcode)
