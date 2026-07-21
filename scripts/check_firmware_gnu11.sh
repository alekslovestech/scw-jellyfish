#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE="$ROOT/firmware/JellyfishForest"
STUBS="$ROOT/scripts/arduino_stubs"
BUILD="$(mktemp -d "${TMPDIR:-/tmp}/jellyfish-gnu11.XXXXXX")"
trap 'rm -rf "$BUILD"' EXIT

CXXFLAGS=(
  -std=gnu++11
  -Wall
  -Wextra
  -Wpedantic
  -Werror=return-type
  -DARDUINO_ARCH_ESP32
  -DCONFIG_IDF_TARGET_ESP32
  -I"$STUBS"
  -I"$SOURCE"
)

for source in "$SOURCE"/*.cpp; do
  name="$(basename "$source" .cpp)"
  g++ "${CXXFLAGS[@]}" -c "$source" -o "$BUILD/$name.o"
done

g++ "${CXXFLAGS[@]}" -x c++ -c \
  "$SOURCE/JellyfishForest.ino" -o "$BUILD/JellyfishForest.o"
g++ "${CXXFLAGS[@]}" -c \
  "$STUBS/globals.cpp" -o "$BUILD/globals.o"

printf '%s\n' 'Firmware GNU++11 host syntax check passed.'
