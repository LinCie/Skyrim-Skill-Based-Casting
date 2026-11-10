#include "CastingProgression.h"
#include "Config.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>

namespace
{
	int g_failures = 0;

	void Check(const bool condition, const std::string_view name)
	{
		if (condition) {
			std::cout << "[PASS] " << name << '\n';
		} else {
			std::cerr << "[FAIL] " << name << '\n';
			++g_failures;
		}
	}

	void CheckNear(const double actual, const double expected, const double epsilon, const std::string_view name)
	{
		Check(std::isfinite(actual) && std::abs(actual - expected) <= epsilon, name);
	}

	void CheckOptionalNear(const std::optional<double>& actual, const double expected, const double epsilon, const std::string_view name)
	{
		if (!actual) {
			Check(false, name);
			return;
		}
		CheckNear(*actual, expected, epsilon, name);
	}

	void TestDefaultProgression()
	{
		using namespace skill_based_casting::progression;
		const Settings settings{};

		CheckOptionalNear(SpeedFactor(15.0, settings), 0.65, 1e-12, "skill 15 is 0.65x speed");
		CheckOptionalNear(SpeedFactor(50.0, settings), 1.0, 1e-12, "skill 50 is 1.0x speed");
		CheckOptionalNear(SpeedFactor(100.0, settings), 1.5, 1e-12, "skill 100 is 1.5x speed");

		CheckOptionalNear(AdjustedChargeTime(10.0, 15.0, settings), 10.0 / 0.65, 1e-12,
			"charge time uses vanilla time divided by speed factor");
		CheckOptionalNear(AdjustedChargeTime(10.0, 50.0, settings), 10.0, 1e-12,
			"skill 50 preserves vanilla charge time");
		CheckOptionalNear(AdjustedChargeTime(10.0, 100.0, settings), 10.0 / 1.5, 1e-12,
			"skill 100 reduces charge time");
	}

	void TestSkillClamping()
	{
		using namespace skill_based_casting::progression;
		const Settings settings{};
		CheckOptionalNear(SpeedFactor(150.0, settings), 1.5, 1e-12, "skill above maximum is capped");
		CheckOptionalNear(SpeedFactor(-20.0, settings), 0.5, 1e-12, "negative skill is clamped to zero");
	}

	void TestInvalidInputs()
	{
		using namespace skill_based_casting::progression;
		const Settings defaults{};
		const auto nan = std::numeric_limits<double>::quiet_NaN();
		const auto infinity = std::numeric_limits<double>::infinity();

		Check(!SpeedFactor(nan, defaults), "NaN skill is rejected");
		Check(!SpeedFactor(infinity, defaults), "infinite skill is rejected");
		Check(!SpeedFactor(50.0, Settings{ 0.0, 0.01, 100 }), "zero minimum speed is rejected");
		Check(!SpeedFactor(50.0, Settings{ -0.5, 0.01, 100 }), "negative minimum speed is rejected");
		Check(!SpeedFactor(50.0, Settings{ 0.5, -0.01, 100 }), "negative progression is rejected");
		Check(!SpeedFactor(50.0, Settings{ 0.5, infinity, 100 }), "infinite progression is rejected");
		Check(!SpeedFactor(50.0, Settings{ 0.5, 0.01, 0 }), "non-positive maximum skill is rejected");
		Check(!SpeedFactor(50.0, Settings{ 1.0, std::numeric_limits<double>::max(), 100 }), "overflowing speed factor is rejected");
		Check(!AdjustedChargeTime(0.0, 50.0, defaults), "zero vanilla charge time is unchanged/rejected");
		Check(!AdjustedChargeTime(-1.0, 50.0, defaults), "negative vanilla charge time is rejected");
		Check(!AdjustedChargeTime(infinity, 50.0, defaults), "infinite vanilla charge time is rejected");
		Check(!AdjustedChargeTime(10.0, 50.0, Settings{ 0.0, 0.0, 100 }), "division by zero cannot occur");
	}

