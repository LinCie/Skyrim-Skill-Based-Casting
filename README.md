# SkillBasedCasting

A small, runtime-only SKSE plugin that shortens charge times for supported fire-and-forget spells as the caster improves the spell's associated magic-school skill. It is an independent implementation inspired by the published description of [Spell Cast Time Progression by Elzar125](https://www.nexusmods.com/skyrimspecialedition/mods/163122); it is not an official continuation and does not use the original mod's unavailable source or binaries.

## Compatibility and status

- **Build host:** Ubuntu x86_64 only. The project is not intended to be built on Windows.
- **Artifact target:** Windows x64 PE DLL, because Skyrim/SKSE loads Windows plugins (including under Proton); this is cross-compiled on Ubuntu with `clang-cl` using the MSVC ABI.
- Target runtime: Skyrim Special Edition / Anniversary Edition **Steam 1.7.104.0**.
- Minimum SKSE64: **2.3.1**; requires Address Library for SKSE Plugins **v13** (v5 `versionlib-1-7-104-0.bin`).
- Skyrim 1.7.99, VR, GOG, and other runtimes are not enabled or claimed compatible.
- No ESP/ESL/ESM, Papyrus, MCM, or separate CommonLib installation is required by players.
- The hook's CommonLib declaration and vtable relocation are verified against the selected library source, but the Skyrim executable implementation, invocation frequency, and dual-cast timing have not been verified in-game. Treat the hook as an **unvalidated candidate** until the checklist below is completed. Unit tests or a successful cross-build do not prove runtime compatibility.

On Ubuntu 24.04.5, both `./scripts/build.sh Release` and `./scripts/build.sh Debug` completed; the Release ZIP was created by `./scripts/package.sh Release`. The native progression/config test target passed (42 checks). This confirms build and packaging only—not in-game behavior.

The plugin refuses unsupported runtimes and checks the Address Library file's presence, v5 format/version, pointer size, offset count, and minimum file length before attempting relocation. A structurally valid but internally incompatible/corrupt database is not fully validated. If a checked prerequisite or guarded vtable write fails, the plugin logs the reason and leaves the game's casting behavior unchanged.

## How progression works

```text
speedFactor = fMinChargeTimeMultiplier
            + clamp(skillLevel, 0, iMaxSkillLevel) * fChargeTimeMultiplier
actualChargeTime = vanillaChargeTime / speedFactor
```

The `fMinChargeTimeMultiplier` and `fChargeTimeMultiplier` names are retained for settings migration, but contribute to a **speed factor**, not a direct duration multiplier. Defaults give 0.65x speed at skill 15, 1.0x at 50, and 1.5x at 100. Higher skill does not produce slower casting for valid positive progression values. Invalid settings and non-finite values are rejected safely.

The hook calls Skyrim's original timer setter first and adjusts its resulting timer, retaining spell-record charge time and any changes reflected in the engine-established timer. It uses `MagicItem::GetAssociatedSkill()` to resolve the school's ActorValue, including custom spells where the engine provides a recognized school. Only `MagicCaster` states `kUnk02` and `kCharging` are adjusted; other states fall back to vanilla. CommonLib labels `kUnk02` as a possible start-charge state, but the actual runtime transition order still needs verification.

The implementation supports player and optionally NPC `SpellItem` spells with `FireAndForget` casting type, positive charge time, recognized school, and left/right hand source. Concentration, instant, scroll, staff, unsupported casting sources, unknown schools, and invalid states retain vanilla timing. Dual casting is intentionally not claimed as supported: the callback checks both caster and actor dual-casting flags and skips reported dual casts, but the timing/flag lifecycle still needs in-game verification.

## Configuration

Install or edit `Data/SKSE/Plugins/SkillBasedCasting.ini`:

```ini
[General]
bEnableNPCSupport = true

[Progression]
fMinChargeTimeMultiplier = 0.5
fChargeTimeMultiplier = 0.01
iMaxSkillLevel = 100

[Debug]
bEnableDebugLogging = false
```

The shipped INI is `config/SkillBasedCasting.ini`. Missing/unreadable INI uses defaults. Invalid known values are rejected individually with a startup warning; progression values that would overflow reset to defaults. Configuration loads once at startup; there is no hot reload or MCM.

## Player installation and removal

