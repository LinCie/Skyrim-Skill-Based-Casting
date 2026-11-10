# Third-party notices

## CommonLibSSE-NG

- Project: [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG)
- Version: 16.0.1, source commit `dddc6ec670381ab6af883b32dcb67a2589878acd`.
- License: GPL-3.0-or-later, with the upstream Modding Exception and GPL-3.0 Linking Exception. The license text is in `LICENSE`; the exception text copied from the pinned upstream revision is in `licenses/CommonLibSSE-NG-EXCEPTIONS.md`.
- CommonLibSSE-NG's GPL-3.0 Linking Exception permits combining the library with modding libraries and requires corresponding source for covered work and the portions of modding libraries used when conveying a non-source combination. The project source, pinned vcpkg manifest/baseline, and this notice should accompany or be made available with DLL redistribution. The two-file player ZIP is not a substitute for the corresponding source distribution.

## vcpkg build dependencies

The listed ports are selected from vcpkg baseline `ee12231b20c95013c6638d845d04c91559a1d1ff`, as constrained by `vcpkg.json` and CommonLibSSE-NG v16.0.1's manifest. Their upstream/port metadata identifies these licenses:

| Package | Resolved version | License | Upstream |
| --- | --- | --- | --- |
| DirectXMath | 2025-04-03 | MIT | https://github.com/microsoft/DirectXMath |
| DirectXTK (Ubuntu overlay port) | 2026-05-07#2 | MIT | https://github.com/microsoft/DirectXTK |
| fmt | 12.1.0 | MIT | https://github.com/fmtlib/fmt |
| nlohmann-json | 3.12.0#1 | MIT | https://github.com/nlohmann/json |
| rapidcsv | 8.90 | BSD-3-Clause | https://github.com/d99kris/rapidcsv |
| SimpleIni | 4.25 | MIT | https://github.com/brofield/simpleini |
| spdlog | 1.16.0 | MIT | https://github.com/gabime/spdlog |
| toml11 | 4.4.0 | MIT | https://github.com/ToruNiina/toml11 |
| Xbyak | 7.28 | BSD-3-Clause | https://github.com/herumi/xbyak |
| vcpkg-cmake-config (host build helper) | 2024-05-23 | MIT | https://github.com/microsoft/vcpkg |
| fxc2 (build-only HLSL compiler stand-in) | 1.0.0 | MPL-2.0 | https://github.com/WasabiIceCream/fxc2 |

These are build-time dependencies only; the vcpkg target libraries are statically linked and are not separate player dependencies. The pinned CommonLib Linux cross-build example supplies an Ubuntu overlay port for DirectXTK because its shader build normally expects the Windows SDK `fxc.exe`; that overlay uses `fxc2` under Wine. Neither fxc2 nor its helper DLL is included in the mod ZIP. The Windows SDK/MSVC CRT sysroot obtained with `xwin` is used under Microsoft's SDK/CRT terms and is not redistributed. Consult each pinned source/port for full license text and notices.

## Inspiration

SkillBasedCasting independently implements a mechanic inspired by the published description of [Spell Cast Time Progression by Elzar125](https://www.nexusmods.com/skyrimspecialedition/mods/163122). No original source code or binary was used. This project is not an official continuation or endorsement.
