#pragma once

#include "Config.h"

namespace skill_based_casting::casting_hook
{
	// Installs the runtime-only hook. A failed prerequisite leaves vanilla casting unchanged.
	[[nodiscard]] bool Install(const config::Settings& settings);
}
