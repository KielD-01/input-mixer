#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "==> Building Rust core"
source "$HOME/.cargo/env" 2>/dev/null || true
cargo build --release

echo "==> Building JUCE shell"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

echo "==> Done"
if [[ "$(uname)" == "Darwin" ]]; then
  find build -name "*.app" -maxdepth 5 2>/dev/null || true
fi
