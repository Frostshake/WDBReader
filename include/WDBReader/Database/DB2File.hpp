#pragma once

#include "../Database.hpp"
#include "../Utility.hpp"
#include "DB2Format.hpp"
#include <memory>
#include <ranges>
#include <type_traits>
#include <vector>
#include <unordered_map>

namespace WDBReader::Database {

	template<typename T>
	using integer_equivelant_t = std::conditional_t<std::is_same_v<T, float>, uint32_t, T>;

	class DB2Format {
	public:
		template<TSchema S>
		static constexpr size_t recordSizeDest(const S& schema) {
			return recordSize<string_data_t>(schema);
		}

		template<TSchema S>
		static constexpr size_t recordSizeSrc(const S& schema) {
			return recordSize<string_ref_t>(schema);
		}
	private:
		template<typename StrT, TSchema S>
		static constexpr size_t recordSize(const S& schema) {
			size_t sum = 0;
			for (const auto& field : schema.fields()) {
				if (field.type == Field::Type::STRING || field.type == Field::Type::LANG_STRING) {
					sum += (sizeof(StrT) * field.size);
				}
				else {
					sum += field.totalBytes();
				}
			}
			return sum;
		}

		DB2Format() = delete;
	};

	struct DB2LoadInfo {
	public:
		template<TSchema S>
		constexpr static DB2LoadInfo make(const S& schema) {
			bool _expect_id_list = false;
			if (schema.fields().size() > 0) {
				const Field& field = schema.fields()[0];
				_expect_id_list = field.annotation.isId && !field.annotation.isInline;
			}
			
			return { _expect_id_list };
		}
		const bool useIdList;
	};

	using db2_record_id_t = uint32_t;

	template<TDB2Format F>
	struct DB2Structure {
	public:
		F::Header header;
		std::unique_ptr<typename F::SectionHeader[]> sectionHeaders;
		std::unique_ptr<typename F::FieldStructure[]> fieldStructures;
		std::unique_ptr<typename F::FieldStorageInfo[]> fieldStorage;

		std::unique_ptr<std::unique_ptr<typename F::PalletValue[]>[]> indexedPalletData;
		std::unique_ptr<std::unique_ptr<typename F::CommonValue[]>[]> indexedCommonData;	//	only used for loading.
		std::unique_ptr<std::unordered_map<typename F::CommonValue::id_t, typename F::CommonValue::value_t>[]> commonData;

		std::vector<db2_record_id_t> idList;
		std::vector<typename F::CopyTableEntry> copyTable;
		std::vector<typename F::OffsetMapEntry> offsetMap;
		std::vector<db2_record_id_t> offsetMapIds;
		std::vector<typename F::RelationshipEntry> relationships;	// only used during loading.
		std::unordered_map<decltype(F::RelationshipEntry::record_index), decltype(F::RelationshipEntry::foreign_id)> relationshipMap;
	};

	template<TDB2Format F, TRecord R>
	class DB2Loader {
	public:
		virtual ~DB2Loader() = default;
		virtual void loadSection(const typename F::SectionHeader& format) = 0;		
		virtual uint32_t size() const = 0;
		virtual R operator[](uint32_t index) const = 0;
	};

	template<TDB2Format F, TSchema S, TRecord R, Filesystem::TFileSource FS>
	class DB2LoaderStandard final : public DB2Loader<F, R> {
	public:
		DB2LoaderStandard(const S& schema, const DB2LoadInfo& load, DB2Structure<F>& structure, FS* source) :
			_schema(schema), _load_info(load), _structure(structure), _source(source), _record_size(DB2Format::recordSizeSrc(schema))
		{
			_section_offsets.reserve(_structure.header.section_count);
			_buffer.resize(_structure.header.record_size);
		}

		virtual ~DB2LoaderStandard() = default;

		void loadSection(const typename F::SectionHeader& section) override {

			SectionOffset current{
				_structure.header.record_size * section.record_count,
				section.string_table_size,
				section.record_count
			};

			_source->setPos(_source->getPos() + current.dataOffsetEnd + current.stringOffsetEnd);

			for (const auto& offset : _section_offsets) {
				current.dataOffsetEnd += offset.dataOffsetEnd;
				current.stringOffsetEnd += offset.stringOffsetEnd;
				current.recordIndexEnd += offset.recordIndexEnd;
			}

			_section_offsets.push_back(std::move(current));
		}

