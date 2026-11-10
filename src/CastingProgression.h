#pragma once

#include <optional>

namespace skill_based_casting::progression
{
	struct Settings
	{
		double minimumSpeed{ 0.5 };
		double speedPerLevel{ 0.01 };
		int maximumSkillLevel{ 100 };
	};

	// Returns no value for invalid settings, non-finite input, or an invalid speed factor.
	[[nodiscard]] std::optional<double> SpeedFactor(double skillLevel, const Settings& settings) noexcept;

	// Computes vanillaChargeTime / SpeedFactor(...), returning no value rather than an unsafe time.
	[[nodiscard]] std::optional<double> AdjustedChargeTime(
		double vanillaChargeTime,
		double skillLevel,
		const Settings& settings) noexcept;
}
