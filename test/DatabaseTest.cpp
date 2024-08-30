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