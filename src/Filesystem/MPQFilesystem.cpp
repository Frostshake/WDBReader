#include "WDBReader/Filesystem/MPQFilesystem.hpp"
#include "WDBReader/Utility.hpp"

namespace WDBReader::Filesystem {

	void MPQFileSource::read(void* dest, uint64_t bytes)
	{
		DWORD res = 0;
		const auto result = SFileReadFile(_mpq_file.get(), dest, bytes, &res, NULL);
		_pos += res;
		if (!result)
		{
			throw WDBReaderException("Error reading MPQ file.", GetLastError());
		}

		assert(bytes == res);
	}

	void MPQFileSource::setPos(uint64_t position)
	{
		const auto result = SFileSetFilePointer(_mpq_file.get(), position, NULL, FILE_BEGIN);
		if (result == SFILE_INVALID_SIZE)
		{
			throw WDBReaderException("Error setting position of MPQ file.", GetLastError());
		}
		else
		{
			_pos = position;
		}
	}

	MPQFilesystem::MPQFilesystem(const std::filesystem::path& root, std::vector<std::string>&& names) 
	{
		_archives.reserve(names.size());

		for (const auto& name : names) {
			HANDLE temp;
			if (SFileOpenArchive((root / name).string().c_str(), 0, MPQ_OPEN_FORCE_MPQ_V1 | MPQ_OPEN_READ_ONLY, &temp)) {
				_archives.emplace_back(name, temp);
			}
			else {
				throw WDBReaderException(std::string("Unable to open MPQ file -") + name, GetLastError());
			}
		}
	}

	std::unique_ptr<MPQFileSource> MPQFilesystem::open(MPQFileUri uri)
	{
		HANDLE temp;
		for (const auto& mpq : _archives) {
			if (SFileOpenFileEx(mpq.second.get(), uri.c_str(), SFILE_OPEN_FROM_MPQ, &temp)) {
				return std::make_unique<MPQFileSource>(temp);
			}
		}

		return nullptr;
	}

	std::vector<std::string> discoverMPQArchives(const std::filesystem::path& root)
	{
		std::vector<std::filesystem::path> result;

		for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
			if (entry.is_regular_file()) {
				const auto extension = entry.path().extension().string();
				if (extension == ".mpq" || extension == ".MPQ") {
					result.push_back(entry.path().lexically_relative(root).generic_string());
				}
			}
		}

		std::ranges::sort(result.begin(), result.end(), [](const std::filesystem::path& left, const std::filesystem::path& right) {
			const auto& left_name = left.stem().native();
			const auto& right_name = right.stem().native();

			const auto& left_path = left.parent_path().native();
			const auto& right_path = right.parent_path().native();

			if (left_path == right_path) {

				if (left_name.starts_with(right_name)) {
					return true;
				}
				else if (right_name.starts_with(left_name)) {
					return false;
				}

				if (left_name == right_name) {
					return left.string() > right.string();
				}

				return left_name > right;
			}

			return left_path > right_path;
		});

		std::vector<std::string> out;
		out.reserve(result.size());
		std::transform(result.begin(), result.end(), std::back_inserter(out), [](const auto& item) {
			return item.string();
		});

		return out;
	}

}