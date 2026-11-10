#include "CastingHook.h"

#include "RE/A/Actor.h"
#include "RE/A/ActorMagicCaster.h"
#include "RE/F/FormTraits.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/S/SpellItem.h"
#include "REL/Module.h"
#include "REL/Relocation.h"
#include "SKSE/SKSE.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>

namespace skill_based_casting::casting_hook
{
	namespace
	{
		using SetCastingTimerFunction = void (*)(RE::ActorMagicCaster*);

		// CommonLibSSE-NG v16.0.1 documents this override as vtable slot 0x14.
		constexpr std::size_t kSetCastingTimerForChargeVtableIndex = 0x14;

		SetCastingTimerFunction g_originalSetCastingTimerForCharge = nullptr;
		config::Settings g_settings{};
		bool g_installed = false;

		[[nodiscard]] bool Contains(
			const REL::Segment& segment,
			const std::uintptr_t address,
			const std::size_t byteCount = 1) noexcept
		{
			const auto begin = segment.address();
			const auto length = segment.size();
			if (begin == 0 || address < begin || byteCount > length) {
				return false;
			}
			const auto offset = address - begin;
			return offset <= length - byteCount;
		}

		[[nodiscard]] bool IsExecutableAddress(const REL::Module& module, const std::uintptr_t address) noexcept
		{
			return Contains(module.segment(REL::Segment::Name::textx), address) ||
				Contains(module.segment(REL::Segment::Name::textw), address);
		}

		[[nodiscard]] bool IsMagicSchool(const RE::ActorValue skill) noexcept
		{
			switch (skill) {
			case RE::ActorValue::kAlteration:
			case RE::ActorValue::kConjuration:
			case RE::ActorValue::kDestruction:
			case RE::ActorValue::kIllusion:
			case RE::ActorValue::kRestoration:
				return true;
			default:
				return false;
			}
		}

		void SetCastingTimerForChargeHook(RE::ActorMagicCaster* caster)
		{
			if (!g_originalSetCastingTimerForCharge || !caster) {
				return;
			}

			// Let Skyrim establish its normal timer first; all conservative filters below
			// preserve that exact vanilla value when the spell/caster is unsupported.
			g_originalSetCastingTimerForCharge(caster);

			auto* actor = caster->actor;
			const auto state = caster->state.get();
			if (!actor || caster->GetIsDualCasting() || actor->IsDualCasting() ||
				(state != RE::MagicCaster::State::kUnk02 && state != RE::MagicCaster::State::kCharging)) {
				return;
			}

			const auto source = caster->castingSource;
			if (source != RE::MagicSystem::CastingSource::kLeftHand &&
				source != RE::MagicSystem::CastingSource::kRightHand) {
				return;
			}

			const auto* magicItem = caster->currentSpell;
			if (!magicItem) {
				return;
			}
			const auto* spell = magicItem->As<RE::SpellItem>();
			if (!spell ||
				spell->GetSpellType() != RE::MagicSystem::SpellType::kSpell ||
				spell->GetCastingType() != RE::MagicSystem::CastingType::kFireAndForget ||
				spell->data.flags.any(RE::SpellItem::SpellFlag::kInstantCast)) {
				return;
			}

			const auto recordChargeTime = spell->GetChargeTime();
			const auto vanillaTimer = caster->castingTimer;
			if (!std::isfinite(recordChargeTime) || recordChargeTime <= 0.0F ||
				!std::isfinite(vanillaTimer) || vanillaTimer <= 0.0F) {
				return;
			}

			const auto skill = spell->GetAssociatedSkill();
			if (!IsMagicSchool(skill)) {
				return;
			}

			if (!g_settings.enableNPCSupport && actor != RE::PlayerCharacter::GetSingleton()) {
				return;
			}

			const auto skillLevel = actor->GetActorValue(skill);
			const auto adjusted = progression::AdjustedChargeTime(
				static_cast<double>(vanillaTimer),
				static_cast<double>(skillLevel),
				g_settings.progression);
			if (!adjusted || *adjusted > static_cast<double>(std::numeric_limits<float>::max())) {
				return;
			}

			const auto adjustedTimer = static_cast<float>(*adjusted);
			if (!std::isfinite(adjustedTimer) || adjustedTimer <= 0.0F) {
				return;
			}

			caster->castingTimer = adjustedTimer;
			if (g_settings.enableDebugLogging) {
				const auto speedFactor = progression::SpeedFactor(
					static_cast<double>(skillLevel), g_settings.progression);
				if (speedFactor) {
					SKSE::log::debug(
						"Adjusted spell charge: actorValue={}, skill={:.2f}, speedFactor={:.3f}, vanilla={:.3f}, adjusted={:.3f}",
						static_cast<std::int32_t>(skill),
						static_cast<double>(skillLevel),
						*speedFactor,
						static_cast<double>(vanillaTimer),
						static_cast<double>(adjustedTimer));
				}
			}
		}