1. Install SKSE64 2.3.1 or newer that supports runtime 1.7.104.0, and Address Library for SKSE Plugins v13.
2. Install `dist/SkillBasedCasting.zip` with MO2/Vortex, or extract its `SKSE/Plugins/SkillBasedCasting.dll` and `SKSE/Plugins/SkillBasedCasting.ini` under Skyrim's `Data` directory.
3. Start through SKSE and check its plugin log for initialization/hook errors.
4. Remove/disable the DLL and INI between game sessions. There is no save serialization, record/form modification, persistent actor edit, Papyrus, quest, alias, or script. Runtime changes disappear when the process exits. Do not unload the DLL while Skyrim is running.

Build and deployment scripts never write to the real game directory. An optional staging path copies only the DLL and INI into a mod-manager folder.

## Ubuntu build requirements

- Ubuntu x86_64.
- CMake 3.25+, Ninja, Git, Python 3, `file`, Clang/LLVM tools (`clang-cl`, `lld-link`, `llvm-rc`, `llvm-mt`), Rust/Cargo, Wine, and llvm-mingw.
- `xwin` 0.10.0 to obtain the Windows SDK/MSVC CRT sysroot.
- Bootstrapped vcpkg; its target dependencies are resolved in manifest mode.

This workflow follows CommonLibSSE-NG v16.0.1's `examples/linux-cross-compile` pipeline: `clang-cl` + `lld-link`, an xwin Windows sysroot, and a vcpkg clang-cl triplet. DirectXTK invokes the Windows HLSL compiler during its build; the included vcpkg overlay uses the build-only `fxc2` stand-in under Wine. `fxc2` and its helper DLL are build tools and are not packaged with the mod.

### One-time tool setup

Install Ubuntu packages (package availability depends on the Ubuntu release):

```bash
sudo apt-get update
sudo apt-get install -y clang lld llvm cmake ninja-build git curl python3 file wine rustup
rustup default stable
```

Install llvm-mingw using its Ubuntu x86_64 release archive (the same toolchain layout used by the pinned CommonLib cross-build example):

```bash
LLVM_MINGW_VERSION=20260826
curl -fL "https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW_VERSION}/llvm-mingw-${LLVM_MINGW_VERSION}-ucrt-ubuntu-22.04-x86_64.tar.xz" -o /tmp/llvm-mingw.tar.xz
sudo mkdir -p /opt/llvm-mingw
sudo tar -xf /tmp/llvm-mingw.tar.xz -C /opt/llvm-mingw --strip-components=1
export LLVM_MINGW_BIN=/opt/llvm-mingw/bin
```

Install xwin and stage the Windows SDK/CRT. The two sysroot layout flags are required by the CMake toolchain:

```bash
cargo install xwin --version 0.10.0 --locked
export PATH="$HOME/.cargo/bin:$PATH"
mkdir -p "$HOME/xwin-cache" "$HOME/xwin-out"
xwin --accept-license --http-retry 10 --cache-dir "$HOME/xwin-cache" splat \
  --output "$HOME/xwin-out" --include-debug-libs \
  --use-winsysroot-style --preserve-ms-arch-notation
export XWIN_SYSROOT="$HOME/xwin-out"
```

Bootstrap vcpkg and export the environment used by the presets:

```bash
git clone https://github.com/microsoft/vcpkg "$HOME/.local/share/vcpkg"
"$HOME/.local/share/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
export PATH="$HOME/.local/bin:$LLVM_MINGW_BIN:$HOME/.cargo/bin:$PATH"
```

Set `VCPKG_ROOT`, `LLVM_MINGW_BIN`, `XWIN_SYSROOT`, and the `PATH` in each new shell (or add them to your shell profile). The paths above are examples for build tools only; no Skyrim installation path is hardcoded.

### Build, test, and package

From the project root, one command cross-builds the Windows DLL on Ubuntu, builds/runs native Ubuntu progression/config tests, and optionally stages the two install files:

```bash
./scripts/build.sh Release
./scripts/build.sh Debug
./scripts/build.sh Release --deploy-directory "$HOME/mod-staging/SkillBasedCasting"
```

Create the mod-manager-ready ZIP after a successful Release build:

```bash
./scripts/package.sh Release
```

The ZIP is `dist/SkillBasedCasting.zip` and contains only:

```text
SKSE/Plugins/SkillBasedCasting.dll
SKSE/Plugins/SkillBasedCasting.ini
```

