#include <catch2/catch_test_macros.hpp> 

#include <WDBReader/Database.hpp>

using namespace WDBReader::Database;

TEST_CASE("Annotations can be compared.", "[database]")
{
    {
        constexpr Annotation static_annotation = Annotation().Id().NonInline();
        Annotation runtime_annotation = Annotation().Id().NonInline();

        REQUIRE(static_annotation == runtime_annotation);
    }

    {
        constexpr Annotation static_annotation = Annotation().Id();
        Annotation runtime_annotation = Annotation().Id().NonInline();

        REQUIRE(static_annotation != runtime_annotation);
    }

    {
        constexpr Annotation annotation1 = Annotation().Id();
        constexpr Annotation annotation2 = Annotation().Id();
        static_assert(annotation1 == annotation2);
    }
}

TEST_CASE("Fields can be compared.", "[database]")
{
    {
        constexpr Field static_field = Field::value<uint32_t>();
        Field runtime_field = Field::value<uint32_t>();

        REQUIRE(static_field == runtime_field);
    }

    {
        constexpr Field static_field = Field::value<uint32_t>();
        Field runtime_field = Field::value<uint32_t>(Annotation().Id().NonInline());

        REQUIRE(static_field != runtime_field);
    }

    {
        constexpr Field field1 = Field::value<uint32_t>(Annotation().Id());
        constexpr Field field2 = Field::value<uint32_t>(Annotation().Id().NonInline());
        static_assert(field1 != field2);
    }
}


TEST_CASE("Field type resolution.", "[database]")
{
    {
        uint32_t raw1[3];
        constexpr auto field1 = Field::value<decltype(raw1)>();
        constexpr auto field2 = Field::integerArray(sizeof(raw1), 3);

        static_assert(field1 == field2);
    }

    {
        int32_t raw1;
        constexpr Field field1 = Field::value<decltype(raw1)>();
        static_assert(field1.annotation.isSigned);

        int32_t raw2[2];
        constexpr Field field2 = Field::value<decltype(raw2)>();
        static_assert(field2.annotation.isSigned);
    }
}


TEST_CASE("Schema can be compared.", "[database]")
{
    {
        constexpr Schema static_schema = Schema(
            Field::value<uint32_t>(Annotation().Id()),
            Field::value<uint32_t>()
        );

        RuntimeSchema runtime_schema = RuntimeSchema({
            Field::value<uint32_t>(Annotation().Id()),
            Field::value<uint32_t>()
        }, {
            "field1",
            "field2"
        });

        REQUIRE(static_schema == runtime_schema);
    }

    {
        constexpr Schema static_schema1 = Schema(
            Field::value<uint32_t>(Annotation().Id()),
            Field::value<uint32_t>()
        );

        constexpr Schema static_schema2 = Schema(
            Field::value<uint32_t>(Annotation().Id()),
            Field::value<uint32_t>()
        );

        static_assert(static_schema1 == static_schema2);
    }

    {
        RuntimeSchema runtime_schema1 = RuntimeSchema({
               Field::value<uint32_t>(Annotation().Id()),
               Field::value<uint32_t>()
            }, {
                "field1",
                "field2"
            });

        RuntimeSchema runtime_schema2 = RuntimeSchema({
           Field::value<uint32_t>(Annotation().Id()),
           Field::value<uint32_t>()
            }, {
                "field1",
                "field2"
            });

        REQUIRE(runtime_schema1 == runtime_schema2);
    }
}


TEST_CASE("Runtime schema can read runtime records.", "[database]")
{
    auto schema = RuntimeSchema(
        {
            Field::value<uint32_t>(Annotation().Id().NonInline()),
            Field::value<uint32_t>(),
            Field::value<uint32_t>(),
            Field::value<uint32_t>(),
            Field::value<uint32_t>(Annotation().Relation().NonInline())
        },
        {
            "id",
            "component",
            "color",
            "fileDataId",
            "emblemId"
        }
    );


    auto record = RuntimeRecord();
    record.data.push_back(runtime_value_t(10u));
    record.data.push_back(runtime_value_t(11u));
    record.data.push_back(runtime_value_t(12u));
    record.data.push_back(runtime_value_t(13u));
    record.data.push_back(runtime_value_t(14u));

    auto accessor = schema(record);

    auto r1 = accessor["id"];
    auto r2 = accessor["component"];

    REQUIRE(r1.size() == 1);
    REQUIRE(r2.size() == 1);

    REQUIRE(std::get<uint32_t>(r1[0]) == 10u);
    REQUIRE(std::get<uint32_t>(r2[0]) == 11u);

    std::for_each(accessor.begin(), accessor.end(), [](auto& item) {
        const auto& name = item.name;
        const auto& value = item.value;

        REQUIRE(value.size() > 0);
    });



    auto [file_data_id, id] = accessor.get<uint8_t, uint32_t>("fileDataId", "id");

    REQUIRE(file_data_id == 13);
    REQUIRE(id == 10);


}


