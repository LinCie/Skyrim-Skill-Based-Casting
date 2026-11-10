#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: ./scripts/build.sh [Debug|Release] [--deploy-directory PATH]

Cross-compiles the Windows x64 SKSE DLL from Ubuntu, runs the native Ubuntu
progression/config tests, and optionally copies the DLL and INI to a mod-staging
folder. It never writes to a Skyrim installation directory.
USAGE
}

configuration=Release
deploy_directory=""
configuration_set=false
while (($#)); do
    case "$1" in
        Debug|Release)
            if [[ "$configuration_set" == true ]]; then
                echo "Only one build configuration may be specified." >&2
                usage >&2
                exit 2
            fi
            configuration="$1"
            configuration_set=true
            shift
            ;;
        --deploy-directory)
            if (($# < 2)) || [[ -z "$2" ]]; then
                echo "--deploy-directory requires a path." >&2
                usage >&2
                exit 2
            fi
            deploy_directory="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ "$(uname -s)" != Linux || ! -r /etc/os-release ]]; then
    echo "This project must be built on Ubuntu Linux; the plugin target is a Windows x64 DLL." >&2
    exit 1
fi
# shellcheck disable=SC1091
. /etc/os-release
if [[ "${ID:-}" != ubuntu ]]; then
    echo "This project build is supported on Ubuntu only (detected: ${PRETTY_NAME:-unknown})." >&2
    exit 1
fi

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/.local/share/vcpkg}"
XWIN_SYSROOT="${XWIN_SYSROOT:-$HOME/xwin-out}"
LLVM_MINGW_BIN="${LLVM_MINGW_BIN:-/opt/llvm-mingw/bin}"
export VCPKG_ROOT XWIN_SYSROOT LLVM_MINGW_BIN
export PATH="$HOME/.local/bin:$LLVM_MINGW_BIN:$HOME/.cargo/bin:$PATH"

for tool in cmake ninja clang-cl lld-link llvm-rc llvm-mt xwin wine python3 file; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Required Ubuntu build tool '$tool' was not found. See README.md for setup." >&2
        exit 1
    fi
done
if [[ ! -x "$VCPKG_ROOT/vcpkg" || ! -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]]; then
    echo "vcpkg is not bootstrapped at '$VCPKG_ROOT'. Set VCPKG_ROOT and run bootstrap-vcpkg.sh." >&2
    exit 1
fi
if [[ ! -d "$XWIN_SYSROOT/VC/Tools/MSVC" || ! -d "$XWIN_SYSROOT/Windows Kits/10/Include" ]]; then
    echo "The xwin Windows SDK/MSVC sysroot is missing at '$XWIN_SYSROOT'. Follow the README setup instructions." >&2
    exit 1
fi
if [[ ! -x "$LLVM_MINGW_BIN/x86_64-w64-mingw32-clang++" ]]; then
    echo "llvm-mingw was not found at '$LLVM_MINGW_BIN'. Set LLVM_MINGW_BIN to its bin directory." >&2
    exit 1
fi

build_preset="ubuntu-${configuration,,}"
build_directory="$project_root/build/$build_preset"
dll_path="$build_directory/$configuration/SkillBasedCasting.dll"

printf 'Configuring Ubuntu-hosted Windows x64 %s build...\n' "$configuration"
cmake --preset "$build_preset"
cmake --build --preset "$build_preset" --parallel

# Unit tests are host-native: do not reuse the cross compiler or Windows vcpkg toolchain.
unset CC CXX
cmake -S "$project_root" -B "$project_root/build/ubuntu-tests" -G Ninja \
    -DSBC_BUILD_PLUGIN=OFF -DSBC_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build "$project_root/build/ubuntu-tests" --parallel
ctest --test-dir "$project_root/build/ubuntu-tests" --output-on-failure

if [[ ! -f "$dll_path" ]]; then
    echo "Cross-build completed but DLL is missing: $dll_path" >&2
    exit 1
fi
if ! file "$dll_path" | grep -Eq 'PE32[+].*x86-64'; then
    echo "Expected a Windows x64 PE32+ DLL, but file inspection did not identify one: $dll_path" >&2
    file "$dll_path" >&2 || true
    exit 1
fi
printf 'Windows plugin built: %s\n' "$dll_path"

if [[ -n "$deploy_directory" ]]; then
    plugin_directory="$deploy_directory/SKSE/Plugins"
    mkdir -p "$plugin_directory"
    cp "$dll_path" "$plugin_directory/SkillBasedCasting.dll"
    cp "$project_root/config/SkillBasedCasting.ini" "$plugin_directory/SkillBasedCasting.ini"
    printf 'Staged DLL and INI under: %s\n' "$plugin_directory"
fi
