#!/usr/bin/env python3
"""Prepare build inputs from the pinned public PWA release."""
import argparse
from decimal import Decimal
import hashlib
import json
import mmap
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tempfile
import urllib.request


ROOT = Path(__file__).resolve().parents[1]
RELEASE = '2eea699b1b030222fe2f3f26860179bb4d0ce467'
PACKAGE_URL = f'https://raw.githubusercontent.com/gelb2/HeavyMach2/{RELEASE}/'
PACKAGE_FILES = {
    'game.js': (259690, '6da906d39693afef9f233839bd62f912532af78a816e026edb25218236b3822a'),
    'game.data': (91435806, '5c82f60dd03ed67b8740c28cdc920e42e09c0505a42263fe88c860cc34fe00bb'),
}
REQUIRED = {'manifest.json', 'MapInfo.mcb', 'ItemInfo.tble', 'Main_BG.png'}
OFFSET = r'(\d+(?:e\d+)?)'
ENTRY = re.compile(r'\{filename:("(?:[^"\\]|\\.)*"),start:' + OFFSET + ',end:' + OFFSET + r'\}')


def download(cache, name):
    size, digest = PACKAGE_FILES[name]
    dest = cache / name

    def verified(path):
        if not path.is_file() or path.stat().st_size != size:
            return False
        with path.open('rb') as stream:
            return hashlib.file_digest(stream, 'sha256').hexdigest() == digest

    if not verified(dest):
        print(f'Downloading {name} from the pinned public PWA build', flush=True)
        with tempfile.TemporaryDirectory(dir=cache) as temporary:
            candidate = Path(temporary) / name
            with urllib.request.urlopen(PACKAGE_URL + name, timeout=60) as response, candidate.open('wb') as stream:
                while chunk := response.read(1024 * 1024):
                    if stream.tell() + len(chunk) > size:
                        raise ValueError(f'{name}: download exceeds the expected size')
                    stream.write(chunk)
            if not verified(candidate):
                raise ValueError(f'{name}: release size or SHA-256 mismatch')
            candidate.replace(dest)
    return dest


def file_ranges(loader, size):
    package = re.search(r'\(\{files:\[(.*?)\],remote_package_size:' + OFFSET + r'\}\)', loader, re.S)
    if not package or Decimal(package[2]) != size:
        raise ValueError('The loader does not describe this data package')
    rows = list(ENTRY.finditer(package[1]))
    if not rows or ','.join(row[0] for row in rows) != package[1]:
        raise ValueError('Unrecognized release file table')
    names = set()
    ranges = {}
    cursor = 0
    for row in rows:
        full_name = json.loads(row[1])
        if not full_name.startswith('/campaign/'):
            raise ValueError(f'Unexpected package path {full_name!r}')
        name = full_name.removeprefix('/campaign/')
        path = PurePosixPath(name)
        if (any(part in {'', '.', '..'} for part in name.split('/'))
                or any(char in name for char in ('\\', ':', '\0')) or path.suffix.lower() == '.ipa'
                or name.casefold() in names):
            raise ValueError(f'Unsafe or duplicate resource path {name!r}')
        start, end = int(Decimal(row[2])), int(Decimal(row[3]))
        if not 0 <= start == cursor <= end <= size:
            raise ValueError(f'Invalid package offsets for {name!r}')
        names.add(name.casefold())
        ranges[name] = (start, end)
        cursor = end
    if cursor != size or not REQUIRED <= ranges.keys():
        raise ValueError('The package is missing campaign data')
    return ranges


def prepare_campaign(data, files, output):
    output.parent.mkdir(parents=True, exist_ok=True)
    with data.open('rb') as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as payload:
        if output.exists():
            paths = {p.relative_to(output).as_posix() for p in output.rglob('*') if p.is_file()}
            if paths != files.keys() or any((output / name).read_bytes() != payload[start:end]
                                           for name, (start, end) in files.items()):
                raise ValueError(f'{output} already contains different resources; use a clean checkout')
            return
        with tempfile.TemporaryDirectory(dir=output.parent) as temporary:
            staging = Path(temporary) / 'HD'
            for name, (start, end) in files.items():
                path = staging / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(payload[start:end])
            staging.rename(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--campaign-only', action='store_true', help='Skip the Apple/PWA icon catalogs for Android-only builds')
    args = parser.parse_args()
    cache = ROOT / '.local/downloads' / RELEASE
    cache.mkdir(parents=True, exist_ok=True)
    loader = download(cache, 'game.js')
    data = download(cache, 'game.data')
    output = ROOT / '.local/prepared/Campaign/HD'
    files = file_ranges(loader.read_text(encoding='utf-8'), data.stat().st_size)
    prepare_campaign(data, files, output)
    print(f'Verified {len(files)} campaign files in {output}')
    if not args.campaign_only:
        subprocess.run([sys.executable, ROOT / 'tools/prepare_app_icons.py', '--from-campaign'], check=True)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        sys.exit(f'Could not prepare build resources: {error}')