		[[nodiscard]] bool HasRequiredAddressLibraryFile(const REL::Version runtime)
		{
			// CommonLibSSE-NG resolves AE Address Library v5 data from this relative path.
			const auto fileName = std::string("versionlib-") + runtime.string() + ".bin";
			const std::filesystem::path relativePath = std::filesystem::path("Data") / "SKSE" / "Plugins" / fileName;
			std::error_code error;
			if (!std::filesystem::is_regular_file(relativePath, error) || error) {
				const auto executablePath = std::filesystem::path(REL::Module::get().filePath());
				const auto absolutePath = executablePath.parent_path() / relativePath;
				error.clear();
				if (std::filesystem::is_regular_file(absolutePath, error) && !error) {
					SKSE::log::error(
						"Address Library file exists at '{}' but not relative to the process working directory; CommonLib resolves it relative to the working directory",
						absolutePath.string());
				} else {
					SKSE::log::error(
						"Required Address Library file is missing: {} (install Address Library for SKSE Plugins v13)",
						relativePath.string());
				}
				return false;
			}

			// CommonLib treats a missing or incompatible Address Library as fatal. Check the
			// documented v5 header and bounds first so a bad/mismatched file fails closed.
			std::ifstream input(relativePath, std::ios::binary);
			std::int32_t format{};
			std::array<std::uint32_t, 4> fileVersion{};
			std::array<char, 64> libraryName{};
			std::int32_t pointerSize{};
			std::int32_t dataFormat{};
			std::int32_t offsetCount{};
			input.read(reinterpret_cast<char*>(&format), sizeof(format));
			input.read(reinterpret_cast<char*>(fileVersion.data()), sizeof(fileVersion));
			input.read(libraryName.data(), static_cast<std::streamsize>(libraryName.size()));
			input.read(reinterpret_cast<char*>(&pointerSize), sizeof(pointerSize));
			input.read(reinterpret_cast<char*>(&dataFormat), sizeof(dataFormat));
			input.read(reinterpret_cast<char*>(&offsetCount), sizeof(offsetCount));

			constexpr std::uint64_t headerBytes = sizeof(format) + sizeof(fileVersion) + 64 + sizeof(pointerSize) + sizeof(dataFormat) + sizeof(offsetCount);
			const auto fileBytes = std::filesystem::file_size(relativePath, error);
			const auto requiredBytes = headerBytes +
				(offsetCount > 0 ? static_cast<std::uint64_t>(offsetCount) * sizeof(std::uint32_t) : 0);
			if (!input || format != 5 ||
				fileVersion[0] != runtime.major() || fileVersion[1] != runtime.minor() ||
				fileVersion[2] != runtime.patch() || fileVersion[3] != runtime.build() ||
				pointerSize != static_cast<std::int32_t>(sizeof(void*)) || offsetCount <= 0 ||
				error || fileBytes < requiredBytes) {
				SKSE::log::error("Address Library file is not a valid v5 database for Skyrim {}", runtime.string("."));
				return false;
			}
			(void) libraryName;
			(void) dataFormat;
			return true;
		}
	}

	bool Install(const config::Settings& settings)
	{
		if (g_installed) {
			return true;
		}

		const auto runtime = REL::Module::get().version();
		if (runtime != SKSE::RUNTIME_SSE_1_7_104) {
			SKSE::log::error("Refusing to install casting hook for unsupported runtime {}", runtime.string("."));
			return false;
		}
		if (!HasRequiredAddressLibraryFile(runtime)) {
			return false;
		}

		const auto& module = REL::Module::get();
		const REL::Relocation<std::uintptr_t> actorMagicCasterVtable{ RE::VTABLE_ActorMagicCaster[0] };
		const auto vtableAddress = actorMagicCasterVtable.address();
		const auto slotOffset = sizeof(std::uintptr_t) * kSetCastingTimerForChargeVtableIndex;
		const auto rdata = module.segment(REL::Segment::Name::rdata);
		if (!Contains(rdata, vtableAddress, slotOffset + sizeof(std::uintptr_t))) {
			SKSE::log::error("ActorMagicCaster vtable relocation did not resolve inside Skyrim's read-only data segment");
			return false;
		}

		const auto slotAddress = vtableAddress + slotOffset;
		std::uintptr_t originalAddress{};
		std::memcpy(&originalAddress, reinterpret_cast<const void*>(slotAddress), sizeof(originalAddress));
		if (!IsExecutableAddress(module, originalAddress)) {
			SKSE::log::error("ActorMagicCaster charge-timer vtable slot did not resolve to executable Skyrim code");
			return false;
		}

		const auto hookAddress = stl::unrestricted_cast<std::uintptr_t>(&SetCastingTimerForChargeHook);
		g_settings = settings;
		g_originalSetCastingTimerForCharge = stl::unrestricted_cast<SetCastingTimerFunction>(originalAddress);
		if (!REL::safe_write(
				slotAddress,
				&hookAddress,
				sizeof(hookAddress),
				&originalAddress,
				sizeof(originalAddress))) {
			g_originalSetCastingTimerForCharge = nullptr;
			SKSE::log::error("ActorMagicCaster vtable slot changed before the hook could be installed; leaving vanilla behavior intact");
			return false;
		}

		g_installed = true;
		return true;
	}
}
