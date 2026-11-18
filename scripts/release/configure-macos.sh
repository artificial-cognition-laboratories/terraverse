#!/usr/bin/env bash
# Configures the engine for macOS on Apple silicon in .output/build: Ninja, clang,
# and every dependency static from vcpkg (arm64-osx), so the app carries no dylibs
# but Chromium's framework. CI runs this; so can anyone on a Mac with vcpkg.
#   scripts/release/configure-macos.sh [version]
# Needs VCPKG_ROOT (or the runners' VCPKG_INSTALLATION_ROOT). Uses sccache when it's on PATH.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
version="${1:-dev}"
vcpkg="${VCPKG_ROOT:-${VCPKG_INSTALLATION_ROOT:-}}"
[[ -n "$vcpkg" ]] || { echo "configure-macos: set VCPKG_ROOT to a vcpkg checkout" >&2; exit 1; }

launcher=()
if command -v sccache >/dev/null; then
	launcher=(-DCMAKE_C_COMPILER_LAUNCHER=sccache -DCMAKE_CXX_COMPILER_LAUNCHER=sccache)
fi

# OpenAL: the system framework, which the engine's <OpenAL/al.h> includes on Apple (vcpkg's
# static openal-soft would need its own fmt and frameworks).
# 13.0: the oldest macOS this CEF supports (its CEF_TARGET_SDK).
cmake -S "$root" -B "$root/.output/build" -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_OSX_ARCHITECTURES=arm64 \
	-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
	"${launcher[@]}" \
	-DCMAKE_TOOLCHAIN_FILE="$vcpkg/scripts/buildsystems/vcpkg.cmake" \
	-DVCPKG_TARGET_TRIPLET=arm64-osx \
	-DRUN_IN_PLACE=TRUE \
	-DENABLE_CURSES=OFF -DENABLE_POSTGRESQL=OFF -DENABLE_LEVELDB=OFF \
	-DENABLE_REDIS=OFF -DENABLE_SPATIAL=OFF -DENABLE_PROMETHEUS=OFF \
	-DENABLE_UPDATE_CHECKER=OFF \
	-DENABLE_LTO=OFF \
	-DOPENAL_LIBRARY="$(xcrun --show-sdk-path)/System/Library/Frameworks/OpenAL.framework" \
	-DOPENAL_INCLUDE_DIR="$(xcrun --show-sdk-path)/System/Library/Frameworks/OpenAL.framework/Headers" \
	-DTERRAFIRMA_VERSION="$version"
