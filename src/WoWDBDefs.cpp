#include "WDBReader/WoWDBDefs.hpp"

#include <algorithm>
#include <cassert>
#include <format>
#include <ranges>

namespace WDBReader::WoWDBDefs {

	const std::array<std::string, 4> DBDReader::ValidTypes { 
		"int", 
		"float", 
		"string", 
		"locstring"
	};

    DBDefinition DBDReader::read(std::istream& stream) {

		assert(!stream.fail());
		assert(stream.tellg() == 0);

		// based on https://github.com/wowdev/WoWDBDefs/blob/master/code/C%23/DBDefsLib/DBDReader.cs

		DBDefinition db_definition;

		std::string first_line;
		auto line_number = 1;
		std::getline(stream, first_line);

		if (first_line != "COLUMNS") {
			throw std::runtime_error("File does not start with column definitions!");
		}

		while (!stream.eof()) {
			line_number++;
			std::string line;
			std::getline(stream, line);

			// Column definitions are done after encountering a newline
			if (line.find_first_not_of(' ') == line.npos) {
				break;
			}

			ColumnDefinition column_definition;

			const auto first_space_pos = line.find_first_of(' ');
			/* TYPE READING */
			if (first_space_pos == line.npos) {
				throw std::runtime_error(std::format("Line {} does not contain a space between type and column name!", line_number));
			}

			const auto type_pos = line.find_first_of(" <");
			assert(type_pos != line.npos);
			auto type = line.substr(0, type_pos);
			
			if (std::ranges::find(ValidTypes, type) == ValidTypes.end()) {
				throw std::runtime_error(std::format("Invalid type: {} on line {}", type, line_number));
			}
			else {
				column_definition.type = type;
			}

			/* FOREIGN KEY READING */
			if (type_pos < line.length()) {
				if (line[type_pos] == '<') {
					const auto end_key_pos = line.find_first_of(">");
					if (end_key_pos == line.npos) {
						throw std::runtime_error("Unable to find foriegn key end token.");
					}

					const auto KEY_SEPARATOR = "::";
					auto foreign_key = line.substr(type_pos + 1, (end_key_pos - type_pos) - 1);
					const auto separator_pos = foreign_key.find(KEY_SEPARATOR);
					if (separator_pos == line.npos) {
						throw std::runtime_error("Unable to find foriegn key separator token.");
					}

					column_definition.foreignTable = foreign_key.substr(0, separator_pos);
					column_definition.foreignColumn = foreign_key.substr(separator_pos + std::strlen(KEY_SEPARATOR));
				}
			}

			/* NAME READING */
			const auto next_space_pos = line.find_first_of(' ', first_space_pos + 1);
			std::string name = "";
			if (next_space_pos == line.npos) {
				name = line.substr(first_space_pos + 1);
			}
			else {
				name = line.substr(first_space_pos + 1, (next_space_pos - first_space_pos) - 1);
			}


			if (name.ends_with('?')) {
				column_definition.verified = false;
				name.pop_back();
			}
			else {
				column_definition.verified = true;
			}

			const auto COMMENT_SEPARATOR = "//";
			auto comment_pos = line.find("//");
			if (comment_pos != line.npos) {
				column_definition.comment = trim(line.substr(comment_pos + std::strlen(COMMENT_SEPARATOR)));
			}

			if (db_definition.columnDefinitions.contains(name)) {
				throw std::runtime_error(std::format("Column name '{}' already exists.", name));
			}
			else {
				db_definition.columnDefinitions.emplace(name, column_definition);
			}
		}

		std::vector<Definition> defintions;
		std::vector<std::string> layout_hashes;
		std::string comment = "";
		std::vector<Build> builds;
		std::vector<BuildRange> build_ranges;

		auto try_push_version_definition = [&]() {
			if (builds.size() != 0 || build_ranges.size() != 0 || layout_hashes.size() != 0) {
				VersionDefinitions ver_defs;
				ver_defs.builds = builds;
				ver_defs.buildRanges = build_ranges;
				ver_defs.layoutHashes = layout_hashes;
				ver_defs.comment = comment;
				ver_defs.definitions = defintions;

				db_definition.versionDefinitions.push_back(std::move(ver_defs));
			}
			else if (defintions.size() != 0 || comment.find_first_not_of(' ') != comment.npos) {
				throw std::runtime_error("No BUILD or LAYOUT, but non-empty lines/'definitions'.");
			}

			defintions.clear();
			layout_hashes.clear();
			comment = "";
			builds.clear();
			build_ranges.clear();
		};


		while (!stream.eof()) {
			line_number++;
			std::string line;
			std::getline(stream, line);

			const bool is_line_empty_or_whitespace = line.find_first_not_of(' ') == line.npos;

			if (is_line_empty_or_whitespace) {
				try_push_version_definition();
			}

			constexpr auto KEYWORD_LAYOUT = "LAYOUT";
			constexpr auto KEYWORD_BUILD = "BUILD";
			constexpr auto KEYWORD_COMMENT = "COMMENT";

			if (line.starts_with(KEYWORD_LAYOUT)) {

				auto hashes_full = line.substr(std::strlen(KEYWORD_LAYOUT) + 1);
				auto hashes_split = split_string(hashes_full, ", ");
				layout_hashes.insert(layout_hashes.end(), hashes_split.begin(), hashes_split.end());
			}
			else if (line.starts_with(KEYWORD_BUILD)) {
				auto builds_full = line.substr(std::strlen(KEYWORD_BUILD) + 1);
				auto builds_split = split_string(builds_full, ", ");
				for (const auto& build_str : builds_split) {
					const auto range_sep_pos = build_str.find_first_of('-');
					if (range_sep_pos != build_str.npos) {
						auto str_min = build_str.substr(0, range_sep_pos);
						auto str_max = build_str.substr(range_sep_pos + 1);
						build_ranges.emplace_back(Build(str_min), Build(str_max));
					}
					else {
						builds.emplace_back(build_str);
					}
				}
			}
			else if (line.starts_with(KEYWORD_COMMENT)) {
				comment = trim(line.substr(std::strlen(KEYWORD_COMMENT)));
			}
			else if (!is_line_empty_or_whitespace) {

				Definition def;
				def.isNonInline = false;

				auto extract_between_tokens = [](std::string& str, char start_token, char end_token, auto callback) {
					const auto start_pos = str.find_first_of(start_token);
					if (start_pos != str.npos) {
						const auto end_pos = str.find_first_of(end_token, start_pos + 1);
						if (end_pos == str.npos) {
							throw std::runtime_error("End token is missing.");
						}

						auto temp = str.substr(start_pos + 1, (end_pos - start_pos) - 1);
						callback(temp);

						str.erase(start_pos, (end_pos - start_pos) + 1);
					}
				};

				extract_between_tokens(line, '$', '$', [&](const std::string& str) {
					auto annotations = split_string(str, ",");

					if (std::ranges::any_of(annotations, [](const auto& e) { return e == "id"; })) {
						def.isID = true;
					}

					if (std::ranges::any_of(annotations, [](const auto& e) { return e == "noninline"; })) {
						def.isNonInline = true;
					}

					if (std::ranges::any_of(annotations, [](const auto& e) { return e == "relation"; })) {
						def.isRelation = true;
					}
				});

				extract_between_tokens(line, '<', '>', [&](const std::string& str) {
					if (str.length() > 0 && str[0] == 'u') {
						def.isSigned = false;
						std::from_chars(str.data() + 1, (str.data() + 1 + str.length()) - 1, def.size);
					}
					else {
						def.isSigned = true;
						std::from_chars(str.data(), str.data() + str.length(), def.size);
					}
				});
		
				extract_between_tokens(line, '[', ']', [&](const std::string& str) {
					std::from_chars(str.data(), str.data() + str.size(), def.arrLength);
				});

				def.name = line;

				if (!db_definition.columnDefinitions.contains(def.name)) {
					throw std::runtime_error(std::format("Unable to find {} in column definitions!", def.name));
				}
				else {
					// Temporary unsigned format update conversion code
					if (db_definition.columnDefinitions[def.name].type == "uint") {
						def.isSigned = false;
					}
				}

				defintions.push_back(def);
			}
		}

		try_push_version_definition();

		return db_definition;
	}

