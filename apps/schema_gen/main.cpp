/*
    Generate static record and schema structures for use with WDBReader.

    schema_gen.exe {version} {db_defs_name} {db_format} {output_path}
    - version       = client version a.b.c.d
    - dbd_defs_name = path to .dbd file.
    - db_format     = DBC_VANILLA, DBC_BC_WOTLK, DBC_CATA_PLUS, DB2
    - output_path   = where output file is saved to.
*/

#include <WDBReader/Database.hpp>
#include <WDBReader/Database/DB2File.hpp>
#include <WDBReader/Database/DBCFile.hpp>
#include <WDBReader/WoWDBDefs.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

using namespace WDBReader::Database;
using namespace WDBReader::WoWDBDefs;


class FileBuilder {
public:
    FileBuilder(const std::string& name, const RuntimeSchema& schema, const Build& build, const std::optional<DBCVersion>& dbc_version);
    void create(const std::filesystem::path& path);
protected:
    void write_header();
    void write_data_section();
    void write_schema_section();
    std::string variableName(std::string name);

    std::string _name;
    RuntimeSchema _schema;
    Build _build;
    std::optional<DBCVersion> _dbc_version;

    std::ofstream _output;
};


int main(int argc, char* argv[]) {

    if (argc < 5) {
        std::cerr << "Not enough parameters." << std::endl;
        std::cerr << "Expected args: {version} {db_defs_name} {db_format} {output_path}" << std::endl;
        return 1;
    }

    std::string version_str(argv[1]);
    std::filesystem::path dbdefs_path(argv[2]);
    std::string db_format(argv[3]);
    std::filesystem::path output_path(argv[4]);

    auto version = Build::fromString(version_str);
    if (!version.has_value()) {
        std::cerr << "Version not valid." << std::endl;
        return 1;
    }

    const std::map<std::string, DBCVersion> dbc_map{
        {"DBC_VANILLA", DBCVersion::VANILLA},
        {"DBC_BC_WOTLK", DBCVersion::BC_WOTLK},
        {"DBC_CATA_PLUS", DBCVersion::CATA_PLUS},
    };

    std::optional<DBCVersion> dbc_version;
    {
        auto dbc_val = dbc_map.find(db_format);
        if (dbc_val != dbc_map.end()) {
            dbc_version = dbc_val->second;
        }
    }

    std::ifstream def_stream(dbdefs_path);
    auto definition = DBDReader::read(def_stream);
    def_stream.close();

    auto schema = makeSchema(definition, version.value());
    if (!schema.has_value()) {
        std::cerr << "Unable to find schema." << std::endl;
        return 1;
    }

    auto plain_name = dbdefs_path.filename().replace_extension("").string();

    FileBuilder builder(plain_name, *schema, *version, dbc_version);
    builder.create(output_path);

    return 0;
}

static const std::map<DBCVersion, std::string> dbc_version_enum_map {
    {DBCVersion::VANILLA, "DBCVersion::VANILLA"},
    {DBCVersion::BC_WOTLK, "DBCVersion::BC_WOTLK"},
    {DBCVersion::CATA_PLUS, "DBCVersion::CATA_PLUS"},
};

void loopSchema(const RuntimeSchema& schema, auto callback) {
    auto name = schema.names().begin();
    auto field = schema.fields().begin();
    auto count = std::max(schema.fields().size(), schema.names().size());
    auto index = 0;

    for (; name != schema.names().end() && field != schema.fields().end(); ++name, ++field) {
        callback(*field, *name, index == count - 1);
        index++;
    }
}

FileBuilder::FileBuilder(const std::string& name, const RuntimeSchema& schema, const Build& build, const std::optional<DBCVersion>& dbc_version) :
    _name(name), _schema(schema), _build(build), _dbc_version(dbc_version)
{
}

void FileBuilder::create(const std::filesystem::path& path) {
    _output.open(path / (_name + "Record.hpp"));

    write_header();
    _output << std::endl;
    _output << "struct " << _name << "Record : public FixedRecord<" << _name << "Record> {" << std::endl << std::endl;

    write_data_section();

    _output << std::endl;
    _output << "\tsize_t recordIndex;" << std::endl;
    _output << "\tRecordEncryption encryptionState;" << std::endl << std::endl;

    write_schema_section();

    _output << std::endl;

    if (_dbc_version.has_value()) {
        _output << "\tstatic_assert(DBCFormat::recordSizeDest(schema, " << dbc_version_enum_map.at(*_dbc_version) << ") == sizeof(data));" << std::endl;
    }
    else {
        _output << "\tstatic_assert(DB2Format::recordSizeDest(schema) == sizeof(data));" << std::endl;
    }

    _output << "};" << std::endl << std::endl;
    _output << "#pragma pack(pop)" << std::endl;

    _output.close();
}

