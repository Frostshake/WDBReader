#include <catch2/catch_test_macros.hpp> 

#include <WDBReader/Database/DBCFile.hpp>
#include <WDBReader/Filesystem/MPQFilesystem.hpp>
#include <WDBReader/Filesystem/NativeFilesystem.hpp>

using namespace WDBReader::Database;
using namespace WDBReader::Filesystem;
using namespace WDBReader::Utility;


#pragma pack(push, 1)

struct StaticWOTLKDBCSpellItemEnchantmentRecord : public FixedRecord<StaticWOTLKDBCSpellItemEnchantmentRecord> {

	struct Data {
		uint32_t id;
		uint32_t charges;
		uint32_t effects[3];
		uint32_t effectPointsMin[3];
		uint32_t effectPointsMax[3];
		uint32_t effectArgs[3];
		DBCLangString<DBCVersion::BC_WOTLK> name;
		uint32_t itemVisualId;
		uint32_t flags;
		uint32_t srcItemId;
		uint32_t conditionId;
		uint32_t skillId;
		uint32_t skillLevel;
		uint32_t requiredLevel;
	} data;

	size_t recordIndex;
	RecordEncryption encryptionState;

	constexpr static Schema schema = Schema(
		Field::value<decltype(data.id)>(Annotation().Id()),
		Field::value<decltype(data.charges)>(),
		Field::value<decltype(data.effects)>(),
		Field::value<decltype(data.effectPointsMin)>(),
		Field::value<decltype(data.effectPointsMax)>(),
		Field::value<decltype(data.effectArgs)>(),
		Field::langString(),
		Field::value<decltype(data.itemVisualId)>(),
		Field::value<decltype(data.flags)>(),
		Field::value<decltype(data.srcItemId)>(),
		Field::value<decltype(data.conditionId)>(),
		Field::value<decltype(data.skillId)>(),
		Field::value<decltype(data.skillLevel)>(),
		Field::value<decltype(data.requiredLevel)>()
	);

	static_assert(DBCFormat::recordSizeDest(schema, DBCVersion::BC_WOTLK) == sizeof(data));

};

struct StaticWOTLKDBCChrRacesRecord : public FixedRecord<StaticWOTLKDBCChrRacesRecord> {

	struct Data {
		uint32_t id;								// 0
		uint32_t flags;								// 4
		uint32_t factionId;							// 8
		uint32_t explorationSoundId;				// 12
		uint32_t maleDisplayId;						// 16
		uint32_t femaleDisplayId;					// 20
		string_data_t clientPrefix;					// 24 (+8)				// 6
		uint32_t baseLanguage;						// 32
		uint32_t creatureType;						// 36
		uint32_t resSicknessSpellId;				// 40
		uint32_t splashSoundId;						// 44
		string_data_t clientFileString;				// 48 (+8)				// 11
		uint32_t cinematicSequenceId;				// 56
		uint32_t alliance;							// 60
		DBCLangString<DBCVersion::BC_WOTLK> nameLang;		//64 (+132)		// 14
		DBCLangString<DBCVersion::BC_WOTLK> nameFemaleLang;	//196 (+132)	// 15
		DBCLangString<DBCVersion::BC_WOTLK> nameMaleLang;	//328 (+132)	// 16
		string_data_t facialHairCustomization[2];			//460 (+16)		// 17
		string_data_t hairCustomization;			//476 (+8)				// 18
		uint32_t requiredExpansion;					//484
		//											//488
	} data;

	size_t recordIndex;
	RecordEncryption encryptionState;

	constexpr static Schema schema = Schema(
		Field::value<decltype(data.id)>(Annotation().Id()),
		Field::value<decltype(data.flags)>(),
		Field::value<decltype(data.factionId)>(),
		Field::value<decltype(data.explorationSoundId)>(),
		Field::value<decltype(data.maleDisplayId)>(),
		Field::value<decltype(data.femaleDisplayId)>(),
		Field::string(),
		Field::value<decltype(data.baseLanguage)>(),
		Field::value<decltype(data.creatureType)>(),
		Field::value<decltype(data.resSicknessSpellId)>(),
		Field::value<decltype(data.splashSoundId)>(),
		Field::string(),
		Field::value<decltype(data.cinematicSequenceId)>(),
		Field::value<decltype(data.alliance)>(),
		Field::langString(),
		Field::langString(),
		Field::langString(),
		Field::string(2),
		Field::string(),
		Field::value<decltype(data.requiredExpansion)>()
	);

