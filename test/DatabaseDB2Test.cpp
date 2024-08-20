#include <catch2/catch_test_macros.hpp> 

#include <WDBReader/Database/DB2File.hpp>
#include <WDBReader/Filesystem/CASCFilesystem.hpp>

using namespace WDBReader::Database;
using namespace WDBReader::Filesystem;

#pragma pack(push, 1)
struct StaticBFADB2ItemSparseRecord : public FixedRecord<StaticBFADB2ItemSparseRecord> {

	struct Data {
		uint32_t id;
		uint64_t allowableRace;
		string_data_t descriptionLang;
		string_data_t display3Lang;
		string_data_t display2Lang;
		string_data_t display1Lang;
		string_data_t displayLang;
		float dmgVariance;
		uint32_t durationInInventory;
		float qualityModifier;
		uint32_t bagFamily;
		float itemRange;
		float statPercentageOfSocket[10];
		uint32_t statPercentEditor[10];
		uint32_t stackable;
		uint32_t maxCount;
		uint32_t requiredAbility;
		uint32_t sellPrice;
		uint32_t buyPrice;
		uint32_t vendorStackCount;
		float priceVariance;
		float priceRandomValue;
		uint32_t flags[4];
		uint32_t oppositeFactionItemId;
		uint16_t itemNameDescriptionId;
		uint16_t requiredTransmogHoliday;
		uint16_t requiredHoliday;
		uint16_t limitCategory;
		uint16_t gemProperties;
		uint16_t socketMatchEnchantmentId;
		uint16_t totemCategoryId;
		uint16_t instanceBound;
		uint16_t zoneBound[2];
		uint16_t itemSet;
		uint16_t lockId;
		uint16_t startQuestId;
		uint16_t pageId;
		uint16_t itemDelay;
		uint16_t scalingStatDistributionId;
		uint16_t minFactionId;
		uint16_t requiredSkillRank;
		uint16_t requiredSkill;
		uint16_t itemLevel;
		uint16_t allowableClass;
		uint8_t expansionId;
		uint8_t artifactId;
		uint8_t spellWeight;
		uint8_t spellWeightCategory;
		uint8_t socketType[3];
		uint8_t sheatheType;
		uint8_t material;
		uint8_t pageMaterialId;
		uint8_t languageId;
		uint8_t bonding;
		uint8_t damageType;
		uint8_t statModifierBonusStat[10];
		uint8_t containerSlots;
		uint8_t minReputation;
		uint8_t requiredPvpMedal;
		uint8_t requiredPvpRank;
		uint8_t requiredLevel;
		uint8_t inventoryType;
		uint8_t overallQualityId;
	} data;

	size_t recordIndex;
	RecordEncryption encryptionState;

