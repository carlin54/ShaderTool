#!/usr/bin/env bash
# Bundle ShaderTool + Qt on Linux using linuxdeploy.
# Usage:
#   ./scripts/deploy_linux.sh [path/to/ShaderTool] [output-dir]
# Example:
#   ./scripts/deploy_linux.sh ./build/ShaderTool ./dist/linux

set -euo pipefail

BIN="${1:-./build/ShaderTool}"
OUT="${2:-./dist/linux}"
APPDIR="${OUT}/ShaderTool.AppDir"

if [[ ! -f "$BIN" ]]; then
  echo "error: executable not found: $BIN"
  echo "hint: build first with: cmake -S . -B build && cmake --build build -j"
  exit 1
fi

if ! command -v linuxdeploy >/dev/null 2>&1; then
  echo "error: linuxdeploy not found in PATH"
  echo "download: https://github.com/linuxdeploy/linuxdeploy/releases"
  exit 1
fi

if [[ -z "${LINUXDEPLOY_PLUGIN_QT:-}" ]] && ! command -v linuxdeploy-plugin-qt >/dev/null 2>&1; then
  echo "error: linuxdeploy-plugin-qt not found"
  echo "set LINUXDEPLOY_PLUGIN_QT=/path/to/linuxdeploy-plugin-qt"
  exit 1
fi

PLUGIN_QT="${LINUXDEPLOY_PLUGIN_QT:-$(command -v linuxdeploy-plugin-qt)}"

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$OUT"
cp -f "$BIN" "$APPDIR/usr/bin/ShaderTool"
chmod +x "$APPDIR/usr/bin/ShaderTool"

if [[ -n "${SHADERTOOL_DXC:-}" && -f "${SHADERTOOL_DXC}" ]]; then
  cp -f "${SHADERTOOL_DXC}" "$APPDIR/usr/bin/dxc"
  chmod +x "$APPDIR/usr/bin/dxc"
elif command -v dxc >/dev/null 2>&1; then
  cp -f "$(command -v dxc)" "$APPDIR/usr/bin/dxc"
  chmod +x "$APPDIR/usr/bin/dxc"
fi

echo "Running linuxdeploy..."
LINUXDEPLOY_PLUGIN_QT="$PLUGIN_QT" \
  linuxdeploy --appdir "$APPDIR" --executable "$APPDIR/usr/bin/ShaderTool" --plugin qt

TAR_OUT="${OUT}/ShaderTool-linux-portable.tar.gz"
tar czf "$TAR_OUT" -C "$OUT" "ShaderTool.AppDir"
echo "Created: $TAR_OUT"