void FileBuilder::write_header() {
    _output << "/* Created via WDBReader schema_gen (client " << _build.toString() << ") */" << std::endl;
    _output << "#pragma once" << std::endl;
    _output << "#include <WDBReader/Database/Schema.hpp>" << std::endl;

    if (_dbc_version.has_value()) {
        _output << "#include <WDBReader/Database/DBCFile.hpp>" << std::endl;
    }
    else {
        _output << "#include <WDBReader/Database/DB2File.hpp>" << std::endl;
    }

    _output << "#include <cstdint>" << std::endl;
    _output << std::endl;
    _output << "using namespace WDBReader::Database;" << std::endl;
    _output << "#pragma pack(push, 1)" << std::endl;
}

void FileBuilder::write_data_section() {
    _output << "\tstruct Data {" << std::endl;
    loopSchema(_schema, [&](const Field& field, const std::string& name, bool is_end) {
        switch (field.type) {
        case Field::Type::STRING:
            _output << "\t\tstring_data_t " << variableName(name);
            if (field.isArray()) {
                _output << "[" << (int)field.size << "]";
            }
            _output << ";" << std::endl;
            break;
        case Field::Type::LANG_STRING:
            if (_dbc_version.has_value()) {
                _output << "\t\tDBCLangString<" << dbc_version_enum_map.at(*_dbc_version) << "> " << variableName(name);
                if (field.isArray()) {
                    _output << "[" << (int)field.size << "]";
                }
                _output << ";" << std::endl;
            }
            else {
                _output << "\t\tstring_data_t " << variableName(name);
                if (field.isArray()) {
                    _output << "[" << (int)field.size << "]";
                }
                _output << ";" << std::endl;
            }
            break;
        case Field::Type::INT:
            _output << "\t\tuint" << int(field.bytes * 8) << "_t " << variableName(name);
            if (field.isArray()) {
                _output << "[" << (int)field.size << "]";
            }
            _output << ";" << std::endl;
            break;
        case Field::Type::FLOAT:
            _output << "\t\tfloat " << variableName(name);
            if (field.isArray()) {
                _output << "[" << (int)field.size << "]";
            }
            _output << ";" << std::endl;
            break;
        default:
            throw std::logic_error("Unknown field type.");
        }
        });
    _output << "\t} data;" << std::endl;
}

void FileBuilder::write_schema_section() {
    auto annotate_str = [](const Annotation& ann) -> std::string {
        constexpr auto default_value = Annotation();
        if (ann == default_value) {
            return "";
        }

        std::stringstream ss;
        ss << "Annotation()";

        if (ann.isId) {
            ss << ".Id()";
        }

        if (!ann.isInline) {
            ss << ".NonInline()";
        }

        if (ann.isRelation) {
            ss << ".Relation()";
        }

        return ss.str();
        };

    _output << "\tconstexpr static Schema schema = Schema(" << std::endl;
    loopSchema(_schema, [&](const Field& field, const std::string& name, bool is_end) {
        auto annotation = annotate_str(field.annotation);
        switch (field.type) {
        case Field::Type::STRING:
        case Field::Type::LANG_STRING:
        {
            const auto keyword = field.type == Field::Type::LANG_STRING ? "langString" : "string";
            _output << "\t\tField::" << keyword;
            if (annotation.size() > 0) {
                _output << "(" << (int)field.size << ", " << annotation << ")";
            }
            else if (field.size > 1) {
                _output << "(" << (int)field.size << ")";
            }
            else {
                _output << "()";
            }
        }
        break;
        case Field::Type::INT:
        case Field::Type::FLOAT:
            _output << "\t\tField::value<decltype(data." << variableName(name) << ")>(" << annotation << ")";
            break;
        default:
            throw std::logic_error("Unknown field type.");
        }

        if (!is_end) {
            _output << ",";
        }

        _output << std::endl;
        });
    _output << "\t);" << std::endl;
}

std::string FileBuilder::variableName(std::string name) {

    if (name == "ID") {
        return "id";
    }

    // ending ID
    if (name.ends_with("ID")) {
        name.replace(name.end() - 2, name.end(), "Id");
    }

    // remove underscores
    auto underscore = name.find("_");
    while (underscore != std::string::npos) {
        name[underscore + 1] = std::toupper(name[underscore + 1]);
        name.erase(underscore, 1);
        underscore = name.find("_");
    }

    // lower case first char if not abbreviation
    if (!std::isupper(name[1])) {
        name[0] = std::tolower(name[0]);
    }

    return name;
}
