#!/usr/bin/env python3
"""Check the installed edition's build inputs, APK assets, native ELF and signing."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PREPARED = ROOT / ".local/prepared/Campaign/HD"
BASELINE = "43ae492a56ea58881b84a36f51d385cc092bc981"
SDK = Path(os.environ.get("ANDROID_HOME", Path.home() / "Library/Android/sdk"))
BUILD_TOOLS = SDK / "build-tools/36.0.0"
EDITION = {"normal": "com.gelb2.heavymach2.restored", "cheat": "com.gelb2.heavymach2.validation"}


def require(value, reason):
    if not value:
        raise ValueError(reason)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def protected_paths():
    allow = ["Android/**", "Sources/Platform/Android/**", "docs/android-port-plan-20261120.md"]
    scope = ["--", ".", *[f":(exclude){path}" for path in allow]]
    for command in (["diff", "--name-only", BASELINE], ["ls-files", "--others", "--exclude-standard"]):
        changes = subprocess.check_output(["git", *command, *scope], cwd=ROOT).decode().strip()
        require(not changes, f"Changes outside Android scope: {changes}")


def verify_native(data):
    require(data[:6] == b"\x7fELF\x02\x01", "Expected a little-endian 64-bit ELF")
    header = struct.unpack_from("<16sHHIQQQIHHHHHH", data)
    offset, size, count = header[5], header[9], header[10]
    alignments = []
    for index in range(count):
        segment = struct.unpack_from("<IIQQQQQQ", data, offset + index * size)
        if segment[0] == 1:  # PT_LOAD
            alignments.append(segment[-1])
    require(alignments and all(value >= 16384 for value in alignments), f"Unsuitable ELF alignment: {alignments}")
    return alignments


def validate_build_inputs(map_path, cheat):
    records = json.loads(map_path.with_name("compile_commands.json").read_text())
    files = {Path(row["file"]).resolve() for row in records}
    require(ROOT / "Sources/Core/Session/GameSession.cpp" in files, "Shared GameSession source missing")
    require(ROOT / "Sources/Core/UI/Screens.cpp" in files, "Shared Screens source missing")
    for row in records:
        require("Sources/Platform/Apple" not in str(row) and str(ROOT / "Apple") not in str(row),
                "Apple file referenced in Android compilation")
    link = map_path.read_text()
    require("libHeavyMach2Core.a(GameSession.cpp.o)" in link, "Missing linked game core")
    for object_name in ("libHeavyMach2Core.a(ConcurrentValidation.cpp.o)", "Android/Validation.cpp.o"):
        require((object_name in link) == cheat, f"Unexpected validation linkage for {object_name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("edition", choices=EDITION)
    parser.add_argument("apk", type=Path)
    parser.add_argument("--map", dest="maps", type=Path, action="append", required=True,
                        help="Linker map for each packaged ABI")
    parser.add_argument("--prepared-before", type=Path, help="Pre-build JSON of path to SHA-256")
    args = parser.parse_args()
    protected_paths()
    all_inputs = {p.relative_to(PREPARED).as_posix(): digest(p.read_bytes())
                  for p in sorted(PREPARED.rglob("*")) if p.is_file()}
    require("manifest.json" in all_inputs and "MapInfo.mcb" in all_inputs, "Missing prepared campaign")
    if args.prepared_before:
        require(json.loads(args.prepared_before.read_text()) == all_inputs, "Prepared assets changed")
    expected = dict(all_inputs)
    del expected["manifest.json"]
    with zipfile.ZipFile(args.apk) as archive:
        names = archive.namelist()
        require(len(set(names)) == len(names), "Duplicate APK entries")
        game_assets = {name[7:] for name in names if name.startswith("assets/") and not name.endswith("/")}
        # These two files belong to the Android baseline-profile packager.
        game_assets.difference_update({"dexopt/baseline.prof", "dexopt/baseline.profm"})
        require(game_assets == set(expected),
                f"APK asset inventory differs: {sorted(game_assets.symmetric_difference(expected))[:12]}")
        for path, sha in expected.items():
            require(digest(archive.read("assets/" + path)) == sha, f"Changed asset {path}")
        native = {name: verify_native(archive.read(name)) for name in names
                  if name.startswith("lib/") and name.endswith(".so")}
        abis = {name.split("/")[1] for name in native}
        require(abis and all(f"lib/{abi}/libheavymach2.so" in native for abi in abis), "Missing runtime")
    require(sorted(path.parent.name for path in args.maps) == sorted(abis), "Linker maps must match APK ABIs")
    for path in args.maps:
        validate_build_inputs(path, args.edition == "cheat")
    subprocess.run([BUILD_TOOLS / "zipalign", "-c", "-P", "16", "4", args.apk], check=True)
    signer = subprocess.check_output([BUILD_TOOLS / "apksigner", "verify", "--print-certs", args.apk], text=True)
    cert = next(line.split(": ", 1)[1] for line in signer.splitlines() if "certificate SHA-256 digest:" in line)
    badging = subprocess.check_output([BUILD_TOOLS / "aapt", "dump", "badging", args.apk], text=True)
    require(f"package: name='{EDITION[args.edition]}'" in badging, "Incorrect app ID")
    require(("application-debuggable" in badging) == (args.edition == "cheat"), "Unexpected debug flag")
    print(json.dumps(dict(app=EDITION[args.edition], apkSHA256=digest(args.apk.read_bytes()),
                          signerSHA256=cert, assets=len(expected), native=native,
                          unchangedPrepared=bool(args.prepared_before), protectedBaseline=BASELINE), indent=2))


if __name__ == "__main__":
    main()