	void TestMonotonicProgression()
	{
		using namespace skill_based_casting::progression;
		const Settings settings{};
		double previousTime = std::numeric_limits<double>::infinity();
		bool monotonic = true;
		for (int skill = 0; skill <= settings.maximumSkillLevel; ++skill) {
			const auto chargeTime = AdjustedChargeTime(5.0, static_cast<double>(skill), settings);
			if (!chargeTime || *chargeTime > previousTime) {
				monotonic = false;
				break;
			}
			previousTime = *chargeTime;
		}
		Check(monotonic, "higher valid skill never increases charge time");
	}

	void TestConfiguration()
	{
		using namespace skill_based_casting::config;
		std::istringstream validIni(
			"[General]\n"
			"bEnableNPCSupport = false\n"
			"[Progression]\n"
			"fMinChargeTimeMultiplier = 0.6 ; comment\n"
			"fChargeTimeMultiplier = 0.02\n"
			"iMaxSkillLevel = 80\n"
			"[Debug]\n"
			"bEnableDebugLogging = true\n");
		const auto parsed = Parse(validIni);
		Check(parsed.loadedFromFile, "parsed INI is marked loaded");
		Check(!parsed.settings.enableNPCSupport, "NPC support INI value parsed");
		CheckNear(parsed.settings.progression.minimumSpeed, 0.6, 1e-12, "minimum speed INI value parsed");
		CheckNear(parsed.settings.progression.speedPerLevel, 0.02, 1e-12, "speed per level INI value parsed");
		Check(parsed.settings.progression.maximumSkillLevel == 80, "maximum skill INI value parsed");
		Check(parsed.settings.enableDebugLogging, "debug logging INI value parsed");

		std::istringstream invalidIni(
			"[Progression]\n"
			"fMinChargeTimeMultiplier = 0\n"
			"fChargeTimeMultiplier = -1\n"
			"iMaxSkillLevel = 0\n"
			"[Debug]\n"
			"bEnableDebugLogging = maybe\n");
		const auto invalid = Parse(invalidIni);
		Check(invalid.warnings.size() == 4, "invalid INI values are reported");
		CheckNear(invalid.settings.progression.minimumSpeed, 0.5, 1e-12, "invalid minimum speed retains default");
		CheckNear(invalid.settings.progression.speedPerLevel, 0.01, 1e-12, "invalid progression retains default");
		Check(invalid.settings.progression.maximumSkillLevel == 100, "invalid max skill retains default");
		Check(!invalid.settings.enableDebugLogging, "invalid boolean retains default");

		std::istringstream overflowingIni(
			"[Progression]\n"
			"fMinChargeTimeMultiplier = 1e308\n"
			"fChargeTimeMultiplier = 1e308\n");
		const auto overflowing = Parse(overflowingIni);
		Check(!overflowing.warnings.empty(), "overflowing INI progression is reported");
		CheckNear(overflowing.settings.progression.minimumSpeed, 0.5, 1e-12, "overflowing INI resets minimum speed");
		CheckNear(overflowing.settings.progression.speedPerLevel, 0.01, 1e-12, "overflowing INI resets progression");

		std::istringstream malformedIni("[Progression\nnot-a-setting\n");
		const auto malformed = Parse(malformedIni);
		Check(malformed.warnings.size() == 2, "malformed INI lines are ignored with warnings");

		const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
		const auto missingPath = std::filesystem::temp_directory_path() /
			("SkillBasedCasting-missing-" + std::to_string(unique) + ".ini");
		std::error_code error;
		std::filesystem::remove(missingPath, error);
		const auto missing = Load(missingPath);
		Check(!missing.loadedFromFile, "missing INI uses defaults");
		Check(missing.settings.enableNPCSupport, "missing INI default enables NPC support");
		CheckNear(missing.settings.progression.minimumSpeed, 0.5, 1e-12, "missing INI default minimum speed");
		CheckNear(missing.settings.progression.speedPerLevel, 0.01, 1e-12, "missing INI default speed per level");
		Check(missing.settings.progression.maximumSkillLevel == 100, "missing INI default max skill");
		Check(!missing.settings.enableDebugLogging, "missing INI default disables debug logging");
	}
}

int main()
{
	TestDefaultProgression();
	TestSkillClamping();
	TestInvalidInputs();
	TestMonotonicProgression();
	TestConfiguration();

	if (g_failures != 0) {
		std::cerr << g_failures << " test(s) failed\n";
		return 1;
	}
	std::cout << "All progression and configuration tests passed\n";
	return 0;
}
