#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
cxx=${CXX:-g++}
out=${SCENE_TEST_OUTPUT:-/tmp/test-rs97-scene}
# Preserve the two-word INLINE macro as a single compiler argument.
"$cxx" -std=c++03 -O1 -g -ffunction-sections -fdata-sections \
  -DUSE_GPULIB -DGPU_UNAI -DGPU_UNAI_LOWRES_EXPERIMENT -DGPU_UNAI_FULL_SCENE \
  '-DINLINE=static inline' -Isrc -Isrc/port/sdl -Isrc/gpu/gpu_unai -Isrc/plugin_lib \
  ${SCENE_TEST_FLAGS:-} tests/test_rs97_scene.cpp src/gpu/gpulib/gpu.cpp src/gpu/gpulib/vout_port.cpp src/plugin_lib/pl_sshot.cpp \
  -Wl,--gc-sections -o "$out"
if [ -n "${SCENE_TEST_RUNNER:-}" ]; then
  $SCENE_TEST_RUNNER "$out"
else
  "$out"
fi