		uint32_t size() const override {
			return _structure.header.record_count + _structure.copyTable.size();
		}

		R operator[](uint32_t index) const override {

			uint32_t lookup_index = index;
			std::optional<db2_record_id_t> replacement_id;

			if (index >= _structure.header.record_count) {
				const auto& copy_entry = _structure.copyTable[index - _structure.header.record_count];
				auto id_it = std::ranges::find(_structure.idList, copy_entry.id_of_copied_row);

				if (id_it == _structure.idList.end()) {
					throw WDBReaderException("Copy table id doesnt exist.");
				}

				lookup_index = std::distance(_structure.idList.begin(), id_it);
				replacement_id = copy_entry.id_of_new_row;
			}

			const auto section_index = getSectionIndex(lookup_index);
			const SectionOffset& offset = _section_offsets[section_index];
			const auto section_record_index_start = offset.recordIndexEnd - _structure.sectionHeaders[section_index].record_count;
			const auto relative_record_index = lookup_index - section_record_index_start;

			const bool is_encypted_section = _structure.sectionHeaders[section_index].tact_key_hash != 0;

			const uint64_t source_record_start_pos = _structure.sectionHeaders[section_index].file_offset + (relative_record_index * _structure.header.record_size);
			_source->setPos(source_record_start_pos);

			std::fill(_buffer.begin(), _buffer.end(), 0);
		
			_source->read(_buffer.data(), _structure.header.record_size);

			R record;	
			record.recordIndex = index;
			record.encryptionState = RecordEncryption::NONE;

			uint32_t schema_field_index = 0;
			ptrdiff_t view_offset = 0;
			db2_record_id_t record_id = 0;
			db2_record_id_t id_list_use_id = 0;

			if (_load_info.useIdList) {
				id_list_use_id = replacement_id.has_value() ? replacement_id.value() : _structure.idList[lookup_index];
				if (id_list_use_id == 0 && is_encypted_section) {
					//if the idList value is zero, then the ID is encrypted. 
					//even if the buffer is non-zero, it could still be bad (AFAIK it should be read as zero from casclib, however doesnt seem to always be)
					record.encryptionState = RecordEncryption::ENCRYPTED;
					return record;
				}
			}

			if (is_encypted_section) {
				const bool record_encrypted = std::all_of(_buffer.begin(), _buffer.end(), [](auto i) { return i == 0; });
				record.encryptionState = record_encrypted ? RecordEncryption::ENCRYPTED : RecordEncryption::DECRYPTED;

				if (record_encrypted) {
					return record;
				}
			}

			R::make(&record, _schema.elementCount(), _record_size);

			if (_load_info.useIdList) {
				assert(record_id == 0);
				R::insertField(&record, schema_field_index, 1, view_offset);
				R::insertValue(&record, schema_field_index++, 0, view_offset, id_list_use_id);
				view_offset += sizeof(uint32_t);
				record_id = id_list_use_id;
			}

			for (uint32_t x = 0; x < _structure.header.field_count; x++) {
				const Field& schema_field = _schema.fields()[schema_field_index];
				assert(schema_field.annotation.isInline);
				R::insertField(&record, schema_field_index, schema_field.size, view_offset);

				if (schema_field.annotation.isId && replacement_id.has_value()) {
					assert(!schema_field.isArray());
					assert(schema_field.bytes == sizeof(db2_record_id_t));
					R::insertValue(&record,
						schema_field_index,
						0,
						view_offset,
						replacement_id.value()
					);
					view_offset += sizeof(db2_record_id_t);
					assert(record_id == 0);
					record_id = replacement_id.value();
				}
				else {
					for (auto z = 0; z < schema_field.size; z++) {
						schemaFieldHandler(schema_field, [&]<typename T>() {
							if constexpr (std::is_same_v<string_data_t, T>) {
								const auto str_ref = getRecordFieldValue<string_ref_t>(_buffer.data(), x, z, record_id);
								auto str_pos = source_record_start_pos +
									(_structure.fieldStorage[x].field_offset_bits / 8) +
									str_ref;

								str_pos -= (_structure.header.record_count - _structure.sectionHeaders[0].record_count) * _structure.header.record_size; //weird fix need for multi section records.
								_source->setPos(str_pos);

								R::insertValue(&record, schema_field_index, z, view_offset, std::move(readCurrentString(_source)));
								view_offset += sizeof(T);
							}
							else {
								auto val = getRecordFieldValue<T>(_buffer.data(), x, z, record_id);

								if (schema_field.annotation.isId && z == 0) {
									assert(record_id == 0);
									record_id = val;
								}

								R::insertValue(&record,
									schema_field_index,
									z,
									view_offset,
									std::move(val)
								);
								view_offset += sizeof(T);
							}
						});
					}
				}
			
				schema_field_index++;
			}

			assert(schema_field_index <= _structure.header.field_count + 1);

			if (schema_field_index < _schema.fields().size() && _structure.relationshipMap.size() > 0) {
				for (auto x = schema_field_index; x < _schema.fields().size(); ++x) {
					const Field& schema_field = _schema.fields()[schema_field_index];

					assert(schema_field.annotation.isRelation && !schema_field.annotation.isInline);
					assert(schema_field.size == 1);
					assert(schema_field.type == Field::Type::INT);

					R::insertField(&record, schema_field_index, 1, view_offset);

					schemaFieldHandler(schema_field, [&]<typename T>() {
						if constexpr (std::is_integral_v<T>) {
							if ((_structure.header.flags & DB2HeaderFlags::HasRelationshipData) != 0) {
								// relation index is actually the ID
								//TODO implement - so far have not found any files which use this.
								throw std::logic_error("DB2 Relations using ID has not yet been implemented.");
							}
							else {
								R::insertValue(&record,
									schema_field_index,
									0,
									view_offset,
									(T)_structure.relationshipMap[lookup_index]
								);
								view_offset += sizeof(T);
							}
						}
					});

					schema_field_index++;
				}
			}
	
			assert(schema_field_index == _schema.fields().size());

			return std::move(record); 
		}

