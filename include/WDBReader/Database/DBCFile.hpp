#pragma once

#include "../Database.hpp"
#include "../Utility.hpp"
#include "Formats.hpp"
#include <cassert>
#include <cstdint>
#include <memory>
#include <span>

namespace WDBReader::Database {

#pragma pack(push, 1)
	struct DBCHeader {
		uint32_t signature;
		uint32_t recordCount;
		uint32_t fieldCount;
		uint32_t recordSize;
		uint32_t stringBlockSize;
	};

	enum class DBCVersion {
		VANILLA,	// Vanilla
		BC_WOTLK,	// BC & WOLTK	(2.1.0.6692)
		CATA_PLUS	// Cata and above
	};

	inline constexpr DBCVersion getDBCVersion(const GameVersion& version) {
		constexpr auto v1_cutoff = GameVersion(2, 1, 0, 6692);
		constexpr auto v2_cutoff = GameVersion(4, 0, 0, 0);	// not sure of the build number, but this is accurate enough

		if (version < v2_cutoff) {
			if (version < v1_cutoff) {
				return DBCVersion::VANILLA;
			}
			else {
				return DBCVersion::BC_WOTLK;
			}
		}

		return DBCVersion::CATA_PLUS;
	}

	enum class DBCStringLocale : uint8_t {
		enUS = 0,
		koKR,
		frFR,
		deDE,
		zhCN,
		zhTW,
		esES,
		esMX,
		ruRU,
		jaJP,
		ptPT,
		itIT,
		unknown_12,
		unknown_13,
		unknown_14,
		unknown_15,
		SIZE,
		VANILLA_SIZE = esMX + 1,
		BC_WOTLK_SIZE = SIZE,
		ANY = enUS // placeholder value for when locale options arent relevant.
	};

	inline constexpr DBCStringLocale DBCLocaleConvert(const std::string& locale) 
	{
		if (locale == "enUS") {
			return DBCStringLocale::enUS;
		}
		else if (locale == "koKR") {
			return DBCStringLocale::koKR;
		}
		else if (locale == "frFR") {
			return DBCStringLocale::frFR;
		}
		else if (locale == "deDE") {
			return DBCStringLocale::deDE;
		}
		else if (locale == "zhCN") {
			return DBCStringLocale::zhCN;
		}
		else if (locale == "zhTW") {
			return DBCStringLocale::zhTW;
		}
		else if (locale == "esES") {
			return DBCStringLocale::esES;
		}
		else if (locale == "esMX") {
			return DBCStringLocale::esMX;
		}
		else if (locale == "ruRU") {
			return DBCStringLocale::ruRU;
		}
		else if (locale == "jaJP") {
			return DBCStringLocale::jaJP;
		}
		else if (locale == "ptPT") {
			return DBCStringLocale::ptPT;
		}
		else if (locale == "itIT") {
			return DBCStringLocale::itIT;
		}

		throw std::invalid_argument("Unknown DBC locale.");
	}

	template<DBCVersion VER, typename StrT = string_data_t>
	struct DBCLangString {
		consteval static size_t stringCount() {
			switch (VER) {
			case DBCVersion::VANILLA:
				return (size_t)DBCStringLocale::VANILLA_SIZE;
			case DBCVersion::BC_WOTLK:
				return (size_t)DBCStringLocale::BC_WOTLK_SIZE;
			default:
				return 1;
			}
		}

		StrT strings[stringCount()];

		uint32_t flags;
	};

	template<typename StrT>
	struct DBCLangString<DBCVersion::CATA_PLUS, StrT> {
		StrT strings[1];
	};

	
	class DBCFormat {
	public:
		template<TSchema S>
		static constexpr size_t recordSizeDest(const S& schema, DBCVersion version) {
			return recordSize<string_data_t>(schema, version);
		}

		template<TSchema S>
		static constexpr size_t recordSizeSrc(const S& schema, DBCVersion version) {
			return recordSize<string_ref_t>(schema, version);
		}

