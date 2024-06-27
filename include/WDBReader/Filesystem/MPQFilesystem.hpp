#pragma once

#include "../Filesystem.hpp"
#include <StormLib.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace WDBReader::Filesystem
{
	using MPQFileUri = std::string;
	static_assert(TFileUri<MPQFileUri>);

	class MPQFileSource
	{
	public:
		MPQFileSource(HANDLE handle) :
			_mpq_file(handle), _pos(0)
		{
			_size = SFileGetFileSize(_mpq_file.get(), 0);
		}

		size_t size() const
		{
			return _size;
		}
		void read(void* dest, uint64_t bytes);
		void setPos(uint64_t position);
		uint64_t getPos() const
		{
			return _pos;
		}

	protected:
		struct mpq_file_deleter
		{
			void operator()(HANDLE handle) noexcept
			{
				SFileCloseFile(handle);
			}
		};

		std::unique_ptr<std::remove_pointer_t<HANDLE>, mpq_file_deleter> _mpq_file;
		uint64_t _pos;
		size_t _size;
	};

	static_assert(TFileSource<MPQFileSource>);

	class MPQFilesystem
	{
	public:
		MPQFilesystem(const std::filesystem::path& root, std::vector<std::string>&& names);
		std::unique_ptr<MPQFileSource> open(MPQFileUri uri);

		const std::vector<std::pair<std::string, HANDLE>>& getHandles() const {
			return *reinterpret_cast<const std::vector<std::pair<std::string, HANDLE>>*>(&_archives);
		}

	protected:
		struct mpq_archive_deleter
		{
			void operator()(HANDLE handle) noexcept
			{
				SFileCloseArchive(handle);
			}
		};

		using archive_handle_t = std::unique_ptr<std::remove_pointer_t<HANDLE>, mpq_archive_deleter>;
		std::vector<std::pair<std::string, archive_handle_t>> _archives;
	};

	static_assert(TFilesystem<MPQFilesystem, MPQFileUri, MPQFileSource>);

}