	constexpr static Schema schema = Schema(
		Field::integer(sizeof(data.id), Annotation().Id().NonInline()),
		Field::integer(sizeof(data.allowableRace)),
		Field::langString(),
		Field::langString(),
		Field::langString(),
		Field::langString(),
		Field::langString(),
		Field::floatingPoint(sizeof(data.dmgVariance)),
		Field::integer(sizeof(data.durationInInventory)),
		Field::floatingPoint(sizeof(data.qualityModifier)),
		Field::integer(sizeof(data.bagFamily)),
		Field::floatingPoint(sizeof(data.itemRange)),
		Field::floatingPointArray(sizeof(data.statPercentageOfSocket), 10),
		Field::integerArray(sizeof(data.statPercentEditor), 10),
		Field::integer(sizeof(data.stackable)),
		Field::integer(sizeof(data.maxCount)),
		Field::integer(sizeof(data.requiredAbility)),
		Field::integer(sizeof(data.sellPrice)),
		Field::integer(sizeof(data.buyPrice)),
		Field::integer(sizeof(data.vendorStackCount)),
		Field::floatingPoint(sizeof(data.priceVariance)),
		Field::floatingPoint(sizeof(data.priceRandomValue)),
		Field::integerArray(sizeof(data.flags), 4),
		Field::integer(sizeof(data.oppositeFactionItemId)),
		Field::integer(sizeof(data.itemNameDescriptionId)),
		Field::integer(sizeof(data.requiredTransmogHoliday)),
		Field::integer(sizeof(data.requiredHoliday)),
		Field::integer(sizeof(data.limitCategory)),
		Field::integer(sizeof(data.gemProperties)),
		Field::integer(sizeof(data.socketMatchEnchantmentId)),
		Field::integer(sizeof(data.totemCategoryId)),
		Field::integer(sizeof(data.instanceBound)),
		Field::integerArray(sizeof(data.zoneBound), 2),
		Field::integer(sizeof(data.itemSet)),
		Field::integer(sizeof(data.lockId)),
		Field::integer(sizeof(data.startQuestId)),
		Field::integer(sizeof(data.pageId)),
		Field::integer(sizeof(data.itemDelay)),
		Field::integer(sizeof(data.scalingStatDistributionId)),
		Field::integer(sizeof(data.minFactionId)),
		Field::integer(sizeof(data.requiredSkillRank)),
		Field::integer(sizeof(data.requiredSkill)),
		Field::integer(sizeof(data.itemLevel)),
		Field::integer(sizeof(data.allowableClass)),
		Field::integer(sizeof(data.expansionId)),
		Field::integer(sizeof(data.artifactId)),
		Field::integer(sizeof(data.spellWeight)),
		Field::integer(sizeof(data.spellWeightCategory)),
		Field::integerArray(sizeof(data.socketType), 3),
		Field::integer(sizeof(data.sheatheType)),
		Field::integer(sizeof(data.material)),
		Field::integer(sizeof(data.pageMaterialId)),
		Field::integer(sizeof(data.languageId)),
		Field::integer(sizeof(data.bonding)),
		Field::integer(sizeof(data.damageType)),
		Field::integerArray(sizeof(data.statModifierBonusStat), 10),
		Field::integer(sizeof(data.containerSlots)),
		Field::integer(sizeof(data.minReputation)),
		Field::integer(sizeof(data.requiredPvpMedal)),
		Field::integer(sizeof(data.requiredPvpRank)),
		Field::integer(sizeof(data.requiredLevel)),
		Field::integer(sizeof(data.inventoryType)),
		Field::integer(sizeof(data.overallQualityId))
	);

	static_assert(DB2Format::recordSizeDest(schema) == sizeof(data));

};

struct BFADB2ModelFileDataRecord : public FixedRecord<BFADB2ModelFileDataRecord> {
	struct Data {
		uint32_t fileDataId;
		uint8_t flags;
		uint8_t loadCount;
		uint32_t modelResourcesId;
	} data;

	size_t recordIndex;
	RecordEncryption encryptionState;

	constexpr static Schema schema = Schema(
		Field::value<decltype(data.fileDataId)>(Annotation().Id()),
		Field::value<decltype(data.flags)>(),
		Field::value<decltype(data.loadCount)>(),
		Field::value<decltype(data.modelResourcesId)>(Annotation().Relation())
	);

	static_assert(DB2Format::recordSizeDest(schema) == sizeof(data));
};

struct BFACreatureDisplayInfoRecord : public FixedRecord<BFACreatureDisplayInfoRecord> {

	struct Data {
		uint32_t id;
		uint16_t modelId;
		uint16_t soundId;
		uint8_t sizeClass;
		float creatureModelScale;
		uint8_t creatureModelAlpha;
		uint8_t bloodId;
		uint32_t extendedDisplayInfoId;
		uint16_t NPCSoundId;
		uint16_t particleColorId;
		uint32_t portraitCreatureDisplayInfoId;
		uint32_t portraitTextureFileDataId;
		uint16_t objectEffectPackageId;
		uint16_t animReplacementSetId;
		uint8_t flags;
		uint32_t stateSpellVisualKitId;
		float playerOverrideScale;
		float petInstanceScale;
		uint8_t unarmedWeaponType;
		uint32_t mountPoofSpellVisualKitId;
		uint32_t dissolveEffectId;
		uint8_t gender;
		uint32_t dissolveOutEffectId;
		uint8_t creatureModelMinLod;
		uint32_t textureVariationFileDataId[3];
	} data;

