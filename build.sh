#!/usr/bin/env bash
# Linux / macOS 构建脚本（与 build.ps1 等价）。
# 依赖：g++(C++20)、GLFW3、系统 OpenGL；音频由内置 miniaudio 提供，无需额外库。
#   Debian/Ubuntu: sudo apt install g++ libglfw3-dev libgl1-mesa-dev
#   Arch:          sudo pacman -S gcc glfw mesa
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$root"

out="${1:-build/qlwt}"
mkdir -p "$(dirname "$out")"

mapfile -t src < <(find core game renderer gfx -name '*.cpp' | sort)
src+=(main.cpp)

echo "compiling ${#src[@]} sources -> $out"
g++ -std=c++20 -O2 -Wall -Wextra -I. "${src[@]}" -o "$out" \
    $(pkg-config --cflags --libs glfw3 gl) -lpthread -ldl -lm
echo "build OK -> $out"
