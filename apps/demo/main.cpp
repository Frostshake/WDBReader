/*
    Demo application that shows basic WDBReader capabilities.

    Example usage:
    demo.exe {wow_dir} {db_file_uri} {dbd_defs_name}
    - wow_dir       = path to wow install directory.
    - db_file_uri   = path to db file, MPQ: string, CASC file id.
    - dbd_defs_name = path to .dbd file.

    The application will output:
    - Detected client versions
    - DB schema
    - first record of DB
*/

#include <WDBReader/Database/DB2File.hpp>
#include <WDBReader/Database/DBCFile.hpp>
#include <WDBReader/Detection.hpp>
#include <WDBReader/Filesystem/CASCFilesystem.hpp>
#include <WDBReader/Filesystem/MPQFilesystem.hpp>
#include <WDBReader/WoWDBDefs.hpp>

#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <ranges>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <objbase.h>

using namespace WDBReader::Database;
using namespace WDBReader::Detection;
using namespace WDBReader::Filesystem;
using namespace WDBReader::Utility;
using namespace WDBReader::WoWDBDefs;

struct AppArgs {
public:
    std::filesystem::path wow_dir;
    std::string db_file;
    std::filesystem::path definition_file;
};

class FilesystemHandler {
public:
    virtual std::unique_ptr<DataSource<RuntimeRecord>> open(const std::string& file_uri, const RuntimeSchema& schema) = 0;
};

class CASCFSHandler : public FilesystemHandler {
public:
    CASCFSHandler(const AppArgs& args, const ClientInfo& info);
    std::unique_ptr<DataSource<RuntimeRecord>> open(const std::string& file_uri, const RuntimeSchema& schema) override;
private:
    CASCFilesystem _fs;
};

class MPQFSHandler : public FilesystemHandler {
public:
    MPQFSHandler(const AppArgs& args, const ClientInfo& info, std::vector<std::string>&& mpqs);
    std::unique_ptr<DataSource<RuntimeRecord>> open(const std::string& file_uri, const RuntimeSchema& schema) override;
private:
    MPQFilesystem _fs;
    GameVersion _game_ver;
};

void printSchema(const RuntimeSchema& schema);
void printRecord(const RuntimeSchema& schema, const RuntimeRecord& record);
std::vector<std::string> searchMPQs(const std::filesystem::path& root);

