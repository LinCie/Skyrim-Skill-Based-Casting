#include "Config.h"

#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <string_view>
#include <system_error>

namespace skill_based_casting::config
{
	namespace
	{
		[[nodiscard]] std::string_view Trim(const std::string_view value) noexcept
		{
			const auto first = value.find_first_not_of(" \t\r\n");
			if (first == std::string_view::npos) {
				return {};
			}
			const auto last = value.find_last_not_of(" \t\r\n");
			return value.substr(first, last - first + 1);
		}

		[[nodiscard]] bool EqualInsensitive(const std::string_view lhs, const std::string_view rhs) noexcept
		{
			if (lhs.size() != rhs.size()) {
				return false;
			}
			for (std::size_t i = 0; i < lhs.size(); ++i) {
				const auto left = lhs[i] >= 'A' && lhs[i] <= 'Z' ? lhs[i] - 'A' + 'a' : lhs[i];
				const auto right = rhs[i] >= 'A' && rhs[i] <= 'Z' ? rhs[i] - 'A' + 'a' : rhs[i];
				if (left != right) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] bool ParseBool(const std::string_view value, bool& output) noexcept
		{
			if (EqualInsensitive(value, "true") || value == "1") {
				output = true;
				return true;
			}
			if (EqualInsensitive(value, "false") || value == "0") {
				output = false;
				return true;
			}
			return false;
		}

		[[nodiscard]] bool ParseDouble(const std::string_view value, double& output) noexcept
		{
			const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), output, std::chars_format::general);
			return error == std::errc{} && end == value.data() + value.size() && std::isfinite(output);
		}

		[[nodiscard]] bool ParseInt(const std::string_view value, int& output) noexcept
		{
			const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), output);
			return error == std::errc{} && end == value.data() + value.size();
		}

		void WarnInvalid(LoadResult& result, const std::size_t line, const std::string_view key)
		{
			result.warnings.emplace_back("line " + std::to_string(line) + ": invalid value for " + std::string(key) + "; using the previous/default value");
		}
	}

	LoadResult Load(const std::filesystem::path& path)
	{
		std::ifstream input(path);
		if (!input) {
			return {};
		}
		return Parse(input);
	}

	LoadResult Parse(std::istream& input)
	{
		LoadResult result;
		result.loadedFromFile = true;

		std::string line;
		std::string section;
		std::size_t lineNumber = 0;
		while (std::getline(input, line)) {
			++lineNumber;
			auto text = Trim(line);
			if (text.empty() || text.front() == ';' || text.front() == '#') {
				continue;
			}

			if (text.front() == '[') {
				if (text.size() < 2 || text.back() != ']') {
					result.warnings.emplace_back("line " + std::to_string(lineNumber) + ": malformed section header ignored");
					section = {};
					continue;
				}
				section = Trim(text.substr(1, text.size() - 2));
				continue;
			}

			const auto equals = text.find('=');
			if (equals == std::string_view::npos) {
				result.warnings.emplace_back("line " + std::to_string(lineNumber) + ": malformed setting ignored");
				continue;
			}

			const auto key = Trim(text.substr(0, equals));
			auto value = Trim(text.substr(equals + 1));
			const auto comment = value.find_first_of(";#");
			if (comment != std::string_view::npos) {
				value = Trim(value.substr(0, comment));
			}

			if (EqualInsensitive(section, "General") && EqualInsensitive(key, "bEnableNPCSupport")) {
				if (!ParseBool(value, result.settings.enableNPCSupport)) {
					WarnInvalid(result, lineNumber, key);
				}
			} else if (EqualInsensitive(section, "Progression") && EqualInsensitive(key, "fMinChargeTimeMultiplier")) {
				double parsed{};
				if (ParseDouble(value, parsed) && parsed > 0.0) {
					result.settings.progression.minimumSpeed = parsed;
				} else {
					WarnInvalid(result, lineNumber, key);
				}
			} else if (EqualInsensitive(section, "Progression") && EqualInsensitive(key, "fChargeTimeMultiplier")) {
				double parsed{};
				if (ParseDouble(value, parsed) && parsed >= 0.0) {
					result.settings.progression.speedPerLevel = parsed;
				} else {
					WarnInvalid(result, lineNumber, key);
				}
			} else if (EqualInsensitive(section, "Progression") && EqualInsensitive(key, "iMaxSkillLevel")) {
				int parsed{};
				if (ParseInt(value, parsed) && parsed > 0) {
					result.settings.progression.maximumSkillLevel = parsed;
				} else {
					WarnInvalid(result, lineNumber, key);
				}
			} else if (EqualInsensitive(section, "Debug") && EqualInsensitive(key, "bEnableDebugLogging")) {
				if (!ParseBool(value, result.settings.enableDebugLogging)) {
					WarnInvalid(result, lineNumber, key);
				}
			}
		}

		const auto& progression = result.settings.progression;
		const auto maximumFactor = progression.minimumSpeed +
			(static_cast<double>(progression.maximumSkillLevel) * progression.speedPerLevel);
		if (!std::isfinite(maximumFactor) || maximumFactor <= 0.0) {
			result.warnings.emplace_back("progression values overflow the speed factor; using default progression settings");
			result.settings.progression = progression::Settings{};
		}
		return result;
	}
}
