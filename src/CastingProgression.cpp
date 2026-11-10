#include "CastingProgression.h"

#include <algorithm>
#include <cmath>

namespace skill_based_casting::progression
{
	std::optional<double> SpeedFactor(const double skillLevel, const Settings& settings) noexcept
	{
		if (!std::isfinite(skillLevel) ||
			!std::isfinite(settings.minimumSpeed) || settings.minimumSpeed <= 0.0 ||
			!std::isfinite(settings.speedPerLevel) || settings.speedPerLevel < 0.0 ||
			settings.maximumSkillLevel <= 0) {
			return std::nullopt;
		}

		const auto clampedSkill = std::clamp(skillLevel, 0.0, static_cast<double>(settings.maximumSkillLevel));
		const auto speedFactor = settings.minimumSpeed + (clampedSkill * settings.speedPerLevel);
		if (!std::isfinite(speedFactor) || speedFactor <= 0.0) {
			return std::nullopt;
		}

		return speedFactor;
	}

	std::optional<double> AdjustedChargeTime(
		const double vanillaChargeTime,
		const double skillLevel,
		const Settings& settings) noexcept
	{
		if (!std::isfinite(vanillaChargeTime) || vanillaChargeTime <= 0.0) {
			return std::nullopt;
		}

		const auto speedFactor = SpeedFactor(skillLevel, settings);
		if (!speedFactor) {
			return std::nullopt;
		}

		const auto chargeTime = vanillaChargeTime / *speedFactor;
		if (!std::isfinite(chargeTime) || chargeTime <= 0.0) {
			return std::nullopt;
		}

		return chargeTime;
	}
}
