#pragma once

#include "Formats.hpp"
#include <memory>
#include <type_traits>

namespace WDBReader::Database {

	template<typename T>
	concept TDB2Format = requires(T t) {
		{ T::signature };
		std::is_pod_v<typename T::Header>;
		std::is_pod_v<typename T::SectionHeader>;
		std::is_pod_v<typename T::FieldStorageInfo>;
		std::is_pod_v<typename T::FieldStructure>;
	};

	enum DB2HeaderFlags {
		HasOffsetMap = 0x01,
		HasRelationshipData = 0x02,
		HasNonInlineIds = 0x04, 
		IsBitpacked = 0x10 
	};

	enum class DB2FieldCompression : uint32_t
	{
		// None -- usually the field is a 8-, 16-, 32-, or 64-bit integer in the record data. But can contain 96-bit value representing 3 floats as well
		None,
		// Bitpacked -- the field is a bitpacked integer in the record data.  It
		// is field_size_bits long and starts at field_offset_bits.
		// A bitpacked value occupies
		//   (field_size_bits + (field_offset_bits & 7) + 7) / 8
		// bytes starting at byte
		//   field_offset_bits / 8
		// in the record data.  These bytes should be read as a little-endian value,
		// then the value is shifted to the right by (field_offset_bits & 7) and
		// masked with ((1ull << field_size_bits) - 1).
		Bitpacked,
		// Common data -- the field is assumed to be a default value, and exceptions
		// from that default value are stored in the corresponding section in
		// common_data as pairs of { uint32_t record_id; uint32_t value; }.
		CommonData,
		// Bitpacked indexed -- the field has a bitpacked index in the record data.
		// This index is used as an index into the corresponding section in
		// pallet_data.  The pallet_data section is an array of uint32_t, so the index
		// should be multiplied by 4 to obtain a byte offset.
		BitpackedIndexed,
		// Bitpacked indexed array -- the field has a bitpacked index in the record
		// data.  This index is used as an index into the corresponding section in
		// pallet_data.  The pallet_data section is an array of uint32_t[array_count],
		//
		BitpackedIndexedArray,
		// Same as field_compression_bitpacked
		BitpackedSigned,
	};

#pragma pack(push, 1)

	struct WDC3SectionHeader // WDC3+
	{
		uint64_t tact_key_hash;          // TactKeyLookup hash
		uint32_t file_offset;            // absolute position to the beginning of the section
		uint32_t record_count;           // 'record_count' for the section
		uint32_t string_table_size;      // 'string_table_size' for the section
		uint32_t offset_records_end;     // Offset to the spot where the records end in a file with an offset map structure;
		uint32_t id_list_size;           // Size of the list of ids present in the section
		uint32_t relationship_data_size; // Size of the relationship data in the section
		uint32_t offset_map_id_count;    // Count of ids present in the offset map in the section
		uint32_t copy_table_count;       // Count of the number of deduplication entries (you can multiply by 8 to mimic the old 'copy_table_size' field)
	};

	struct WDC3FieldStructure //WDC3+
	{
		int16_t size;                   // size in bits as calculated by: byteSize = (32 - size) / 8; this value can be negative to indicate field sizes larger than 32-bits
		uint16_t position;              // position of the field within the record, relative to the start of the record
	};

	struct WDC3FieldStorageInfo //WDC3+
	{
		uint16_t field_offset_bits;
		uint16_t field_size_bits; // very important for reading bitpacked fields; size is the sum of all array pieces in bits - for example, uint32[3] will appear here as '96'
		// additional_data_size is the size in bytes of the corresponding section in
		// common_data or pallet_data.  These sections are in the same order as the
		// field_info, so to find the offset, add up the additional_data_size of any
		// previous fields which are stored in the same block (common_data or
		// pallet_data).
		uint32_t additional_data_size;
		DB2FieldCompression compression_type;

		union {
			struct {
				uint32_t bit_offset;
				uint32_t bit_width;
				bool is_signed;
			} bitpacked;

			struct {
				uint32_t default_value;
			} common_data;

			struct {
				uint32_t bit_offset;
				uint32_t bit_width;
				uint32_t array_size;
			} pallet;

			struct {
				uint32_t val1;
				uint32_t val2;
				uint32_t val3;
			} raw;
		} compression_data;

	};

	struct WDC3CopyTableEntry //WDC3+
	{
		uint32_t id_of_new_row;
		uint32_t id_of_copied_row;
	};

	struct WDC3OffsetMapEntry //WDC3+
	{
		uint32_t offset;
		uint16_t size;
	};