TEST_CASE("Runtime schema can read runtime records (array).", "[database]")
{
    auto schema = RuntimeSchema(
        {
            Field::value<uint32_t>(Annotation().Id().NonInline()),
            Field::value<uint32_t[3]>(),
        },
        {
            "id",
            "array"
        }
    );


    auto record = RuntimeRecord();
    record.data.push_back(runtime_value_t(10u));
    record.data.push_back(runtime_value_t(11u));
    record.data.push_back(runtime_value_t(12u));
    record.data.push_back(runtime_value_t(13u));

    auto accessor = schema(record);


    auto [array3] = accessor.get<std::array<uint32_t, 3>>("array");

    REQUIRE(array3[0] == 11);
    REQUIRE(array3[1] == 12);
    REQUIRE(array3[2] == 13);

    auto [array1] = accessor.get<std::array<uint32_t, 1>>("array");

    REQUIRE(array1[0] == 11);
}

TEST_CASE("Runtime schema reading handles conversions.", "[database]")
{
    SECTION("Handles casts")
    {
        auto schema = RuntimeSchema(
            {
                Field::value<uint32_t>(Annotation().Id().NonInline()),
                Field::value<uint8_t>(),
                Field::value<int8_t>(), // annotated as signed.
                Field::value<uint32_t>(),
            },
            {
                "id",
                "ubyte",
                "sbyte",
                "uint32"
            }
        );

        auto record = RuntimeRecord();
        record.data.push_back(runtime_value_t(10u));
        record.data.push_back(uint8_t(255));
        record.data.push_back(uint8_t(-1));
        record.data.push_back(uint32_t(123456));

        SECTION("Same size")
        {

            {
                auto [ubyte, sbyte] = schema(record).get<uint8_t, int8_t>("ubyte", "sbyte");
                REQUIRE(ubyte == 255);
                REQUIRE(sbyte == -1);
            }

            {
                auto [ubyte, sbyte] = schema(record).get<uint8_t, uint8_t>("ubyte", "sbyte");
                REQUIRE(ubyte == 255);
                REQUIRE(sbyte == 255);
            }

            {
                auto [ubyte, sbyte] = schema(record).get<int8_t, int8_t>("ubyte", "sbyte");
                REQUIRE(ubyte == -1);
                REQUIRE(sbyte == -1);
            }
        }

        SECTION("Up size")
        {
            {
                auto [ubyte, sbyte] = schema(record).get<uint16_t, int16_t>("ubyte", "sbyte");
                REQUIRE(ubyte == 255);
                REQUIRE(sbyte == -1);
            }

            {
                auto [ubyte, sbyte] = schema(record).get<uint16_t, uint16_t>("ubyte", "sbyte");
                REQUIRE(ubyte == 255);
                REQUIRE(sbyte == 255);
            }

            {
                auto [ubyte, sbyte] = schema(record).get<int16_t, int16_t>("ubyte", "sbyte");
                REQUIRE(ubyte == 255);
                REQUIRE(sbyte == -1);
            }
        }

        SECTION("Down size")
        {
            {
                auto [uint32] = schema(record).get<uint64_t>("uint32");
                REQUIRE(uint32 == 123456);
            }

            {
                auto [uint32] = schema(record).get<int64_t>("uint32");
                REQUIRE(uint32 == 123456);
            }
        } 
    }

    SECTION("Handles overflow")
    {
        auto schema = RuntimeSchema(
            {
                Field::value<uint32_t>(Annotation().Id().NonInline()),
                Field::value<uint16_t>(),
                Field::value<uint32_t>(),
                Field::value<int16_t>(),
                Field::value<int32_t>(),
                Field::value<int32_t>(),
            },
            {
                "id",
                "uint16",
                "uint32",
                "sint16",
                "sint32",
                "sint32_safe"
            }
        );

        auto record = RuntimeRecord();
        record.data.push_back(runtime_value_t(10u));
        record.data.push_back(uint16_t(std::numeric_limits<uint16_t>::max() - 1));
        record.data.push_back(uint32_t(std::numeric_limits<uint32_t>::max() - 1));
        record.data.push_back(uint16_t(std::numeric_limits<int16_t>::max() - 1));
        record.data.push_back(uint32_t(std::numeric_limits<int32_t>::max() - 1));
        record.data.push_back(uint32_t(10));

        {
            REQUIRE_NOTHROW(schema(record).get<uint16_t>("uint16"));
            REQUIRE_NOTHROW(schema(record).get<uint32_t>("uint32"));
            REQUIRE_NOTHROW(schema(record).get<int16_t>("uint16"));
            REQUIRE_NOTHROW(schema(record).get<int32_t>("uint32"));
        }

        {
            REQUIRE_THROWS(schema(record).get<int16_t>("sint8"));
            REQUIRE_THROWS(schema(record).get<int32_t>("uint8"));
        }

        {
            REQUIRE_NOTHROW(schema(record).get<int8_t>("sint32_safe"));
        }
    }
}

TEST_CASE("Format Strings", "[database]")
{
    auto str = WDB2_MAGIC.str();
    REQUIRE(str == "WDB2");
}