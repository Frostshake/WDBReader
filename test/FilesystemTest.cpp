#include <catch2/catch_test_macros.hpp> 

#include <WDBReader/Filesystem.hpp>
#include <WDBReader/Filesystem/NativeFilesystem.hpp>
#include <WDBReader/Utility.hpp>
#include <filesystem>
#include <fstream>

using namespace WDBReader::Filesystem;
using namespace WDBReader::Utility;

TEST_CASE("Native filesystem can be read.", "[filesystem]")
{
    const auto temp_file_name = std::filesystem::temp_directory_path() / "wdbreader_test.txt";
    auto file_guard = ScopeGuard([&temp_file_name]() {
        if (std::filesystem::exists(temp_file_name)) {
            std::filesystem::remove(temp_file_name);
        }
    });


    const std::string msg = "Hello world.";
    std::ofstream writer(temp_file_name, std::ios::binary);
    writer << msg;
    writer.close();

    NativeFilesystem native_fs;
    auto native_source = native_fs.open(temp_file_name);
    
    REQUIRE(native_source->size() == msg.size());
    REQUIRE(native_source->getPos() == 0);

    std::string out;
    out.resize(msg.size());
    native_source->read(out.data(), out.size());

    REQUIRE(out == msg);
}

#ifdef TESTING_CASC_DIR
#include <WDBReader/Filesystem/CASCFilesystem.hpp>

TEST_CASE("CASC filesystem can be read.", "[filesystem]")
{
    auto casc_fs = CASCFilesystem(TESTING_CASC_DIR, CASC_LOCALE_ENUS);
    auto source = casc_fs.open(1572924u); //itemsparse.db2

    REQUIRE(source != nullptr);
    REQUIRE(source->size() > 0);

}
#endif

#ifdef TESTING_MPQ_DIR
#include <WDBReader/Filesystem/MPQFilesystem.hpp>

TEST_CASE("MPQ filesystem can be read.", "[filesystem]")
{
    std::vector<std::string> mpq_names;
    const std::filesystem::path mpq_root = TESTING_MPQ_DIR;

    for(const auto& entry : std::filesystem::recursive_directory_iterator(mpq_root)) {
        if (entry.is_regular_file()) {
            const auto extension = entry.path().extension().string();
            if (extension == ".mpq" || extension == ".MPQ") {
                mpq_names.push_back(entry.path().lexically_relative(mpq_root).generic_string());
            }
        }
    }

    REQUIRE(mpq_names.size() > 0);

    auto mpq_fs = MPQFilesystem(mpq_root, std::move(mpq_names));
    auto list_file_src = mpq_fs.open("(listfile)");

    REQUIRE(list_file_src != nullptr);
    REQUIRE(list_file_src->size() > 0);

}
#endif