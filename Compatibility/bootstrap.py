#!/usr/bin/env python3
"""Prepare an independent pinned runtime; preserve existing work on mismatch."""
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CONFIG = json.loads((ROOT / 'Compatibility/runtime.json').read_text())
UPSTREAM = ROOT / '.local/compat-source/upstream'


def run(*args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def prepare_patch(repo, patch):
    if subprocess.run(['git', '-C', str(repo), 'apply', '--reverse', '--check', str(patch)],
                      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0:
        return
    run('git', '-C', str(repo), 'apply', '--check', str(patch))
    run('git', '-C', str(repo), 'apply', str(patch))


def pinned_checkout(path, url, commit, sparse=None):
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        run('git', 'clone', '--filter=blob:none', '--no-checkout', url, str(path))
        if sparse:
            run('git', '-C', str(path), 'sparse-checkout', 'set', sparse)
        run('git', '-C', str(path), 'checkout', '--detach', commit)
    actual = subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != commit:
        raise SystemExit(f'Preserving existing checkout with unexpected commit: {path}')


if __name__ == '__main__':
    pinned_checkout(UPSTREAM, CONFIG['repository'], CONFIG['commit'], CONFIG['source'])
    for patch in CONFIG.get('patches', ['heavymach.patch']):
        prepare_patch(UPSTREAM, ROOT / 'Compatibility/patches' / patch)
    sdl = UPSTREAM / CONFIG['source'] / 'vendor/rust-sdl2'
    pinned_checkout(sdl, CONFIG['sdl_repository'], CONFIG['sdl_commit'])
    prepare_patch(sdl, ROOT / 'Compatibility/patches/rust-sdl2.patch')
    print('Pinned runtime source ready. Install Rust 1.99.1 and the iOS targets using rustup if absent.')
