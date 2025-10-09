#!/usr/bin/env bash

set -eu

path=$(realpath raylib/)
cd raylib/src/

args=(
    CC="mold -run clang"
    DESTDIR="$path"
    GRAPHICS=GRAPHICS_API_OPENGL_43
    PLATFORM=PLATFORM_DESKTOP
    USE_EXTERNAL_GLFW=FALSE
)

make "${args[@]}"
sudo make "${args[@]}" install
