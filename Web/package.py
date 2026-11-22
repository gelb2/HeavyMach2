#!/usr/bin/env python3
"""Finalize a local web build and give its cache a reproducible edition-specific identity."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

shell, dist = map(Path, sys.argv[1:3])
edition = sys.argv[3]
if edition not in {"normal", "validation"}:
    raise ValueError("Unknown web edition")
validation = edition == "validation"
static = ("index.html", "app.js", "storage.js", "audio.js", "display.js", "install.js", "download.js", "style.css",
          "social-preview.jpg")
icon_sizes = (192, 512)
required = ("game.js", "game.wasm", "game.data")
if any(not (dist / name).is_file() for name in required):
    raise FileNotFoundError("Missing Emscripten output")

catalog = Path(__file__).resolve().parents[1] / ".local/prepared/AppIcons.xcassets"
icon_set = "AppIconValidation" if validation else "AppIcon"
artwork = catalog / f"{icon_set}.appiconset/AppIcon1024.png"
if not artwork.is_file():
    raise FileNotFoundError(f"Missing {artwork}. Run python3 tools/prepare_app_icons.py first.")

def describe(path):
    return {"path": path.name, "bytes": path.stat().st_size,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}

for name in static:
    payload = (shell / name).read_bytes()
    if name == "index.html":
        payload = payload.replace(b"%VALIDATION%", str(validation).lower().encode())
        payload = payload.replace(b"%TITLE%", b"HeavyMach 2 QA" if validation else b"HeavyMach 2")
    (dist / name).write_bytes(payload)

icons = []
for size in icon_sizes:
    path = dist / f"icon-{size}.png"
    pixels = str(size)
    subprocess.run(["sips", "-z", pixels, pixels, str(artwork), "--out", str(path)],
                   check=True, stdout=subprocess.DEVNULL)
    icons.append({"src": path.name, "sizes": f"{size}x{size}", "type": "image/png"})

manifest = {"id": f"./{edition}", "name": "HeavyMach 2 QA" if validation else "HeavyMach 2",
            "short_name": "HM2 QA" if validation else "HeavyMach 2", "start_url": "./",
            "scope": "./", "display": "standalone", "theme_color": "#101b20",
            "background_color": "#101b20",
            "icons": icons}
# Keep the existing manifest URL and app ID for phones already installed.
phone_manifest = {**manifest, "display": "fullscreen", "orientation": "landscape"}
(dist / "manifest.webmanifest").write_text(json.dumps(phone_manifest, indent=2) + "\n")
(dist / "manifest-flexible.webmanifest").write_text(json.dumps(manifest, indent=2) + "\n")
names = sorted((*required, *static, *(icon["src"] for icon in icons),
                "manifest.webmanifest", "manifest-flexible.webmanifest"))
description = [describe(dist / name) for name in names]
worker = (shell / "sw.js").read_text()
version = hashlib.sha256(json.dumps([edition, description, worker], sort_keys=True).encode()).hexdigest()[:24]
worker = worker.replace("%EDITION%", edition).replace("%VERSION%", version)
worker = worker.replace("%FILES%", json.dumps(description))
(dist / "sw.js").write_text(worker)
description.append(describe(dist / "sw.js"))
(dist / "asset-manifest.json").write_text(json.dumps({"version": version,
    "validation": validation, "files": description}, indent=2) + "\n")