	protected:

		template<typename T>
		inline T getRecordFieldValue(uint8_t* buff, uint32_t field_index, uint32_t array_index, db2_record_id_t record_id) const {
			
			const F::FieldStorageInfo& field_info = _structure.fieldStorage[field_index];

			switch (field_info.compression_type) {
			case DB2FieldCompression::None:
			{
				const auto offset = field_info.field_offset_bits / 8;
				assert(sizeof(T) <= (field_info.field_size_bits / 8)); // field_size_bits is total size of the array, so direct comparison to T not accurate.
				return *reinterpret_cast<T const*>(buff + offset + (sizeof(T) * array_index));
			}
				break;
			case DB2FieldCompression::Bitpacked:
			{
				const auto offset = field_info.compression_data.bitpacked.bit_offset / 8 + _structure.header.bitpacked_data_offset;
				T value = getBitpackedValue<T>(buff + offset, field_info.compression_data.bitpacked);
				return value;
			}
				break;
			case DB2FieldCompression::CommonData:
			{
				if (record_id == 0) {
					throw std::logic_error("Record id not set when accessing common data.");
				}

				auto map_itr = _structure.commonData[field_index].find(record_id);
				if (map_itr != _structure.commonData[field_index].end()) {
					return map_itr->second;
				}

				return field_info.compression_data.common_data.default_value;
			}
				break;
			case DB2FieldCompression::BitpackedIndexed:
			{
				const auto offset = field_info.compression_data.pallet.bit_offset / 8 + _structure.header.bitpacked_data_offset;
				const auto pallet_index = getBitpackedValue<uint64_t>(buff + offset, field_info.compression_data.pallet);
				T value = _structure.indexedPalletData[field_index][pallet_index].value;
				return value;
			}
				break;
			case DB2FieldCompression::BitpackedIndexedArray:
			{
				const auto offset = field_info.compression_data.pallet.bit_offset / 8 + _structure.header.bitpacked_data_offset;
				const auto pallet_index = getBitpackedValue<uint64_t>(buff + offset, field_info.compression_data.pallet);
				const auto key = (pallet_index * field_info.compression_data.pallet.array_size) + array_index;
				T value = _structure.indexedPalletData[field_index][key].value;
				return value;
			}
				break;
			case DB2FieldCompression::BitpackedSigned:
			{
				const auto offset = field_info.compression_data.bitpacked.bit_offset / 8 + _structure.header.bitpacked_data_offset;
				integer_equivelant_t<T> value = getBitpackedValue<T>(buff + offset, field_info.compression_data.bitpacked);
				const integer_equivelant_t<T> mask = integer_equivelant_t<T>(1) << (field_info.compression_data.bitpacked.bit_width - 1);
				value = (value ^ mask) - mask;
				return *reinterpret_cast<T const*>(&value);
			}
				break;
			}
			
			return T(0);
		}

