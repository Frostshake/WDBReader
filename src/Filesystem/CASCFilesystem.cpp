#include "WDBReader/Filesystem/CASCFilesystem.hpp"
#include "WDBReader/Utility.hpp"
#include <unordered_map>

namespace WDBReader::Filesystem {

    void CASCFileSource::read(void* dest, uint64_t bytes)
    {
        DWORD res = 0;
        const auto result = CascReadFile(_casc_file.get(), dest, bytes, &res);
        if (!result) {
			throw WDBReaderException("Error reading CASC file.", GetLastError());
        }

		// currently assuming that is reached, than eiter all bytes have been read, or its an encrypted section and zero has been read.
		// in either case, the offset still needs to be advanced the same.
		// (not encountered any other scenario so far)
		_pos += bytes;
		//_pos += res;
		//assert(bytes == res);
	}

    void CASCFileSource::setPos(uint64_t position) {
        uint64_t pos = 0;
        const auto result = CascSetFilePointer64(_casc_file.get(), position, &pos, FILE_BEGIN);
        if (!result) {
			throw WDBReaderException("Error setting position of CASC file.", GetLastError());
        }
        else {
            _pos = pos;
        }
    }

    CASCFilesystem::CASCFilesystem(const std::filesystem::path& root, DWORD locale_mask, const std::string& product) :
		_storage(nullptr), _locale_mask(locale_mask)
	{
		const std::filesystem::path::string_type casc_params = root.native() + std::filesystem::path("*" + product).native();

		HANDLE temp;
		if (!CascOpenStorage(casc_params.c_str(), _locale_mask, &temp)) {
			throw WDBReaderException("Unable to initialise casc storage.", GetLastError());
		}
		_storage.reset(temp);
	}

	std::unique_ptr<CASCFileSource> CASCFilesystem::open(CASCFileUri uri)
	{
		HANDLE temp;
		if (CascOpenFile(_storage.get(), CASC_FILE_DATA_ID(uri), _locale_mask, CASC_OPEN_BY_FILEID | CASC_OVERCOME_ENCRYPTED, &temp)) {
			
			return std::make_unique<CASCFileSource>(temp);
		}
		else {
			auto error = GetLastError();
			if (error > 0) {
				throw WDBReaderException("Unable to open CASC file", error);
			}
		}
			
		return nullptr;
	}

}