The corresponding output DLL paths are `build/ubuntu-release/Release/SkillBasedCasting.dll` and `build/ubuntu-debug/Debug/SkillBasedCasting.dll`. Confirm the result is Windows PE, not a Linux shared object:

```bash
file build/ubuntu-release/Release/SkillBasedCasting.dll
# expect: PE32+ executable (DLL) ... x86-64, for MS Windows
```

The scripts also run native tests from `build/ubuntu-tests`; they do not run a Windows test executable under Wine. Direct CMake commands are:

```bash
cmake --preset ubuntu-release
cmake --build --preset ubuntu-release --parallel
cmake -S . -B build/ubuntu-tests -G Ninja -DSBC_BUILD_PLUGIN=OFF -DSBC_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/ubuntu-tests --parallel
ctest --test-dir build/ubuntu-tests --output-on-failure
```

CMake presets `ubuntu-debug` and `ubuntu-release` use the Windows x64/MSVC ABI target from the Ubuntu host. There is intentionally no Windows-host preset or PowerShell build script.

## Tests

`tests/CastingProgressionTests.cpp` covers default skill 15/50/100 factors and charge times, above/below-range clamping, invalid/non-finite inputs, division-by-zero prevention, monotonicity, valid/invalid/malformed INI, overflow handling, and defaults when the INI is missing. These host-native tests do not prove the in-game hook is correct.

## Manual in-game verification checklist

Use a disposable profile/save and record baseline behavior. Confirm the plugin log reports runtime 1.7.104 and successful hook installation. Compare identical spells and conditions over several casts.

- [ ] Destruction charge spell at low, medium, and high skill (e.g. 15, 50, 100).
- [ ] Repeat with charge-based spells from Destruction, Restoration, Conjuration, Illusion, and Alteration.
- [ ] Test left-hand and right-hand casting.
- [ ] Test dual casting; check timer, cancel, release, and animation behavior. Flagged dual casts are intentionally skipped.
- [ ] Test NPC support with `bEnableNPCSupport = true`, then `false` after a full restart.
- [ ] Confirm concentration and instant spells are unchanged.
- [ ] Test repeated casts, interruptions, cancellations, magicka use, release, and animations.
- [ ] Test a custom spell mod and verify its engine-resolved school is supported.
- [ ] Save/load with the plugin enabled; verify no spell records or actor stats change.
- [ ] Remove the mod between sessions and reopen a copy of the save; confirm no persistent effects remain.

## Engineering notes and limitations

- Hook candidate: `RE::ActorMagicCaster::SetCastingTimerForCharge()`, CommonLibSSE-NG v16.0.1 virtual slot `0x14`. `RE::VTABLE_ActorMagicCaster` is resolved using Address Library; there is no hardcoded executable address. Before replacing the slot, the plugin validates the vtable and target memory segments and compares the expected original function pointer. It calls the original method first.
- The selected CommonLib source declares the method and layout but does not contain the 1.7.104 game implementation/call sites. Call frequency, exact timer initialization semantics, release/interruption behavior, and dual-cast lifecycle still require validation against the actual game executable and in-game tests. If the virtual is called repeatedly or at an unsuitable state, the hook must be redesigned. Do not distribute as verified until the checklist passes.
- Only runtime 1.7.104.0 is accepted. No other runtime is claimed compatible.
- No custom save serialization, FormID/record changes, persistent actor changes, Papyrus, scripts, quests, aliases, or plugin masters.

## Dependencies and licensing

- CommonLibSSE-NG **v16.0.1**, pinned commit `dddc6ec670381ab6af883b32dcb67a2589878acd`, C++23, GPL-3.0-or-later with included modding/linking exceptions. See `LICENSE` and `licenses/CommonLibSSE-NG-EXCEPTIONS.md`.
- vcpkg baseline `ee12231b20c95013c6638d845d04c91559a1d1ff`. The Ubuntu DirectXTK overlay pins DirectXTK `2026-05-07#2` and patches its shader build for the Linux host; other resolved package versions are listed in `THIRD-PARTY-NOTICES.md`.
- The DirectXTK build overlay uses `fxc2` v1.0.0 (MPL-2.0) and Wine only as a host-side shader compilation workaround. Neither is a mod runtime dependency or included in the ZIP.
- Project source is GPL-3.0-or-later. See `THIRD-PARTY-NOTICES.md`; keep the corresponding source and dependency manifest available when redistributing a built DLL.