		template<typename T, typename D>
		inline T getBitpackedValue(uint8_t* buff, const D& compression_data) const 
			requires (sizeof(T) <= sizeof(uint64_t))
		{
			const auto bit_width = compression_data.bit_width;
			const auto bits_to_read = compression_data.bit_offset & 7;
		
			const auto val = *reinterpret_cast<uint64_t const*>(buff) << (64 - bits_to_read - bit_width) >> (64 - bit_width);
			return val;
		}

		uint32_t getSectionIndex(uint32_t record_index) const {
			uint32_t section = 0;
			for (; section < _structure.header.section_count; ++section)
			{
				const F::SectionHeader& sectionHeader = _structure.sectionHeaders[section];
				if (record_index < sectionHeader.record_count)
					break;

				record_index -= sectionHeader.record_count;
			}

			return section;
		}

		struct SectionOffset {
			uint32_t dataOffsetEnd;
			uint32_t stringOffsetEnd;
			uint32_t recordIndexEnd;
		};

		const S& _schema;
		const size_t _record_size;
		const DB2LoadInfo& _load_info;
		DB2Structure<F>& _structure;
		std::vector<SectionOffset> _section_offsets;
		FS* _source;
		mutable std::vector<uint8_t> _buffer;
	};

	template<TDB2Format F, TSchema S, TRecord R, Filesystem::TFileSource FS>
	class DB2LoaderSparse final : public DB2Loader<F, R> {
	public:
		DB2LoaderSparse(const S& schema, const DB2LoadInfo& load, DB2Structure<F>& structure, FS* source) : 
			_schema(schema), _load_info(load), _structure(structure), _source(source), _record_size(DB2Format::recordSizeSrc(schema))
		{}
		virtual ~DB2LoaderSparse() = default;

		void loadSection(const typename F::SectionHeader& section) override {
			_source->setPos(section.offset_records_end);
		}

		uint32_t size() const override {
			return _structure.header.record_count + _structure.copyTable.size();
		}
		
		R operator[](uint32_t index) const override {

			uint32_t lookup_index = index;
			std::optional<db2_record_id_t> replacement_id;

			if (index >= _structure.header.record_count) {
				const auto& copy_entry = _structure.copyTable[index - _structure.header.record_count];
				auto id_it = std::ranges::find(_structure.idList, copy_entry.id_of_copied_row);

				if (id_it == _structure.idList.end()) {
					throw WDBReaderException("Copy table id doesnt exist.");
				}

				lookup_index = std::distance(_structure.idList.begin(), id_it);
				replacement_id = copy_entry.id_of_new_row;
			}

			const auto section_index = getSectionIndex(lookup_index);
			const bool is_encypted_section = _structure.sectionHeaders[section_index].tact_key_hash != 0;

			_source->setPos(_structure.offsetMap[lookup_index].offset);

			const auto buffer_size = _structure.offsetMap[lookup_index].size;
			ptrdiff_t buffer_offset = 0;

			R record;
			record.recordIndex = index;
			record.encryptionState = RecordEncryption::NONE;
			
			_buffer.resize(buffer_size);
			std::fill(_buffer.begin(), _buffer.end(), 0);

			if (is_encypted_section && buffer_size == 0) {
				record.encryptionState = RecordEncryption::ENCRYPTED;
			}
			else {
				_source->read(_buffer.data(), buffer_size);
			}

			if (is_encypted_section) {
				bool record_encrypted = record.encryptionState == RecordEncryption::ENCRYPTED;
				if (!record_encrypted) {
					record_encrypted = std::all_of(_buffer.begin(), _buffer.end(), [](auto i) { return i == 0; });
					record.encryptionState = record_encrypted ? RecordEncryption::ENCRYPTED : RecordEncryption::DECRYPTED;
				}

				if (record_encrypted) {
					return record;
				}
			}

			R::make(&record, _schema.elementCount(), _record_size);

			uint32_t schema_field_index = 0;
			ptrdiff_t view_offset = 0;

			if (_load_info.useIdList) {
				R::insertField(&record, schema_field_index, 1, view_offset);
				db2_record_id_t use_id = replacement_id.has_value() ? replacement_id.value() : _structure.idList[lookup_index];
				R::insertValue(&record, schema_field_index++, 0, view_offset, use_id);
				view_offset += sizeof(uint32_t);
			}

			for (uint32_t x = 0; x < _structure.header.field_count; x++) {
				const Field& schema_field = _schema.fields()[schema_field_index];
				assert(schema_field.annotation.isInline);

				R::insertField(&record, schema_field_index, schema_field.size, view_offset);

				if (schema_field.annotation.isId && replacement_id.has_value()) {
					assert(!schema_field.isArray());
					assert(schema_field.bytes == sizeof(db2_record_id_t));
					R::insertValue(&record,
						schema_field_index,
						0,
						view_offset,
						replacement_id.value()
					);
					view_offset += sizeof(db2_record_id_t);
				}
				else {
					for (auto z = 0; z < schema_field.size; z++) {
						schemaFieldHandler(schema_field, [&]<typename T>() {
							if constexpr (std::is_same_v<string_data_t, T>) {
								std::string_view str_view((char*)(_buffer.data() + buffer_offset));
								const auto str_bytes_size = str_view.size() + 1; // add null terminator.
								buffer_offset += str_bytes_size;

								T result = std::make_unique_for_overwrite<typename T::element_type[]>(str_bytes_size);
								memcpy(result.get(), str_view.data(), str_bytes_size);

								R::insertValue(&record, schema_field_index, z, view_offset, std::move(result));
								view_offset += sizeof(T);
							}
							else {
								R::insertValue(&record,
									schema_field_index,
									z,
									view_offset,
									getRecordFieldValue<T>(_buffer.data(), x, z, &buffer_offset)
								);
								view_offset += sizeof(T);
							}
						});
					}
				}

				schema_field_index++;
			}

			assert(schema_field_index == _schema.fields().size());

			return record;
		}

