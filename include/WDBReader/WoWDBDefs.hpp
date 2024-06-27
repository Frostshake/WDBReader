#pragma once

#include "Utility.hpp"
#include "Database.hpp"
#include <array>
#include <cstdint>
#include <istream>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace WDBReader::WoWDBDefs {
    using Build = WDBReader::Utility::GameVersion;

	struct BuildRange {
	public:
		Build minBuild;
		Build maxBuild;

		BuildRange() = delete;
		constexpr BuildRange(Build min, Build max) :
			minBuild(min),
			maxBuild(max)
		{}

		constexpr bool contains(const Build& build) const {
			return maxBuild >= build && build >= minBuild;
		}
	};

	struct ColumnDefinition {
	public:
		std::string type;
		std::string foreignTable;
		std::string foreignColumn;
		bool verified {false};
		std::string comment;
	};

	struct Definition
	{
	public:
		int32_t size {0};
		int32_t arrLength {0};
		std::string name;
		bool isID {false};
		bool isRelation {false};
		bool isNonInline {false};
		bool isSigned {false};
		std::string comment;
	};

	struct VersionDefinitions
	{
	public:
		std::vector<Build> builds;
		std::vector<BuildRange> buildRanges;
		std::vector<std::string> layoutHashes;
		std::string comment;
		std::vector<Definition> definitions;
	};

	struct DBDefinition
	{
	public:
		std::map<std::string, ColumnDefinition> columnDefinitions;
		std::vector<VersionDefinitions> versionDefinitions;
	};

    class DBDReader {
	public:
		DBDReader() = delete;
		static DBDefinition read(std::istream& stream);
		static const std::array<std::string, 4> ValidTypes;
	};

	std::optional<Database::RuntimeSchema> makeSchema(const DBDefinition& db_definition, const Utility::GameVersion& target_version);
}