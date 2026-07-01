#!/usr/bin/env bash
# DEPRECATED: The app installs BlackHole directly (see shell/app/DriverInstaller.cpp).
# This script remains for reference; the production InputMixer.driver HAL pkg replaces both.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_NAME="InputMixer-Driver.pkg"
BUILD_DIR="$SCRIPT_DIR/build-pkg"
PAYLOAD_DIR="$BUILD_DIR/payload"
SCRIPTS_DIR="$BUILD_DIR/scripts"

rm -rf "$BUILD_DIR"
mkdir -p "$PAYLOAD_DIR/Library/Application Support/InputMixer"
mkdir -p "$SCRIPTS_DIR"

cat > "$PAYLOAD_DIR/Library/Application Support/InputMixer/README.txt" <<'EOF'
Input Mixer virtual microphone (development installer)

This development package prepares your Mac for the Input Mixer virtual microphone.

The production HAL audio driver will be installed to:
  /Library/Audio/Plug-Ins/HAL/InputMixer.driver

Until that driver ships, Input Mixer also recognizes BlackHole as a stand-in virtual
microphone for development and testing.

After the production driver is available, reinstall from Input Mixer setup to replace
this development package.
EOF

cat > "$SCRIPTS_DIR/postinstall" <<'EOF'
#!/bin/bash
set -e

MARKER_DIR="/Library/Application Support/InputMixer"
mkdir -p "$MARKER_DIR"
touch "$MARKER_DIR/.driver-pkg-installed"

launchctl kickstart -kp system/com.apple.audio.coreaudiod 2>/dev/null || true

exit 0
EOF

chmod 755 "$SCRIPTS_DIR/postinstall"

pkgbuild \
  --root "$PAYLOAD_DIR" \
  --scripts "$SCRIPTS_DIR" \
  --identifier "com.inputmixer.driver" \
  --version "0.1.0" \
  --install-location "/" \
  "$SCRIPT_DIR/$PKG_NAME"

echo "Created $SCRIPT_DIR/$PKG_NAME"