int main(int argc, char* argv[]) {

    CoInitialize(nullptr);
    auto exit_guard = ::ScopeGuard([]() { 
        CoUninitialize(); 
    });

    std::cout << "### WDBReader - DB Inspect ###" << std::endl;

#ifdef _DEBUG
    for (int i = 0; i < argc; i++) {
        std::cout << i << ": " << argv[i] << std::endl;
    }
#endif

    if (argc < 4) {
        std::cerr << "Not enough parameters." << std::endl;
        std::cerr << "Expected args: {wow_dir} {file_uri} {def_name}" << std::endl;
        return 1;
    }

    AppArgs args = {
        argv[1],
        argv[2],
        argv[3]
    };

    try {
        auto found_clients = Detector::all().detect(args.wow_dir);
        if (found_clients.size() == 0) {
            std::cerr << "No WoW installations found." << std::endl;
            return 1;
        }
        else {
            std::cout << "Found " << found_clients.size() << " installs: " << std::endl;
            for (const auto& info : found_clients) {
                std::cout << "Name: '" << info.name << "', Locale: '" << info.locale << "', Version: " << info.version.toString() << std::endl;
            }
        }

        if (found_clients.size() > 1) {
            std::cout << "(Multiple installs - using first)" << std::endl;
        }
        const auto& target_client = found_clients[0];

        std::ifstream def_stream(args.definition_file);
        auto definition = DBDReader::read(def_stream);
        def_stream.close();

        auto schema = makeSchema(definition, target_client.version);
        if (!schema.has_value()) {
            std::cout << "Unable to create schema." << std::endl;
            return 1;
        }

        std::unique_ptr<FilesystemHandler> fs_handler;
        auto found_mpqs = searchMPQs(args.wow_dir / "Data");
        if (found_mpqs.size() > 0) {
            fs_handler = std::make_unique<MPQFSHandler>(args, target_client, std::move(found_mpqs));
        }
        else {
            fs_handler = std::make_unique<CASCFSHandler>(args, target_client);
        }

        std::unique_ptr<DataSource<RuntimeRecord>> data_source = fs_handler->open(args.db_file, *schema);

        if (data_source->size() > 0) {
            std::cout << "-----" << std::endl;
            printRecord(*schema, (*data_source)[0]);
        }
        
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
   
    return 0;
}

CASCFSHandler::CASCFSHandler(const AppArgs& args, const ClientInfo& info) :
    _fs(args.wow_dir, CASCLocaleConvert(info.locale), info.name)
{
    std::cout << "Using CASC Filesystem" << std::endl;
}

std::unique_ptr<DataSource<RuntimeRecord>> CASCFSHandler::open(const std::string& file_uri, const RuntimeSchema& schema) {
    auto source = _fs.open(std::stoul(file_uri));

    printSchema(schema);
    std::cout << "Src record bytes: " << DB2Format::recordSizeSrc(schema) << std::endl;
    std::cout << "Dest record bytes: " << DB2Format::recordSizeDest(schema) << std::endl;

    auto db2 = makeDB2File<RuntimeSchema, RuntimeRecord, CASCFileSource>(schema, std::move(source));

    std::cout << "DB2 (" << db2->size() << ")" << std::endl;
    return db2;
}

MPQFSHandler::MPQFSHandler(const AppArgs& args, const ClientInfo& info, std::vector<std::string>&& mpqs) :
    _fs(args.wow_dir / "Data", std::move(mpqs)), _game_ver(info.version)
{
    std::cout << "Using MPQ Filesystem" << std::endl;
}

std::unique_ptr<DataSource<RuntimeRecord>> MPQFSHandler::open(const std::string& file_uri, const RuntimeSchema& schema) {
    auto source = _fs.open(file_uri);
    DBCVersion version = DBCVersion::CATA_PLUS;

    if (_game_ver.expansion == 1) {
        version = DBCVersion::VANILLA;
    }
    else if (_game_ver.expansion == 2 || _game_ver.expansion == 3) {
        version = DBCVersion::BC_WOTLK;
    }

    printSchema(schema);
    std::cout << "Src record bytes: " << DBCFormat::recordSizeSrc(schema, version) << std::endl;
    std::cout << "Dest record bytes: " << DBCFormat::recordSizeDest(schema, version) << std::endl;

    auto dbc = std::make_unique<DBCFile<RuntimeSchema, RuntimeRecord, MPQFileSource, false>>(schema, version);
    dbc->open(std::move(source));
    dbc->load();

    std::cout << "DBC (" << dbc->size() << ")" << std::endl;
    return dbc;
}

std::vector<std::string> searchMPQs(const std::filesystem::path& root) {

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
        auto left_name = left.filename().replace_extension("").string();
        auto right_name = right.filename().replace_extension("").string();

        if (left_name.starts_with(right_name)) {
            return false;
        }
        else if (right_name.starts_with(left_name)) {
            return true;
        }

        if (left_name == right_name) {
            return left.string() < right.string();
        }

        return left_name < right;
    });

    std::vector<std::string> out;
    out.reserve(result.size());
    std::transform(result.begin(), result.end(), std::back_inserter(out), [](const auto& item) {
        return item.string();
    });

    return out;
}

void printSchema(const RuntimeSchema& schema) {
    std::cout << "------" << std::endl;
    std::cout << "Schema:" << std::endl;

    assert(schema.fields().size() == schema.names().size());

    const std::map<Field::Type, std::string> field_types_map{
        {Field::Type::INT, "INT"},
        {Field::Type::FLOAT, "FLOAT"},
        {Field::Type::STRING, "STRING"},
        {Field::Type::LANG_STRING, "LANG_STRING"},
    };

    auto max_name_len = 1 + std::ranges::max(schema.names(), {}, &std::string::size).size();
    auto max_type_len = 1 + std::reduce(
        schema.fields().begin(), schema.fields().end(),
        0,
        [&](size_t init, const Field& field) -> size_t {
            return std::max(init, field_types_map.at(field.type).size());
        }
    );

    for (auto i = 0; i < schema.fields().size(); i++)
    {
        const auto& name = schema.names()[i];
        const auto& field = schema.fields()[i];
        std::cout << std::setw(2) << std::left << i << ": "
            << std::setw(max_name_len) << std::left << name << " - "
            << std::setw(max_type_len) << std::right << field_types_map.at(field.type) << " "
            << (int)field.bytes << "[" << (int)field.size << "]"
            << std::endl;
    }

    std::cout << "------" << std::endl;
}

void printRecord(const RuntimeSchema& schema, const RuntimeRecord& record) {

    auto print_value = [](const runtime_value_ref_t& val) {
        std::visit([](auto v) {
            if constexpr (std::is_same_v<decltype(v), string_data_ref_t>) {
                std::cout << "'" << v << "'";
            }
            else {
                std::cout << v;
            }
        }, val);
    };

    auto accessor = schema(record);

    for (auto& el : accessor) {
        std::cout << *el.name << ": " << std::endl;

        auto count = el.value.size();
        if (count == 1) {
            print_value(el.value[0]);
        }
        else {
            std::cout << "[";
            auto index = 0;
            for (auto& val : el.value) {

                print_value(val);
                if (++index < count) {
                    std::cout << ",";
                }
            }
            std::cout << "]";
        }

        std::cout << std::endl;
    }
}