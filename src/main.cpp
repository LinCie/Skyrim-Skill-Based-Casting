#include "CastingHook.h"
#include "Config.h"

#include "REL/Module.h"
#include "SKSE/SKSE.h"

#include <filesystem>
#include <exception>
#include <utility>

namespace
{
	skill_based_casting::config::Settings g_settings{};
	bool g_hookInstallationAttempted = false;

	[[nodiscard]] std::filesystem::path GetConfigPath()
	{
		const auto executablePath = std::filesystem::path(REL::Module::get().filePath());
		return executablePath.parent_path() / "Data" / "SKSE" / "Plugins" / "SkillBasedCasting.ini";
	}

	void OnMessage(SKSE::MessagingInterface::Message* message)
	{
		if (!message || message->type != SKSE::MessagingInterface::kPostPostLoad || g_hookInstallationAttempted) {
			return;
		}
		g_hookInstallationAttempted = true;

		try {
			if (skill_based_casting::casting_hook::Install(g_settings)) {
				SKSE::log::info("Casting hook installed");
			} else {
				SKSE::log::error("Casting hook was not installed; vanilla spell charging remains unchanged");
			}
		} catch (const std::exception& error) {
			SKSE::log::error("Casting hook setup failed safely: {}", error.what());
		} catch (...) {
			SKSE::log::error("Casting hook setup failed safely with an unknown error");
		}
	}
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* skse)
{
	SKSE::Init(skse, { .logName = "SkillBasedCasting" });
	SKSE::log::info("Plugin initializing");

	const auto runtime = REL::Module::get().version();
	SKSE::log::info("Runtime: Skyrim {}", runtime.string("."));
	if (runtime != SKSE::RUNTIME_SSE_1_7_104) {
		SKSE::log::error("Unsupported runtime {}; this build supports only Skyrim 1.7.104.0", runtime.string("."));
		return false;
	}

	try {
		auto loaded = skill_based_casting::config::Load(GetConfigPath());
		g_settings = std::move(loaded.settings);
		if (loaded.loadedFromFile) {
			SKSE::log::info("Configuration loaded");
		} else {
			SKSE::log::info("Configuration INI missing or unreadable; using defaults");
		}
		for (const auto& warning : loaded.warnings) {
			SKSE::log::warn("Configuration: {}", warning);
		}
	} catch (const std::exception& error) {
		SKSE::log::error("Configuration loading failed safely; using defaults: {}", error.what());
		g_settings = {};
	}

	auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(OnMessage)) {
		SKSE::log::error("Could not register the SKSE post-post-load listener; casting hook will not be installed");
		return false;
	}
	return true;
}
