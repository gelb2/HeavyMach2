#!/usr/bin/env python3
"""Reject IPA paths, known original blobs/LFS pointers and renamed IPA ZIPs."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess
import sys
import zipfile


def violation(name, data, original_hashes=()):
    if any(part.lower().endswith('.ipa') for part in Path(name).parts):
        return 'IPA path'
    if hashlib.sha256(data).hexdigest() in original_hashes:
        return 'Original package bytes'
    if data.startswith(b'version https://git-lfs.github.com/spec/v1\n'):
        match = re.search(rb'oid sha256:([0-9a-f]{64})', data)
        if match and match[1].decode() in original_hashes:
            return 'Original package LFS pointer'
    if zipfile.is_zipfile(io.BytesIO(data)):
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            names = [n.replace('\\', '/') for n in archive.namelist()]
            if any(n.lower().endswith('.ipa') for n in names):
                return 'Archive containing IPA'
            if any(re.search(r'(^|/)Payload/[^/]+\.app/', n, re.I) for n in names):
                return 'Renamed IPA package'
    return None


def git(*args):
    return subprocess.check_output(['git', *args])


def scan(all_history=False):
    root = Path(git('rev-parse', '--show-toplevel').decode().strip())
    manifest = json.loads((root/'docs/source-provenance.json').read_text())
    originals = {x['sha256'] for x in manifest['files']}
    trees = git('rev-list', '--all').decode().splitlines() if all_history else [None]
    seen = set()
    violations = []
    for tree in trees:
        entries = git('ls-tree', '-rz', tree) if tree else git('ls-files', '--stage', '-z')
        for entry in entries.split(b'\0'):
            if not entry:
                continue
            meta, path = entry.split(b'\t', 1)
            fields = meta.split()
            if tree:
                if fields[1] != b'blob':
                    continue
                oid = fields[2]
            else:
                if fields[0] == b'160000':
                    continue
                oid = fields[1]
            key = (oid, path)
            if key in seen:
                continue
            seen.add(key)
            data = git('cat-file', 'blob', oid.decode())
            name = path.decode('utf-8', 'surrogateescape')
            reason = violation(name, data, originals)
            if reason:
                violations.append({'path':name,'reason':reason,'commit':tree or 'index'})
    for item in violations:
        print('BLOCK:', json.dumps(item, ensure_ascii=True), file=sys.stderr)
    print(f'IPA guard: {len(seen)} file versions checked, {len(violations)} violations')
    return bool(violations)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--all', action='store_true', help='Check all reachable commit trees')
    parser.add_argument('--staged', action='store_true', help='Check the complete index (default)')
    sys.exit(scan(parser.parse_args().all))
