#include <catch2/catch_test_macros.hpp> 

#include <WDBReader/WoWDBDefs.hpp>
#include <fstream>

using namespace WDBReader::WoWDBDefs;

#ifdef TESTING_WOWDBDEFS_DIR

TEST_CASE("Definitions can be read.", "[wowdbdefs]")
{
    std::ifstream stream(TESTING_WOWDBDEFS_DIR "/definitions/CharTitles.dbd");
    auto definition = DBDReader::read(stream);
    stream.close();

    REQUIRE(definition.columnDefinitions.size() > 0);
    REQUIRE(definition.versionDefinitions.size() > 0);
}

TEST_CASE("Schema created from definitions.", "[wowdbdef]")
{
    std::ifstream stream(TESTING_WOWDBDEFS_DIR "/definitions/SpellItemEnchantment.dbd");
    auto definition = DBDReader::read(stream);
    stream.close();

    auto schema = makeSchema(definition, Build(3, 3, 5, 12340));

    REQUIRE(schema.has_value());
}


#endif