	protected:

		template<typename T>
		inline T getRecordFieldValue(uint8_t* buff, uint32_t field_index, uint32_t array_index, ptrdiff_t* offset) const {

			const F::FieldStorageInfo& field_info = _structure.fieldStorage[field_index];

			switch (field_info.compression_type) {
			case DB2FieldCompression::None:
			{
				const auto bit_offset_remainder = field_info.field_offset_bits % 8;
				assert(bit_offset_remainder == 0);
				const auto result = *reinterpret_cast<T const*>(buff + *offset);
				*offset += sizeof(T);
				return result;
			}
			break;
			default:
				throw WDBReaderException("Unhandled field compression type.");
			break;
			}

			return T(0);
		}

		uint32_t getSectionIndex(uint32_t record_index) const {
			uint32_t section = 0;
			for (; section < _structure.header.section_count; ++section)
			{
				const F::SectionHeader& sectionHeader = _structure.sectionHeaders[section];
				if (record_index < sectionHeader.offset_map_id_count)
					break;

				record_index -= sectionHeader.offset_map_id_count;
			}

			return section;
		}

		const S& _schema;
		const size_t _record_size;
		const DB2LoadInfo& _load_info;
		DB2Structure<F>& _structure;
		FS* _source;
		mutable std::vector<uint8_t> _buffer;
	};

	template<TDB2Format F, TSchema S, TRecord R, Filesystem::TFileSource FS>
	class DB2File final : public DataSource<R> {
	public:
		DB2File(const S& schema) : _format(F()), _schema(schema), _load_info(DB2LoadInfo::make(schema)) {}
		virtual ~DB2File() = default;