	static_assert(DBCFormat::recordSizeDest(schema, DBCVersion::BC_WOTLK) == sizeof(data));
};

#pragma pack(pop)


#ifdef TESTING_MPQ_DIR

TEST_CASE("Reading dbc file.", "[database:dbc]")
{
    auto mpq_fs = MPQFilesystem(TESTING_MPQ_DIR, {
        "patch-3.MPQ",
        "patch-2.MPQ",
        "patch.MPQ",
        "lichking.MPQ",
        "expansion.MPQ",
        "common-2.MPQ",
        "common.MPQ",
		"enUS/patch-enUS-3.MPQ",
		"enUS/patch-enUS-2.MPQ",
        "enUS/patch-enUS.MPQ"
    });

	
	SECTION("Reading SpellItemEnchantment")
	{
		auto source = mpq_fs.open("DBFilesClient\\SpellItemEnchantment.dbc");
		REQUIRE(source != nullptr);

		auto dbc = DBCFile<decltype(StaticWOTLKDBCSpellItemEnchantmentRecord::schema), StaticWOTLKDBCSpellItemEnchantmentRecord, MPQFileSource>(StaticWOTLKDBCSpellItemEnchantmentRecord::schema, DBCVersion::BC_WOTLK);
		dbc.open(std::move(source));
		dbc.load();

		const size_t size = dbc.size();
		std::vector< StaticWOTLKDBCSpellItemEnchantmentRecord> copied;
		REQUIRE(size > 0);

		for (auto& rec : dbc) {
			copied.push_back(std::move(rec));
		}

		REQUIRE(size == copied.size());
	}

	SECTION("Reading ChrRaces")
	{

		auto native_fs = NativeFilesystem();
		auto source2 = native_fs.open("C:\\World of Warcraft\\ChrRaces.dbc");
		auto source = mpq_fs.open("DBFilesClient\\ChrRaces.dbc");
		REQUIRE(source != nullptr);

		auto dbc = makeDBCFile<StaticWOTLKDBCChrRacesRecord, MPQFileSource>(DBCVersion::BC_WOTLK);
		dbc.open(std::move(source));
		dbc.load();

		REQUIRE(dbc.size() > 0);

		for (auto& rec : dbc) {
			// ...
		}
	}
}

#endif


TEST_CASE("Shorthand file creation.", "[database:dbc]")
{
	auto rt_schema = RuntimeSchema({}, {});
	auto runtime_type = makeDBCFile<MPQFileSource>(rt_schema, DBCVersion::VANILLA, DBCStringLocale::enUS);
	auto static_type = makeDBCFile<StaticWOTLKDBCSpellItemEnchantmentRecord, MPQFileSource>(DBCVersion::BC_WOTLK);
}

TEST_CASE("DBC version detection.", "[database:dbc]")
{
	{
		constexpr GameVersion vanilla{ 1, 12, 1, 5875 };
		constexpr DBCVersion ver = getDBCVersion(vanilla);
		static_assert(ver == DBCVersion::VANILLA);
	}

	{
		constexpr GameVersion early_tbc{ 2, 0, 0, 5991 };
		constexpr DBCVersion ver = getDBCVersion(early_tbc);
		static_assert(ver == DBCVersion::VANILLA);
	}

	{
		constexpr GameVersion late_tbc{ 2, 4, 3, 8606 };
		constexpr DBCVersion ver = getDBCVersion(late_tbc);
		static_assert(ver == DBCVersion::BC_WOTLK);
	}

	{
		constexpr GameVersion wotlk{ 3, 3, 5, 12340 };
		constexpr DBCVersion ver = getDBCVersion(wotlk);
		static_assert(ver == DBCVersion::BC_WOTLK);
	}

	{
		constexpr GameVersion cata{ 4, 3, 4, 15595 };
		constexpr DBCVersion ver = getDBCVersion(cata);
		static_assert(ver == DBCVersion::CATA_PLUS);
	}

}
