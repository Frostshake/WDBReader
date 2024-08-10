# WDBReader

C++ library for reading "World of Warcraft" database files.

## Features

- Reads DBC and DB2 Files.
- Accepts multiple filesystems (MPQ, CASC, + more)
- Usable with WOWDBDefs
- Record structure determined at compile time or runtime.

Currently only tested with client versions: `1.12`, `2.4`, `3.3`, `8.3` & `10.2`
However should be able to work with any using WDBC, WDC3, WDC4 or WDC5 formats.

## Building

- Build with CMake
- Optionally use `vcpkg`
- Optionally link to StormLib & CascLib

## Example Usage

Available filesystems / sources:
```cpp
class NativeFileSystem; // native file system access.
class MemoryFileSource; // direct memory source.
class CASCFilesystem;   // CASC
class MPQFilesystem;    // MPQ

auto filesystem = FilesystemType(...);
auto source = filesystem.open(name);
```

DBC file reading:
```cpp
//depending on RecordType, a third param 'locale' may be needed. 
auto dbc = DBCFile<SchemaType, RecordType, FileSourceType>(Schema, DBCVersion);
//shorthand helpers
auto dbc = makeDBCFile<SourceType>(RuntimeSchema, DBCVersion, DBCLocale);
auto dbc = makeDBCFile<StaticRecordType, FileSourceType>(DBCVersion);
```

DB2 file read:
```cpp
//Single format DB2 loader
auto db2 = DB2File<DB2Format, SchemaType, RecordType, FileSourceType>(Schema);
//shorthand helpers (accepts multiple db2 formats, open and load get automatically called.)
auto db2 = makeDB2File<SchemaType, RecordType, FileSourceType>(Schema, Source);     
auto db2 = makeDB2File(RuntimeSchema, Source);
auto db2 = makeDB2File<StaticRecordType>(Source);
```

DB usage (applied to both DBC & DB2):
```cpp
auto db = ...;
db.open(source);
db.load();
// ...
db.size();
auto record = db[record_index];
for(const auto& rec : db) {
    if(rec.encryptionState != RecordEncryption::ENCRYPTED) {
        // rec.data safe.
    } else {
        // rec.data unsafe.
    }
}
```

Fixed DB records (compile time):
```cpp
struct RecordType : public FixedRecord<RecordType> {
    struct Data {
        //...
    } data;
    size_t recordIndex;
    RecordEncryption encryptionState;
    constexpr static Schema schema = Schema(
        //...
    );
};

RecordType rec = db[index];
rec.recordIndex;
rec.encryptionState;
rec.data.{field_name};
```

Runtime DB records (run time):
```cpp
class RuntimeRecord;
class RuntimeSchema;
RuntimeSchema schema = ...;
RuntimeRecord rec = db[index];
rec.recordIndex;
rec.encryptionState;
auto accessor = schema(rec);
for(const auto& element : accessor) {
    element.name;
    element.value;
    for(const auto& val : element.value) {
        std::visit([](auto v) {
            //...
        }, val);
    }
}

```
Schema type examples:
```cpp
// Fixed / static schema (modelfiledata)
constexpr static Schema schema = Schema(
    Field::value<decltype(data.fileDataId)>(Annotation().Id()),
    Field::value<decltype(data.flags)>(),
    Field::value<decltype(data.loadCount)>(),
    Field::value<decltype(data.modelResourcesId)>(Annotation().Relation())
);

// Runtime schema (guildtabardemblem)
RuntimeSchema({
    Field::value<uint32_t>(Annotation().Id().NonInline()),
    Field::value<uint32_t>(),
    Field::value<uint32_t>(),
    Field::value<uint32_t>(),
    Field::value<uint32_t>(Annotation().Relation().NonInline())
},{
    "id",
    "component",
    "color",
    "fileDataId",
    "emblemId"
})
```

## WOWDBDefs integration.

Record structures can be created at runtime with `.dbd` files. See the demo app:
```bash
demo.exe {wow_dir} {db_file_uri} {dbd_defs_name}
```
Specifically:
```cpp
auto definition = DBDReader::read(stream);
auto schema = makeSchema(definition, client.version);
```

## Compile time structures

Records and schema structures can be created with `schema_gen`, this creates C++ classes in the format expected for WDBReader fixed records. Usage as:
```bash
schema_gen.exe {version} {db_defs_name} {db_format} {output_path}
```

## Client detection
Detect installed client info for a specific path using supplied stratagies, built-in strategies include checking the .build.info file and exe file attributes.
```cpp
auto detected = Detector::all().detect(path);
detected.size();
detected[0].name;
detected[0].locale;
detected[0].version;
```
