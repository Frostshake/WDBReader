#pragma once

#include "../Filesystem.hpp"
#include <filesystem>
#include <fstream>
#include <memory>

namespace WDBReader::Filesystem
{

    using NativeFileUri = std::filesystem::path;
    static_assert(TFileUri<NativeFileUri>);

    class NativeFileSource final : public FileSource
    {
    public:
        NativeFileSource(std::ifstream&& stream) :
            _stream(std::move(stream))
        {
            const auto existing_pos = getPos();
            _stream.seekg(0, std::ios::end);
            _size = _stream.tellg();
            setPos(existing_pos);
        };
        NativeFileSource(NativeFileSource&&) = default;

        size_t size() const override
        {
            return _size;
        }

        void read(void* dest, uint64_t bytes) override
        {
            _stream.read((char*)dest, bytes);
            if(_stream.bad() || _stream.fail()) {
                throw WDBReaderException("Error reading file.");
            }
        }

        void setPos(uint64_t position) override
        {
            _stream.clear();
            _stream.seekg(position, std::ios::beg);
        }

        uint64_t getPos() const override
        {
            return const_cast<std::ifstream*>(&_stream)->tellg();
        }

    protected:
        NativeFileSource(const NativeFileSource&) = delete;

        std::ifstream _stream;
        size_t _size;
        friend class NativeFilesystem;
    };

    static_assert(TFileSource<NativeFileSource>);

    class NativeFilesystem
    {
    public:
        std::unique_ptr<NativeFileSource> open(const NativeFileUri& uri)
        {
            return std::make_unique<NativeFileSource>(std::ifstream(uri, std::ifstream::binary));
        }
    };

    static_assert(TFilesystem<NativeFilesystem, NativeFileUri, NativeFileSource>);
}