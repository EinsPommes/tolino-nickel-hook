#!/bin/sh
# Cross-compile the Stage A preload library for the reader.
#
# Target: Tolino Vision Color / Kobo Libra Colour - MediaTek mt8113t, 32-bit
# ARM Cortex-A, hard-float (armv7-a). libnickel.so.1.0.0 was confirmed ELF
# arm (32-bit) during recon, so the preload must match that ABI.
#
# Requires a glibc hard-float cross-toolchain:
#   Debian/Ubuntu:  sudo apt-get install gcc-arm-linux-gnueabihf
#
# Usage:  scripts/build.sh
# Output: build/libtnh.so  (depends only on the device's glibc)

set -e

CC="${CC:-arm-linux-gnueabihf-gcc}"
command -v "$CC" >/dev/null 2>&1 || {
    echo "cross-compiler '$CC' not found." >&2
    echo "install it, e.g.: sudo apt-get install gcc-arm-linux-gnueabihf" >&2
    exit 1
}

script_dir=$(cd "$(dirname "$0")" && pwd)
root_dir=$(cd "$script_dir/.." && pwd)
out_dir="$root_dir/build"
mkdir -p "$out_dir"

"$CC" -shared -fPIC -O2 -Wall -Wextra -pthread \
    -march=armv7-a -mfpu=neon -mfloat-abi=hard \
    -Wl,--no-undefined \
    -o "$out_dir/libtnh.so" \
    "$root_dir/src/hook.c"

echo "built: $out_dir/libtnh.so"
"${READELF:-arm-linux-gnueabihf-readelf}" -h "$out_dir/libtnh.so" 2>/dev/null \
    | grep -E 'Class|Machine|Type' || true