	std::optional<Database::RuntimeSchema> makeSchema(const DBDefinition& db_definition, const GameVersion& target_version) {

        auto build_schema = [&db_definition](const std::vector<Definition>& definitions) -> Database::RuntimeSchema {
            std::vector<Database::RuntimeSchema::field_name_t> names;
            names.reserve(definitions.size());
            std::vector<Database::Field> fields;
            fields.reserve(definitions.size());

            for (const auto& def : definitions) {
                names.push_back(def.name);

                const auto& type_str = db_definition.columnDefinitions.at(def.name).type;
                const auto ann = Database::Annotation(def.isID, def.isRelation, !def.isNonInline, def.isSigned);

				using array_size_t = decltype(Database::Field::size);
				assert(def.arrLength < std::numeric_limits<array_size_t>::max());
                const auto array_size = static_cast<array_size_t>(std::max(1, def.arrLength));

                if (type_str == "int") {
                    fields.push_back(Database::Field::integerArray((def.size / 8) * array_size, array_size, ann));
                }
                else if (type_str == "float") {
                    fields.push_back(Database::Field::floatingPointArray(sizeof(float) * array_size, array_size, ann));
                }
                else if (type_str == "string") {
                    fields.push_back(Database::Field::string(array_size, ann));
                }
                else if (type_str == "locstring") {
                    fields.push_back(Database::Field::langString(array_size, ann));
                }
                else {
                    throw std::runtime_error("Unexpected field type: " + type_str);
                }
            }

            return Database::RuntimeSchema(std::move(fields), std::move(names));
        };

        for (const auto& version_def : db_definition.versionDefinitions) {
            bool matched = std::ranges::find(version_def.builds, target_version) != version_def.builds.end();

            if (!matched) {
                matched = std::ranges::find_if(version_def.buildRanges, [&target_version](const BuildRange& range) {
                    return range.contains(target_version);
                }) != version_def.buildRanges.end();
            }

            if (matched) {
                return build_schema(version_def.definitions);
            }
        }

        return std::nullopt;
	}

}