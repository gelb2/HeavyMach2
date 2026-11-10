#!/usr/bin/env python3
"""Build HM2's independently vendored OpenAL Soft, without game resources.

Adapted from MachDefense 0aa03aa7 tools/build_openal.py (MIT). Each SDK has
its own HM2 build directory. This command neither installs nor launches apps.
"""
import argparse
import fcntl
import os
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--sdk", choices=["iphonesimulator", "iphoneos", "macosx"], default="iphonesimulator")
args = parser.parse_args()
cmake = shutil.which("cmake")
if not cmake:
    raise SystemExit("CMake is required")
suffix = {"iphonesimulator": "simulator", "iphoneos": "device", "macosx": "host"}[args.sdk]
out = root / "build" / f"audio-{suffix}-openal"
out.mkdir(parents=True, exist_ok=True)
with (out / "build.lock").open("w") as lock:
    fcntl.flock(lock, fcntl.LOCK_EX)
    env = os.environ.copy()
    env.pop("SDKROOT", None)
    subprocess.run([
        cmake, "-S", str(root / "ThirdParty/OpenALSoft"), "-B", str(out),
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
        "-DCMAKE_SYSTEM_NAME=" + ("Darwin" if args.sdk == "macosx" else "iOS"),
        "-DCMAKE_OSX_SYSROOT=" + args.sdk, "-DCMAKE_OSX_ARCHITECTURES=arm64",
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=" + ("12.0" if args.sdk == "macosx" else "15.0"),
        "-DCMAKE_BUILD_TYPE=Release", "-DLIBTYPE=STATIC", "-DALSOFT_UTILS=OFF",
        "-DALSOFT_EXAMPLES=OFF", "-DALSOFT_NO_CONFIG_UTIL=ON", "-DALSOFT_BACKEND_WAVE=OFF",
        "-DALSOFT_REQUIRE_COREAUDIO=ON", "-DALSOFT_INSTALL=OFF", "-DALSOFT_UPDATE_BUILD_VERSION=OFF",
    ], check=True, env=env)
    subprocess.run([cmake, "--build", str(out), "--target", "OpenAL", "--parallel", "4"], check=True, env=env)
print(out / "libopenal.a")