		void open(std::unique_ptr<FS> source) {
			_file_source = std::move(source);

			_file_source->read(&_structure.header, sizeof(_structure.header));

			if (_structure.header.signature != F::signature.integer) {
				throw WDBReaderException("Header signature doesnt match.");
			}

			if (_structure.header.lookup_column_count > 1) {
				throw WDBReaderException("Unexpected number of relation columns.");
			}

			const auto inline_field_count = std::ranges::count_if(_schema.fields(), [](const Field& field) constexpr {
				return field.annotation.isInline;
			});

			if (inline_field_count != _structure.header.field_count) {
				throw WDBReaderException("Schema field count doesnt match structure.");
			}

			_structure.sectionHeaders = std::make_unique_for_overwrite<typename F::SectionHeader[]>(_structure.header.section_count);
			_file_source->read(_structure.sectionHeaders.get(), sizeof(F::SectionHeader) * _structure.header.section_count);
			

			_structure.fieldStructures = std::make_unique_for_overwrite<typename F::FieldStructure[]>(_structure.header.field_count);
			_file_source->read(_structure.fieldStructures.get(), sizeof(F::FieldStructure) * _structure.header.field_count);

			if (_structure.header.field_storage_info_size > 0) {
				_structure.fieldStorage = std::make_unique_for_overwrite<typename F::FieldStorageInfo[]>(_structure.header.total_field_count);
				_file_source->read(_structure.fieldStorage.get(), sizeof(F::FieldStorageInfo) * _structure.header.total_field_count);
			}

			_structure.indexedPalletData = std::make_unique_for_overwrite<std::unique_ptr<typename F::PalletValue[]>[]>(_structure.header.total_field_count);
			if (_structure.header.pallet_data_size > 0) {
				for (uint32_t i = 0; i < _structure.header.total_field_count; ++i) {
					const auto compression = _structure.fieldStorage[i].compression_type;
					if (compression == DB2FieldCompression::BitpackedIndexed ||
						compression == DB2FieldCompression::BitpackedIndexedArray) {
						_structure.indexedPalletData[i] = std::make_unique_for_overwrite<typename F::PalletValue[]>(_structure.fieldStorage[i].additional_data_size / sizeof(F::PalletValue));
						_file_source->read(_structure.indexedPalletData[i].get(), _structure.fieldStorage[i].additional_data_size);
					}
				}
			}

			_structure.indexedCommonData = std::make_unique_for_overwrite<std::unique_ptr<typename F::CommonValue[]>[]>(_structure.header.total_field_count);
			_structure.commonData = std::make_unique_for_overwrite<std::unordered_map<typename F::CommonValue::id_t, typename F::CommonValue::value_t>[]>(_structure.header.total_field_count);
			if (_structure.header.common_data_size > 0) {
				for (uint32_t i = 0; i < _structure.header.total_field_count; ++i) {
					if (_structure.fieldStorage[i].compression_type == DB2FieldCompression::CommonData &&
						_structure.fieldStorage[i].additional_data_size > 0) {

						const auto common_count = _structure.fieldStorage[i].additional_data_size / sizeof(F::CommonValue);

						_structure.indexedCommonData[i] = std::make_unique_for_overwrite<typename F::CommonValue[]>(common_count);
						_file_source->read(_structure.indexedCommonData[i].get(), _structure.fieldStorage[i].additional_data_size);

						for (auto addition = 0; addition < common_count; addition++) {
							const auto& record = _structure.indexedCommonData[i][addition];
							_structure.commonData[i][record.record_id] = record.value;
						}
					}
				}
			}

			if (isSparse()) {
				_loader = std::make_unique<DB2LoaderSparse<F, S, R, FS>>(_schema, _load_info, _structure, _file_source.get());
			}
			else [[ likely ]] {
				_loader = std::make_unique<DB2LoaderStandard<F, S, R, FS>>(_schema, _load_info, _structure, _file_source.get());
			}
		}

		void load() {
			
			if (_load_info.useIdList && _structure.header.record_count > 0) {
				_structure.idList.reserve(_structure.header.record_count);
			}

			for (uint32_t i = 0; i < _structure.header.section_count; i++) {
				const typename F::SectionHeader& section = _structure.sectionHeaders[i];

				if (section.tact_key_hash != 0) {
					//...
				}

				if (!_load_info.useIdList) {
					if (section.id_list_size > 0) {
						throw std::runtime_error("Unexpected id list found.");
					}
				}
				else {
					if(section.id_list_size != section.record_count * sizeof(uint32_t)) {
						throw std::runtime_error("Unexpected id list size found.");
					}
				}

				_file_source->setPos(section.file_offset);
				_loader->loadSection(section);

				if (section.tact_key_hash != 0) {
					//...
				}

				if (section.id_list_size) {
					const auto old_id_list_size = _structure.idList.size();
					_structure.idList.resize(old_id_list_size + (section.id_list_size / sizeof(uint32_t)));
					_file_source->read(&_structure.idList[old_id_list_size], section.id_list_size);
				}

				if (section.copy_table_count > 0) {
					const auto old_copy_table_size = _structure.copyTable.size();
					_structure.copyTable.resize(old_copy_table_size + section.copy_table_count);
					_file_source->read(&_structure.copyTable[old_copy_table_size], section.copy_table_count * sizeof(F::CopyTableEntry));
				}

				if (section.offset_map_id_count > 0) {
					const auto old_offset_map_size = _structure.offsetMap.size();
					_structure.offsetMap.resize(old_offset_map_size + section.offset_map_id_count);
					_file_source->read(&_structure.offsetMap[old_offset_map_size], section.offset_map_id_count * sizeof(F::OffsetMapEntry));
				}

				if (hasSecondaryKeys()) {
					_loadOffsetMapIds(section);
					_loadRelationships(section);
				}
				else {
					_loadRelationships(section);
					_loadOffsetMapIds(section);
				}
			}

			_structure.relationshipMap.reserve(_structure.relationships.size());
			for (const auto& relation : _structure.relationships) {
				//assert(!_structure.relationshipMap.contains(relation.record_index));	//TODO this doesnt work with encrypted sections, where its filled with zero.
				_structure.relationshipMap.emplace(relation.record_index, relation.foreign_id);
			}

			_structure.indexedCommonData.reset();
			_structure.relationships.clear();

#ifdef _DEBUG
			if (_load_info.useIdList) {
				assert(_structure.header.record_count == _structure.idList.size());
			}
#endif
		}

