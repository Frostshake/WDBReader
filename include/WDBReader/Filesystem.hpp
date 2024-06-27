#pragma once

#include "Utility.hpp"
#include <cassert>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace WDBReader::Filesystem {

	template <typename T>
	concept TFileUri = true;

	template<typename T>
	concept TFileSource = requires(T t) {
		{ t.size() } -> std::same_as<size_t>;
		{ t.read(std::declval<void*>(), uint64_t()) } -> std::same_as<void>;
		{ t.setPos(uint64_t()) } -> std::same_as<void>;
		{ t.getPos() } -> std::same_as<uint64_t>;
	};

	template<typename T, typename FU, typename FS>
	concept TFilesystem = requires(T t) {
		TFileUri<FU>;
		TFileSource<FS>;
		{ t.open(FU()) } -> std::same_as<std::unique_ptr<FS>>;
	};

	class MemoryFileSource {
	public:
		template<TFileSource T>
		MemoryFileSource(T& source) : _size(source.size()), _pos(0)
		{
			assert(source.getPos() == 0);
			_data = std::make_unique_for_overwrite<uint8_t[]>(_size);
			source.read(_data.get(), _size);
		}

		inline size_t size() const {
			return _size;
		}

		inline void read(void* dest, uint64_t bytes) {
			const auto available = _size - _pos;
			const auto read_bytes = std::min(bytes, available);
			if (read_bytes != bytes) {
				throw WDBReaderException("Error reading memory source.");
			}

			memcpy(dest, &_data[_pos], bytes);

			dest = &_data[_pos];
			_pos += read_bytes;
		}

		inline void setPos(uint64_t position) {
			_pos = position;
			assert(_pos < _size);
		}

		inline uint64_t getPos() const {
			return _pos;
		}

	private:
		std::unique_ptr<uint8_t[]> _data;
		const uint64_t _size;
		uint64_t _pos;
	};

	static_assert(TFileSource<MemoryFileSource>);

}