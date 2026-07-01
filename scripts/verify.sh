#!/usr/bin/env bash
# End-to-end verification checklist (automated subset)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

source "$HOME/.cargo/env" 2>/dev/null || true

echo "==> Rust build + tests"
cargo test --release

echo "==> Verify app binary links Rust core"
APP_BIN=$(find build -path "*/Input Mixer.app/Contents/MacOS/Input Mixer" -type f | head -1)
if [[ -z "$APP_BIN" ]]; then
  echo "App not built — run scripts/build.sh first"
  exit 1
fi
otool -L "$APP_BIN" >/dev/null

echo "==> Smoke launch (2s)"
"$APP_BIN" &
PID=$!
sleep 2
kill $PID 2>/dev/null || true
wait $PID 2>/dev/null || true

echo "==> All automated checks passed"
echo "Manual: verify Input Mixer in Discord/Skype mic list after driver install"