	size_t recordIndex;
	RecordEncryption encryptionState;

	constexpr static Schema schema = Schema(
		Field::value<decltype(data.id)>(Annotation().Id()),
		Field::value<decltype(data.modelId)>(),
		Field::value<decltype(data.soundId)>(),
		Field::value<decltype(data.sizeClass)>(),
		Field::value<decltype(data.creatureModelScale)>(),
		Field::value<decltype(data.creatureModelAlpha)>(),
		Field::value<decltype(data.bloodId)>(),
		Field::value<decltype(data.extendedDisplayInfoId)>(),
		Field::value<decltype(data.NPCSoundId)>(),
		Field::value<decltype(data.particleColorId)>(),
		Field::value<decltype(data.portraitCreatureDisplayInfoId)>(),
		Field::value<decltype(data.portraitTextureFileDataId)>(),
		Field::value<decltype(data.objectEffectPackageId)>(),
		Field::value<decltype(data.animReplacementSetId)>(),
		Field::value<decltype(data.flags)>(),
		Field::value<decltype(data.stateSpellVisualKitId)>(),
		Field::value<decltype(data.playerOverrideScale)>(),
		Field::value<decltype(data.petInstanceScale)>(),
		Field::value<decltype(data.unarmedWeaponType)>(),
		Field::value<decltype(data.mountPoofSpellVisualKitId)>(),
		Field::value<decltype(data.dissolveEffectId)>(),
		Field::value<decltype(data.gender)>(),
		Field::value<decltype(data.dissolveOutEffectId)>(),
		Field::value<decltype(data.creatureModelMinLod)>(),
		Field::value<decltype(data.textureVariationFileDataId)>()
	);

	static_assert(DB2Format::recordSizeDest(schema) == sizeof(data));
};

struct BFACreatureDisplayInfoExtraRecord : public FixedRecord<BFACreatureDisplayInfoExtraRecord> {

	struct Data {
		uint32_t id;
		uint8_t displayRaceId;
		uint8_t displaySexId;
		uint8_t displayClassId;
		uint8_t skinId;
		uint8_t faceId;
		uint8_t hairStyleId;
		uint8_t hairColorId;
		uint8_t facialHairId;
		uint8_t flags;
		uint32_t bakeMaterialResourcesId;
		uint32_t HDBakeMaterialResourcesId;
		uint8_t customDisplayOption[3];
	} data;

	size_t recordIndex;
	RecordEncryption encryptionState;

	constexpr static Schema schema = Schema(
		Field::value<decltype(data.id)>(Annotation().Id().NonInline()),
		Field::value<decltype(data.displayRaceId)>(),
		Field::value<decltype(data.displaySexId)>(),
		Field::value<decltype(data.displayClassId)>(),
		Field::value<decltype(data.skinId)>(),
		Field::value<decltype(data.faceId)>(),
		Field::value<decltype(data.hairStyleId)>(),
		Field::value<decltype(data.hairColorId)>(),
		Field::value<decltype(data.facialHairId)>(),
		Field::value<decltype(data.flags)>(),
		Field::value<decltype(data.bakeMaterialResourcesId)>(),
		Field::value<decltype(data.HDBakeMaterialResourcesId)>(),
		Field::value<decltype(data.customDisplayOption)>()
	);

	static_assert(DB2Format::recordSizeDest(schema) == sizeof(data));
};

#pragma pack(pop)

#ifdef TESTING_CASC_DIR

