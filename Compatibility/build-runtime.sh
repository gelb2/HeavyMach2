#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PATH="$HOME/.cargo/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin"
export CARGO_TARGET_DIR="$ROOT/.local/runtime-target"
export IPHONEOS_DEPLOYMENT_TARGET=15.0 CMAKE_POLICY_VERSION_MINIMUM=3.5
TARGET="${1:-aarch64-apple-ios-sim}"
case "$TARGET" in aarch64-apple-ios|aarch64-apple-ios-sim|aarch64-apple-darwin) ;; *) echo 'Unsupported target' >&2; exit 1;; esac
cargo +1.99.1 build --manifest-path "$ROOT/.local/compat-source/upstream/fresh-port/20-touchHLE-src/touchHLE/Cargo.toml" --locked --release --target "$TARGET" --no-default-features --features static,cpu_interpreter --bin touchHLE
