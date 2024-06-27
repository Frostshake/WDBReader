#include <catch2/catch_test_macros.hpp> 

#include <WDBReader/Utility.hpp>

using namespace WDBReader::Utility;

TEST_CASE("Game versions can be compared.", "[utility]")
{
    GameVersion a(1, 0, 0);
    GameVersion b(2, 0, 0);

    REQUIRE(b > a);

    GameVersion c(3, 3, 5);
    GameVersion d(3, 3, 5);

    REQUIRE(c == d);
}

TEST_CASE("Game versions can be parsed.", "[utility]")
{
    auto parsed = GameVersion::fromString("3.3.5.12340");
    constexpr GameVersion expected(3, 3, 5, 12340);

    REQUIRE(parsed.has_value());
    REQUIRE(expected == parsed.value());
}


TEST_CASE("Scope guard called.", "[utility]")
{
    bool called = false;

    {
        auto guard = ScopeGuard([&called]() {
            called = true;
            });

        REQUIRE(!called);
    }

    REQUIRE(called);
}