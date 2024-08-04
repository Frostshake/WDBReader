#pragma once

#include "../Filesystem.hpp"
#include <CascLib.h>
#include <cstdint>
#include <filesystem>
#include <memory>

namespace WDBReader::Filesystem {

	using CASCFileUri = uint32_t;
	static_assert(TFileUri<CASCFileUri>);

    class CASCFileSource final : public FileSource {
	public:
		CASCFileSource(HANDLE handle) :
			_casc_file(handle), _pos(0)
		{
			const auto result = CascGetFileSize64(_casc_file.get(), &_size);
			if (!result) {
				throw WDBReaderException("Error getting CASC file size.", GetLastError());
			}
		}

		size_t size() const override {
			return _size;
		}
		void read(void* dest, uint64_t bytes) override;
		void setPos(uint64_t position) override;
		uint64_t getPos() const override 
        {
			return _pos;
		}

	protected:
		struct casc_file_deleter {
			void operator() (HANDLE handle) noexcept
			{
				CascCloseFile(handle);
			}
		};

		std::unique_ptr<std::remove_pointer_t<HANDLE>, casc_file_deleter> _casc_file;
		uint64_t _pos;
		size_t _size;
	};

	static_assert(TFileSource<CASCFileSource>);

	class CASCFilesystem {
	public:
		CASCFilesystem(const std::filesystem::path& root, DWORD locale_mask, const std::string& product = "wow");
		std::unique_ptr<CASCFileSource> open(CASCFileUri uri);
		HANDLE getHandle() const {
			return _storage.get();
		}

	protected:
		struct casc_storage_deleter {
			void operator() (HANDLE handle) noexcept
			{
				CascCloseStorage(handle);
			}
		};

		DWORD _locale_mask;
		std::unique_ptr<std::remove_pointer_t<HANDLE>, casc_storage_deleter> _storage;
	};

	static_assert(TFilesystem<CASCFilesystem, CASCFileUri, CASCFileSource>);

	inline constexpr DWORD CASCLocaleConvert(const std::string& locale) {

		if (locale == "frFr") {
			return CASC_LOCALE_FRFR;
		}
		else if (locale == "deDE") {
			return CASC_LOCALE_DEDE;
		}
		else if (locale == "esES") {
			return CASC_LOCALE_ESES;
		}
		else if (locale == "esMX") {
			return CASC_LOCALE_ESMX;
		}
		else if (locale == "ptBR") {
			return CASC_LOCALE_PTBR;
		}
		else if (locale == "itIT") {
			return CASC_LOCALE_ITIT;
		}
		else if (locale == "ptPT") {
			return CASC_LOCALE_PTPT;
		}
		else if (locale == "enGB") {
			return CASC_LOCALE_ENGB;
		}
		else if (locale == "ruRU") {
			return CASC_LOCALE_RURU;
		}
		else if (locale == "enUS") {
			return CASC_LOCALE_ENUS;
		}
		else if (locale == "enCN") {
			return CASC_LOCALE_ENCN;
		}
		else if (locale == "enTW") {
			return CASC_LOCALE_ENTW;
		}
		else if (locale == "koKR") {
			return CASC_LOCALE_KOKR;
		}
		else if (locale == "zhCN") {
			return CASC_LOCALE_ZHCN;
		}
		else if (locale == "zhTW") {
			return CASC_LOCALE_ZHTW;
		}

		return CASC_LOCALE_NONE;
	}

}