	struct WDC3RelationshipEntry //WDC3+
	{
		uint32_t foreign_id;
		uint32_t record_index;
	};

	struct WDC3PalletValue {
		uint32_t value;
	};

	struct WDC3CommonValue {
		using id_t = uint32_t;
		using value_t = uint32_t;
		id_t record_id;
		value_t value;
	};

	struct DB2FormatWDC3 {
		constexpr static Signature signature = WDC3_MAGIC;

		struct Header {
			uint32_t signature;
			uint32_t record_count;           // this is for all sections combined now
			uint32_t field_count;
			uint32_t record_size;
			uint32_t string_table_size;      // this is for all sections combined now
			uint32_t table_hash;             // hash of the table name
			uint32_t layout_hash;            // this is a hash field that changes only when the structure of the data changes
			uint32_t min_id;
			uint32_t max_id;
			uint32_t locale;                 // as seen in TextWowEnum
			uint16_t flags;                  // possible values are listed in Known Flag Meanings
			uint16_t id_index;               // this is the index of the field containing ID values; this is ignored if flags & 0x04 != 0
			uint32_t total_field_count;      // from WDC1 onwards, this value seems to always be the same as the 'field_count' value
			uint32_t bitpacked_data_offset;  // relative position in record where bitpacked data begins; not important for parsing the file
			uint32_t lookup_column_count;
			uint32_t field_storage_info_size;
			uint32_t common_data_size;
			uint32_t pallet_data_size;
			uint32_t section_count;          // new to WDC2, this is number of sections of data
		};

		using SectionHeader = WDC3SectionHeader;
		using FieldStorageInfo = WDC3FieldStorageInfo;
		using FieldStructure = WDC3FieldStructure;
		using CopyTableEntry = WDC3CopyTableEntry;
		using OffsetMapEntry = WDC3OffsetMapEntry;
		using RelationshipEntry = WDC3RelationshipEntry;

		using PalletValue = WDC3PalletValue;
		using CommonValue = WDC3CommonValue;
	};

	static_assert(TDB2Format<DB2FormatWDC3>);

	struct DB2FormatWDC4 {
		constexpr static Signature signature = WDC4_MAGIC;

		using Header = DB2FormatWDC3::Header;
		using SectionHeader = WDC3SectionHeader;
		using FieldStorageInfo = WDC3FieldStorageInfo;
		using FieldStructure = WDC3FieldStructure;
		using CopyTableEntry = WDC3CopyTableEntry;
		using OffsetMapEntry = WDC3OffsetMapEntry;
		using RelationshipEntry = WDC3RelationshipEntry;

		using PalletValue = WDC3PalletValue;
		using CommonValue = WDC3CommonValue;
	};

	static_assert(TDB2Format<DB2FormatWDC4>);

	struct DB2FormatWDC5 {
		constexpr static Signature signature = WDC5_MAGIC;

		struct Header {
			uint32_t signature;
			uint32_t versionNum;
			uint8_t schemaString[128];
			uint32_t record_count;           // this is for all sections combined now
			uint32_t field_count;
			uint32_t record_size;
			uint32_t string_table_size;      // this is for all sections combined now
			uint32_t table_hash;             // hash of the table name
			uint32_t layout_hash;            // this is a hash field that changes only when the structure of the data changes
			uint32_t min_id;
			uint32_t max_id;
			uint32_t locale;                 // as seen in TextWowEnum
			uint16_t flags;                  // possible values are listed in Known Flag Meanings
			uint16_t id_index;               // this is the index of the field containing ID values; this is ignored if flags & 0x04 != 0
			uint32_t total_field_count;      // from WDC1 onwards, this value seems to always be the same as the 'field_count' value
			uint32_t bitpacked_data_offset;  // relative position in record where bitpacked data begins; not important for parsing the file
			uint32_t lookup_column_count;
			uint32_t field_storage_info_size;
			uint32_t common_data_size;
			uint32_t pallet_data_size;
			uint32_t section_count;          // new to WDC2, this is number of sections of data
		};

		using SectionHeader = WDC3SectionHeader;
		using FieldStorageInfo = WDC3FieldStorageInfo;
		using FieldStructure = WDC3FieldStructure;
		using CopyTableEntry = WDC3CopyTableEntry;
		using OffsetMapEntry = WDC3OffsetMapEntry;
		using RelationshipEntry = WDC3RelationshipEntry;

		using PalletValue = WDC3PalletValue;
		using CommonValue = WDC3CommonValue;
	};

	static_assert(TDB2Format<DB2FormatWDC5>);

#pragma pack(pop)
}