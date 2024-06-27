#pragma once

#include "../Filesystem.hpp"
#include <CascLib.h>
#include <cstdint>
#include <filesystem>
#include <memory>

namespace WDBReader::Filesystem {

    DWORD CASCLocaleConvert(const std::string& locale);

	using CASCFileUri = uint32_t;
	static_assert(TFileUri<CASCFileUri>);

    class CASCFileSource {
	public:
		CASCFileSource(HANDLE handle) :
			_casc_file(handle), _pos(0)
		{
			const auto result = CascGetFileSize64(_casc_file.get(), &_size);
			if (!result) {
				throw WDBReaderException("Error getting CASC file size.", GetLastError());
			}
		}

		size_t size() const {
			return _size;
		}
		void read(void* dest, uint64_t bytes);
		void setPos(uint64_t position);
		uint64_t getPos() const 
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

}