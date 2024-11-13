#pragma once

#include "Schema.hpp"
#include "../Filesystem.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace WDBReader::Database {

	union Signature {
	public:
		Signature() = default;

		template<size_t N>
		consteval Signature(const char(&sig)[N]) noexcept {
			static_assert(N == (4 + 1));
			integer = uint32_t((uint8_t)sig[3] << 24 |
				(uint8_t)sig[2] << 16 |
				(uint8_t)sig[1] << 8 |
				(uint8_t)sig[0]);
		}

		std::array<uint8_t, 4> bytes;
		uint32_t integer;

		static_assert(sizeof(integer) == sizeof(bytes));

		constexpr std::string_view str() const noexcept {
			return std::string_view((char*)&bytes, bytes.size());
		}
	};

	constexpr Signature WDBC_MAGIC = "WDBC";
	constexpr Signature WDB2_MAGIC = "WDB2";
	constexpr Signature WDC3_MAGIC = "WDC3";
	constexpr Signature WDC4_MAGIC = "WDC4";
	constexpr Signature WDC5_MAGIC = "WDC5";

	struct DBFormat {
	public:
		constexpr DBFormat(Signature sig) noexcept :
			signature(sig) {}

		Signature signature;
		std::optional<uint32_t> tableHash;
		std::optional<uint32_t> layoutHash;
	};


	/// <summary>
	/// Reads the current C string from the file source.
	/// </summary>
	template<WDBReader::Filesystem::TFileSource FS>
	string_data_t readCurrentString(FS* _source) 
	{
		std::string buffer;
		std::array<char, 32> intermediate;
		bool end_of_string = false;
		bool end_of_file = false;

		while(!end_of_string && !end_of_file) {
			const size_t src_available = _source->size() - _source->getPos();
			const auto to_read = std::min(intermediate.size(), src_available);

			if (to_read < intermediate.size()) {
				end_of_file = true;
			}

			_source->read(intermediate.data(), to_read);

			size_t append_pos = 0;
			for (size_t i = 0; i < to_read; i++) {
				append_pos++;
				if (intermediate[i] == '\0') {
					end_of_string = true;
					break;
				}
			}

			buffer.append(intermediate.data(), append_pos);
		}

		if (!end_of_string) {
			buffer.push_back('\0');
		}

		auto result = std::make_unique_for_overwrite<string_data_t::element_type[]>(buffer.size());
		memmove(result.get(), buffer.data(), buffer.size());
		return result;
	}
}