		size_t size() const override {
			assert(_loader);
			return _loader->size();
		}

		R operator[](uint32_t index) const override {
			assert(_loader);
			return (*_loader)[index];
		}

		inline bool hasSecondaryKeys() const {
			return (_structure.header.flags & DB2HeaderFlags::HasRelationshipData) != 0;
		}

		inline bool isSparse() const {
			return (_structure.header.flags & DB2HeaderFlags::HasOffsetMap) != 0;
		}

	protected:
		inline void _loadOffsetMapIds(const typename F::SectionHeader& section) {
			if (section.offset_map_id_count > 0) {
				const auto old_offset_map_ids_size = _structure.offsetMapIds.size();
				_structure.offsetMapIds.resize(old_offset_map_ids_size + section.offset_map_id_count);
				_file_source->read(&_structure.offsetMapIds[old_offset_map_ids_size], section.offset_map_id_count * sizeof(db2_record_id_t));
			}
		}

		inline void _loadRelationships(const typename F::SectionHeader& section){
			if (section.relationship_data_size > 0) {

				struct {
					uint32_t count;
					uint32_t min_id;
					uint32_t max_id;
				} relation_header;

				_file_source->read(&relation_header, sizeof(relation_header));

				if (relation_header.count > 0) {
					const auto old_relations_size = _structure.relationships.size();
					_structure.relationships.resize(old_relations_size + relation_header.count);
					_file_source->read(&_structure.relationships[old_relations_size], relation_header.count * sizeof(F::RelationshipEntry));
				}
			}
		}

		const F _format;
		const S _schema;
		const DB2LoadInfo _load_info;
		std::unique_ptr<FS> _file_source;
		DB2Structure<typename F> _structure;
		std::unique_ptr<DB2Loader<typename F, typename R>> _loader;
	};


	template<TSchema S, TRecord R, Filesystem::TFileSource FS, TDB2Format... F>
	std::unique_ptr<DataSource<R>> makeDB2FileFormat(const S& schema, std::unique_ptr<FS> source) {
		Signature sig;
		source->read(&sig.integer, sizeof(sig.integer));
		source->setPos(0);

		std::unique_ptr<DataSource<R>> result = nullptr;

		auto try_format = [&]<TDB2Format Fmt>() -> int {
			if (result == nullptr) {
				if (Fmt::signature.integer == sig.integer) {
					auto res = std::make_unique<DB2File<Fmt, S, R, FS>>(schema);
					res->open(std::move(source));
					res->load();
					result = std::move(res);
					return 0;
				}
			}

			return 1;
		};

		(try_format.operator()<F>() && ...);

		return result;
	}

	template<TSchema S, TRecord R, Filesystem::TFileSource FS>
	std::unique_ptr<DataSource<R>> makeDB2File(const S& schema, std::unique_ptr<FS> source) {
		return makeDB2FileFormat<S, R, FS, DB2FormatWDC3, DB2FormatWDC4, DB2FormatWDC5>(schema, std::move(source));
	};

	template<Filesystem::TFileSource FS>
	auto makeDB2File(const RuntimeSchema& schema, std::unique_ptr<FS> source) {
		return makeDB2File<RuntimeSchema, RuntimeRecord, FS>(schema, std::move(source));
	}
	
	template<TRecord R, Filesystem::TFileSource FS>
	auto makeDB2File(std::unique_ptr<FS> source) {
		return makeDB2File<decltype(R::schema), R, FS>(R::schema, std::move(source));
	}



}