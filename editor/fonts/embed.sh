#!/bin/sh
# Regenerates src/gen/*.inc from the TTFs (compressed, for AddFontFromMemoryCompressedTTF).
set -e
cd "$(dirname "$0")/.."
mkdir -p build src/gen
[ -x build/bin2c ] || g++ -O2 -o build/bin2c third_party/imgui/binary_to_compressed_c.cpp
for f in Oxanium-Medium Oxanium-SemiBold Saira-Regular Saira-Medium JetBrainsMono-Regular JetBrainsMono-Medium; do
  sym=$(echo "$f" | tr 'A-Z-' 'a-z_')
  ./build/bin2c -u8 "fonts/$f.ttf" "$sym" > "src/gen/$sym.inc"
done
ls src/gen
