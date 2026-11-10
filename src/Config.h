#pragma once

#include "CastingProgression.h"

#include <filesystem>
#include <istream>
#include <string>
#include <vector>

namespace skill_based_casting::config
{
	struct Settings
	{
		bool enableNPCSupport{ true };
		progression::Settings progression{};
		bool enableDebugLogging{ false };
	};

	struct LoadResult
	{
		Settings settings{};
		bool loadedFromFile{ false };
		std::vector<std::string> warnings{};
	};

	[[nodiscard]] LoadResult Load(const std::filesystem::path& path);
	[[nodiscard]] LoadResult Parse(std::istream& input);
}