TEST_CASE("Multiple formats can be handled.", "[database:db2]")
{
	auto casc_fs = CASCFilesystem(TESTING_CASC_DIR, CASC_LOCALE_ENUS);
	auto source = casc_fs.open(1349054u); //dbfilesclient/chartitles.db2
	
	REQUIRE(source != nullptr);

	constexpr Schema schema = Schema(
		Field::value<uint32_t>(Annotation().Id().NonInline()),
		Field::langString(),
		Field::langString(),
		Field::value<uint16_t>(),
		Field::value<uint8_t>()
	);

	auto db2 = makeDB2File<decltype(schema), RuntimeRecord, CASCFileSource>(schema, std::move(source));

	REQUIRE(db2 != nullptr);
	REQUIRE(db2->size() > 0);
}

TEST_CASE("Sparse files can be read.", "[database:db2]")
{
	auto casc_fs = CASCFilesystem(TESTING_CASC_DIR, CASC_LOCALE_ENUS);
	auto source = casc_fs.open(1572924u);   //itemsparse.db2

	auto db2 = DB2File<
		DB2FormatWDC3,
		decltype(StaticBFADB2ItemSparseRecord::schema),
		RuntimeRecord,
		CASCFileSource>(
			StaticBFADB2ItemSparseRecord::schema
		);

	db2.open(std::move(source));
	db2.load();

	REQUIRE(db2.size() > 0);


	SECTION("Can handle copy records")
	{
		auto rec = db2[117645];
	}
}


TEST_CASE("Relations can be read.", "[database:db2]")
{
	auto casc_fs = CASCFilesystem(TESTING_CASC_DIR, CASC_LOCALE_ENUS);
	auto source = casc_fs.open(2910470u);   //dbfilesclient/guildtabardemblem.db2

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


	auto db2 = DB2File<
		DB2FormatWDC3,
		RuntimeSchema,
		RuntimeRecord,
		CASCFileSource>(
			schema
		);

	db2.open(std::move(source));
	db2.load();

	const size_t size = db2.size();
	REQUIRE(size > 0);

	size_t count = 0;
	for (const auto& record : db2) {
		count++;
		assert(record.recordIndex >= 0);
	}

	REQUIRE(size == count);

}

TEST_CASE("Common data can be read.", "[database:db2]")
{
	auto casc_fs = CASCFilesystem(TESTING_CASC_DIR, CASC_LOCALE_ENUS);
	auto source = casc_fs.open(1337833u);   //dbfilesclient/modelfiledata.db2

	auto db2 = DB2File<
		DB2FormatWDC3,
		decltype(BFADB2ModelFileDataRecord::schema),
		BFADB2ModelFileDataRecord,
		CASCFileSource>(BFADB2ModelFileDataRecord::schema);

	db2.open(std::move(source));
	db2.load();

	REQUIRE(db2.size() > 0);

	auto rec1 = db2[0];
}

TEST_CASE("Encrypted data can be read.", "[database:db2]")
{

	auto casc_fs = CASCFilesystem(TESTING_CASC_DIR, CASC_LOCALE_ENUS);
	auto source = casc_fs.open(1108759u);   //dbfilesclient/creaturedisplayinfo.db2

	auto db2 = makeDB2File<BFACreatureDisplayInfoRecord, CASCFileSource>(std::move(source));

	REQUIRE(db2->size() > 0);

	for (auto& rec : *db2) {
		assert(rec.recordIndex >= 0);
	}
}

TEST_CASE("Encrypted data can be read 2.", "[database:db2]")
{
	auto casc_fs = CASCFilesystem(TESTING_CASC_DIR, CASC_LOCALE_ENUS);
	auto source = casc_fs.open(1264997u);   // dbfilesclient / creaturedisplayinfoextra.db2

	auto db2 = makeDB2File<BFACreatureDisplayInfoExtraRecord, CASCFileSource>(std::move(source));
	REQUIRE(db2->size() > 0);

	auto rec2 = (*db2)[42090];
	REQUIRE(rec2.recordIndex == 42090);
}


TEST_CASE("Shorthand file creation.", "[database:db2]")
{
	//auto runtime_type = makeDB2File(rt_schema, source);
	//auto static_type = makeDB2File<StaticBFADB2ItemSparseRecord>(source);
}

#endif