		template<TSchema S>
		static constexpr size_t elementCountSrc(const S& schema, DBCVersion version) {
			size_t sum = 0;

			for (const auto& field : schema.fields()) {
				if (field.type == Field::Type::LANG_STRING) {

					size_t field_size;
					switch (version) {
					case DBCVersion::VANILLA:
						field_size = static_cast<size_t>(DBCStringLocale::VANILLA_SIZE) + 1;
						break;
					case DBCVersion::BC_WOTLK:
						field_size = static_cast<size_t>(DBCStringLocale::BC_WOTLK_SIZE) + 1;
						break;
					case DBCVersion::CATA_PLUS:
						field_size = 1;
						break;
					default:
						throw std::logic_error("Unknown dbc version.");
					}

					sum += field_size;
					continue;
				}


				sum += field.size;
			}

			return sum;
		}

	private:

		template<typename StrT, TSchema S>
		static constexpr size_t recordSize(const S& schema, DBCVersion version) {
			size_t sum = 0;

			for (const auto& field : schema.fields()) {
				if (field.type == Field::Type::LANG_STRING) {

					size_t field_size;
					switch (version) {
					case DBCVersion::VANILLA:
						field_size = sizeof(DBCLangString<DBCVersion::VANILLA, StrT>);
						break;
					case DBCVersion::BC_WOTLK:
						field_size = sizeof(DBCLangString<DBCVersion::BC_WOTLK, StrT>);
						break;
					case DBCVersion::CATA_PLUS:
						field_size = sizeof(DBCLangString<DBCVersion::CATA_PLUS, StrT>);
						break;
					default:
						throw std::logic_error("Unknown dbc version.");
					}
					
					sum += (field_size * field.size);
					continue;
				}
				else if (field.type == Field::Type::STRING) {
					sum += (sizeof(StrT) * field.size);
					continue;
				}

				sum += field.totalBytes();
			}

			return sum;
		}


		DBCFormat() = delete;
	};

#pragma pack(pop)

	template<TSchema S, TRecord R, Filesystem::TFileSource FS, bool LegacyLangStrings = std::is_standard_layout_v<decltype(R::data)>>
	requires (LegacyLangStrings == false || (LegacyLangStrings == true && std::is_standard_layout_v<decltype(R::data)>))
	class DBCFile final : public DataSource<R> {
	public:

		template<typename = typename std::enable_if_t<LegacyLangStrings>>
		DBCFile(const S& schema, DBCVersion version) :
			_schema(schema), _version(version), _locale(DBCStringLocale::ANY), _record_size(DBCFormat::recordSizeSrc(schema, version))
		{}

		template<typename = typename std::enable_if_t<!LegacyLangStrings>>
		DBCFile(const S& schema, DBCVersion version, DBCStringLocale locale = DBCStringLocale::ANY) :
			_schema(schema), _version(version), _locale(locale), _record_size(DBCFormat::recordSizeSrc(schema, version))
		{

			if (version == DBCVersion::VANILLA) {
				if ((uint8_t)locale >= (uint8_t)DBCStringLocale::VANILLA_SIZE) {
					throw std::logic_error("Invalid dbc locale.");
				}
			}
			else if (version == DBCVersion::BC_WOTLK) {
				if ((uint8_t)locale >= (uint8_t)DBCStringLocale::BC_WOTLK_SIZE) {
					throw std::logic_error("Invalid dbc locale.");
				}
			}

#ifdef _DEBUG
			if (version == DBCVersion::CATA_PLUS) {
				assert(locale == DBCStringLocale::ANY);
			}
#endif
		}

        virtual ~DBCFile() = default;
		
		void open(std::unique_ptr<FS> source) {
			_file_source = std::move(source);
			
			_file_source->read(&_header, sizeof(_header));
			std::vector<uint8_t> data;
			data.resize(sizeof(_header));
			memcpy(data.data(), &_header, sizeof(_header));

			if (_header.signature != DBC_MAGIC.integer) {
				throw WDBReaderException("Header signature doesnt match.");
			}

			const auto expected = DBCFormat::elementCountSrc(_schema, _version);

			if (_header.fieldCount != expected) {
				throw WDBReaderException("Schema fields doesnt match structure.");
			}

			if (_header.recordSize != _record_size) {
				throw WDBReaderException("Schema record size doesnt match structure.");
			}

			_record_buffer.resize(_header.recordSize);
		}

		void load() {}

		size_t size() const override {
			return _header.recordCount;
		}

