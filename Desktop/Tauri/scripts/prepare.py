#!/usr/bin/env python3
"""Verify and package the normal PWA output without editing its files."""

import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tempfile


DESKTOP = Path(__file__).resolve().parents[1]
REPO = DESKTOP.parents[1]
LAUNCHER_ICON = "icon-512.png"
REQUIRED_FILES = (
    "index.html", "app.js", "audio.js", "display.js", "storage.js", "style.css",
    "game.js", "game.wasm", "game.data", LAUNCHER_ICON,
)


def copy_verified(source, target):
    manifest_file = source / "asset-manifest.json"
    manifest = json.loads(manifest_file.read_text(encoding="utf-8"))
    if manifest.get("validation") is not False:
        raise ValueError("The desktop app requires the normal PWA build")
    seen = set()
    for entry in manifest["files"]:
        name = entry["path"]
        path = PurePosixPath(name)
        if (not name or path.is_absolute() or "\\" in name
                or ".." in path.parts or str(path) != name
                or name == manifest_file.name or name in seen):
            raise ValueError(f"Invalid packaged path: {name}")
        seen.add(name)
        original = source / path
        if original.is_symlink() or not original.resolve().is_relative_to(source.resolve()):
            raise ValueError(f"Packaged path leaves the web build: {name}")
        destination = target / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(original, destination)
        data = destination.read_bytes()
        if len(data) != entry["bytes"] or hashlib.sha256(data).hexdigest() != entry["sha256"]:
            raise ValueError(f"Web build file did not match its manifest: {name}")
    for name in REQUIRED_FILES:
        if name not in seen:
            raise ValueError(f"Missing desktop web dependency: {name}")
    shutil.copyfile(manifest_file, target / manifest_file.name)


def prepare():
    build = DESKTOP / "build"
    build.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=build) as path:
        stage = Path(path)
        frontend = stage / "frontend"
        icons = stage / "icons"
        generated = stage / "generated-icons"
        frontend.mkdir()
        copy_verified(REPO / "build/web-normal/dist", frontend)
        subprocess.run(
            [str(DESKTOP / "node_modules/.bin/tauri"), "icon",
             str(frontend / LAUNCHER_ICON), "--output", str(generated)],
            check=True,
        )
        icons.mkdir()
        for name in ("32x32.png", "icon.icns"):
            shutil.copyfile(generated / name, icons / name)
        for directory in (frontend, icons):
            destination = build / directory.name
            if destination.exists():
                shutil.rmtree(destination)
            directory.rename(destination)
    print("Verified normal PWA files and staged the Mac bundle inputs")


if __name__ == "__main__":
    prepare()
