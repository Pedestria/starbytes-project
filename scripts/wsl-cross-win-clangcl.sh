#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-wsl-cross-clangcl}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
GENERATOR="${GENERATOR:-Ninja}"
JOBS="${JOBS:-$(nproc)}"

CONFIGURE_ONLY=0
BUILD_ONLY=0
BUILD_TARGETS=()
EXTRA_CMAKE_ARGS=()

usage() {
  cat <<'EOF'
Usage: scripts/wsl-cross-win-clangcl.sh [options] [-- <extra cmake args>]

Options:
  --build-dir <path>      Build directory (default: build-wsl-cross-clangcl)
  --build-type <type>     CMAKE_BUILD_TYPE (default: Release)
  --target <name>         Build target (repeatable)
  --configure-only        Only run CMake configure/generate
  --build-only            Only run CMake build
  -h, --help              Show this help

Environment overrides:
  VS_BASE                 Visual Studio root (default: /mnt/c/Program Files/Microsoft Visual Studio/2022/Community)
  SDK_BASE                Windows Kits root (default: /mnt/c/Program Files (x86)/Windows Kits/10)
  VC_VER                  Specific MSVC toolset version
  SDK_VER                 Specific Windows SDK version
  JOBS                    Parallel build jobs (default: nproc)
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --build-type)
      BUILD_TYPE="$2"
      shift 2
      ;;
    --target)
      BUILD_TARGETS+=("$2")
      shift 2
      ;;
    --configure-only)
      CONFIGURE_ONLY=1
      shift
      ;;
    --build-only)
      BUILD_ONLY=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      EXTRA_CMAKE_ARGS+=("$@")
      break
      ;;
    *)
      EXTRA_CMAKE_ARGS+=("$1")
      shift
      ;;
  esac
done

if [[ "$CONFIGURE_ONLY" -eq 1 && "$BUILD_ONLY" -eq 1 ]]; then
  echo "error: --configure-only and --build-only are mutually exclusive" >&2
  exit 1
fi

for tool in cmake ninja clang-cl lld-link llvm-rc llvm-mt; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "error: required tool not found on PATH: $tool" >&2
    exit 1
  fi
done

VS_BASE="${VS_BASE:-/mnt/c/Program Files/Microsoft Visual Studio/2022/Community}"
SDK_BASE="${SDK_BASE:-/mnt/c/Program Files (x86)/Windows Kits/10}"
VC_TOOLS_DIR="$VS_BASE/VC/Tools/MSVC"
SDK_LIB_DIR="$SDK_BASE/Lib"

if [[ ! -d "$VC_TOOLS_DIR" ]]; then
  echo "error: MSVC tools directory not found: $VC_TOOLS_DIR" >&2
  exit 1
fi
if [[ ! -d "$SDK_LIB_DIR" ]]; then
  echo "error: Windows SDK lib directory not found: $SDK_LIB_DIR" >&2
  exit 1
fi

VC_VER="${VC_VER:-$(ls -1 "$VC_TOOLS_DIR" | sort -V | tail -n 1)}"
SDK_VER="${SDK_VER:-$(ls -1 "$SDK_LIB_DIR" | sort -V | tail -n 1)}"
VC_ROOT="$VC_TOOLS_DIR/$VC_VER"

LIB_PATHS=(
  "$VC_ROOT/lib/x64"
  "$SDK_BASE/Lib/$SDK_VER/ucrt/x64"
  "$SDK_BASE/Lib/$SDK_VER/um/x64"
)

INCLUDE_PATHS=(
  "$VC_ROOT/include"
  "$SDK_BASE/Include/$SDK_VER/ucrt"
  "$SDK_BASE/Include/$SDK_VER/shared"
  "$SDK_BASE/Include/$SDK_VER/um"
  "$SDK_BASE/Include/$SDK_VER/winrt"
  "$SDK_BASE/Include/$SDK_VER/cppwinrt"
)

for path in "${LIB_PATHS[@]}" "${INCLUDE_PATHS[@]}"; do
  if [[ ! -d "$path" ]]; then
    echo "error: required path not found: $path" >&2
    exit 1
  fi
done

LIB_VALUE="$(IFS=';'; echo "${LIB_PATHS[*]}")"
INCLUDE_VALUE="$(IFS=';'; echo "${INCLUDE_PATHS[*]}")"
export LIB="$LIB_VALUE"
export INCLUDE="$INCLUDE_VALUE"

echo "info: cross-configure using MSVC $VC_VER and SDK $SDK_VER"
echo "info: build dir: $BUILD_DIR"

if [[ "$BUILD_ONLY" -eq 0 ]]; then
  configure_cmd=(
    cmake
    -S "$ROOT_DIR"
    -B "$BUILD_DIR"
    -G "$GENERATOR"
    -DCMAKE_SYSTEM_NAME=Windows
    "-DCMAKE_SYSTEM_VERSION=$SDK_VER"
    "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
    -DCMAKE_C_COMPILER=clang-cl
    -DCMAKE_CXX_COMPILER=clang-cl
    -DCMAKE_RC_COMPILER=llvm-rc
    -DCMAKE_MT=llvm-mt
  )
  if [[ ${#EXTRA_CMAKE_ARGS[@]} -gt 0 ]]; then
    configure_cmd+=("${EXTRA_CMAKE_ARGS[@]}")
  fi
  "${configure_cmd[@]}"
fi

if [[ "$CONFIGURE_ONLY" -eq 0 ]]; then
  build_cmd=(cmake --build "$BUILD_DIR" -j "$JOBS")
  if [[ ${#BUILD_TARGETS[@]} -gt 0 ]]; then
    build_cmd+=(--target "${BUILD_TARGETS[@]}")
  fi
  "${build_cmd[@]}"
fi

