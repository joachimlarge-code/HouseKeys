#!/bin/bash
# Installerer HouseKeys (VST3 + AU) på Mac.
# Kjør i Terminal:
#   curl -fsSL https://raw.githubusercontent.com/joachimlarge-code/housekeys/main/install.sh | bash
set -e

BASE="https://github.com/joachimlarge-code/housekeys/releases/latest/download"
TMP="$(mktemp -d)"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"

echo "Laster ned HouseKeys..."
curl -fsSL "$BASE/HouseKeys.vst3.zip"      -o "$TMP/vst3.zip"
curl -fsSL "$BASE/HouseKeys.component.zip" -o "$TMP/au.zip"

mkdir -p "$VST3_DIR" "$AU_DIR"
rm -rf "$VST3_DIR/HouseKeys.vst3" "$AU_DIR/HouseKeys.component"

echo "Installerer..."
ditto -x -k "$TMP/vst3.zip" "$VST3_DIR"
ditto -x -k "$TMP/au.zip"   "$AU_DIR"
xattr -cr "$VST3_DIR/HouseKeys.vst3" "$AU_DIR/HouseKeys.component" 2>/dev/null || true
rm -rf "$TMP"

echo ""
echo "Ferdig! HouseKeys er installert:"
ls -d "$VST3_DIR/HouseKeys.vst3" "$AU_DIR/HouseKeys.component"
echo ""
echo "Åpne FL Studio -> Options -> Manage plugins -> Find installed plugins."
