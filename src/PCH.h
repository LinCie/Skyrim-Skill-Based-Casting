#pragma once

#include <SKSE/Impl/PCH.h>

// CommonLib's generated SKSEPluginInfo source uses "..."sv at global scope,
// while its precompiled header imports these utilities inside namespace SKSE.
using namespace std::literals;
namespace stl = SKSE::stl;
