#include "WDBReader/Filesystem/MPQFilesystem.hpp"
#include "WDBReader/Utility.hpp"
#include <array>
#include <string_view>

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
			if (SFileOpenArchive((root / name).native().c_str(), 0, MPQ_OPEN_FORCE_MPQ_V1 | MPQ_OPEN_READ_ONLY, &temp)) {
				_archives.emplace_back(name, temp);
			}
			else {
				throw WDBReaderException(std::string("Unable to open MPQ file - ") + name, GetLastError());
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
		std::vector<std::pair<uint32_t, std::string>> entries;
		using string_t = std::filesystem::path::string_type;

		for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
			if (entry.is_regular_file()) {
				const auto extension = entry.path().extension().string();
				if (extension == ".mpq" || extension == ".MPQ") {
					const auto val = entry.path().lexically_relative(root);
					const auto prefix = [&]() -> string_t {
						if (std::distance(val.begin(), val.end()) > 1) {
							return val.begin()->native();
						}
						return L"";
					}();

					if (prefix == L"Cache" || prefix == L"cache") {
						continue;
					}

					int32_t rank = 0;
					// remove any locale part.
					const auto neutral_name = [&](auto name) -> string_t {
						if (prefix.size() > 0) {
							auto found = name.find(L"-" + prefix);
							if (found != string_t::npos) {
								name.erase(found, prefix.size() + 1);
							}
						}
						return name;
					}(val.stem().native());

					if (neutral_name.starts_with(L"wow-update-")) {
						//currently not working with these files (produces PTCH output).
						continue;
					}

					if (neutral_name == L"development") {
						rank = std::numeric_limits<int32_t>::max();
					}
					else {
						// locale higher rank.
						if (prefix.size() > 0) {
							rank += std::numeric_limits<int32_t>::max() / 2;
						}

						if (neutral_name.starts_with(L"common")) {
							if (neutral_name.size() > std::char_traits<string_t::value_type>::length(L"common")) {
								rank += 2000;
							}
							else {
								rank += 1000;
							}
						}
						else if (neutral_name.starts_with(L"world")) {
							if (neutral_name.size() > std::char_traits<string_t::value_type>::length(L"world")) {
								rank += 3000;
							}
							else {
								rank += 4000;
							}
						}
						else if (neutral_name.starts_with(L"lichking")) {
							rank += 6000;
						}
						else if (neutral_name.starts_with(L"expansion")) {
							if (neutral_name.size() > std::char_traits<string_t::value_type>::length(L"expansion")) {
								rank += 7000;
							}
							else {
								rank += 5000;
							}
						}
						else if (neutral_name == L"alternate") {
							rank += 8000;
						}
						else if (neutral_name.starts_with(L"patch")) {
							if (neutral_name.size() > std::char_traits<string_t::value_type>::length(L"patch")) {
								rank += 10000;
							}
							else {
								rank += 9000;
							}
						}
					}

					assert(val.native().size() > 0);
					entries.push_back({rank, val.string()});
				}
			}
		}

		std::sort(entries.begin(), entries.end(), [](const auto& l, const auto& r) { return l > r; });

		std::vector<std::string> out;
		out.reserve(entries.size());
		std::transform(entries.begin(), entries.end(), std::back_inserter(out), [](const auto& item) {
			return std::move(item.second);
		});

		return out;
	}
}