		R operator[](uint32_t index) const override {

			uint64_t offset = sizeof(_header) + (_header.recordSize * index);
			_file_source->setPos(offset);

			std::fill(_record_buffer.begin(), _record_buffer.end(), 0);

			_file_source->read(_record_buffer.data(), _header.recordSize);
			ptrdiff_t buffer_offset = 0;

			R record;
			record.recordIndex = index;
			record.encryptionState = RecordEncryption::NONE;
			R::make(&record, _schema.elementCount(), _record_size);

			uint32_t schema_field_index = 0;
			ptrdiff_t view_offset = 0;	

			for (const auto& field : _schema.fields()) {

				R::insertField(&record, schema_field_index, field.size, view_offset);

				for (auto z = 0; z < field.size; z++) {
					schemaFieldHandler(field, [&]<typename T>() {
						if constexpr (std::is_same_v<string_data_t, T>) {

							if (field.type == Field::Type::STRING || (field.type == Field::Type::LANG_STRING && _version == DBCVersion::CATA_PLUS)) {
								auto string_ref = *reinterpret_cast<string_ref_t*>(_record_buffer.data() + buffer_offset);
								_file_source->setPos(sizeof(DBCHeader) + (_header.recordSize * _header.recordCount) + string_ref);;

								R::insertValue(
									&record,
									schema_field_index,
									z,
									view_offset,
									std::move(readCurrentString(_file_source.get()))
								);


								buffer_offset += sizeof(string_ref_t);
								view_offset += sizeof(T);
							}
							else if(field.type == Field::Type::LANG_STRING) {
								const size_t strings_size = (size_t)(_version == DBCVersion::VANILLA ? DBCStringLocale::VANILLA_SIZE : DBCStringLocale::BC_WOTLK_SIZE);
								const ptrdiff_t strings_view_size = strings_size * sizeof(lang_string_ref_t);

								std::span<lang_string_ref_t> string_ref_span((lang_string_ref_t*)(_record_buffer.data() + buffer_offset), strings_size);


								if constexpr (LegacyLangStrings) {

									auto array_block = ((strings_size - 1) + 1) * z; // string indexes + flags;

									size_t idx = 0;
									for (auto& str_ref : string_ref_span) {
										_file_source->setPos(sizeof(DBCHeader) + (_header.recordSize * _header.recordCount) + str_ref);

										R::insertValue(&record,
											schema_field_index,
											array_block + idx,
											view_offset,
											std::move(readCurrentString(_file_source.get()))
										);
										

										view_offset += sizeof(T);
										idx++;
									}

									buffer_offset += strings_view_size;

									R::insertValue(&record,
										schema_field_index,
										array_block + idx,
										view_offset,
										*reinterpret_cast<uint32_t*>(_record_buffer.data() + buffer_offset)
									);

									buffer_offset += sizeof(uint32_t);
									view_offset += sizeof(uint32_t);
								}
								else {
									auto& str_ref = string_ref_span[(uint32_t)_locale];
									_file_source->setPos(sizeof(DBCHeader) + (_header.recordSize * _header.recordCount) + str_ref);
									
									R::insertValue(&record,
										schema_field_index,
										z,
										view_offset,
										std::move(readCurrentString(_file_source.get()))
									);

									buffer_offset += strings_view_size;
									buffer_offset += sizeof(uint32_t);
									view_offset += sizeof(T);
								}
							}

						}
						else {
							R::insertValue(&record,
								schema_field_index,
								z,
								view_offset,
								*reinterpret_cast<T*>(_record_buffer.data() + buffer_offset)
							);

							buffer_offset += sizeof(T);
							view_offset += sizeof(T);
						}
					});
				}

				schema_field_index++;
			}

			return record;
		}

	protected:
		const S _schema;
		const size_t _record_size;
		const DBCVersion _version;
		const DBCStringLocale _locale;
		std::unique_ptr<FS> _file_source;
		DBCHeader _header;

		mutable std::vector<uint8_t> _record_buffer;
	};


	template<Filesystem::TFileSource FS>
	auto makeDBCFile(const RuntimeSchema& schema, DBCVersion version, DBCStringLocale locale) {
		return DBCFile<RuntimeSchema, RuntimeRecord, FS, false>(schema, version, locale);
	}

	template<TRecord R, Filesystem::TFileSource FS>
	auto makeDBCFile(DBCVersion version) {
		return DBCFile<decltype(R::schema), R, FS, true>(R::schema, version);
	}

}