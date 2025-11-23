# SecScoreDB - A Lightweight "Database" for SecScore

## Overview

SecScoreDB is a lightweight, file-backed data storage system designed specifically for the SecScore project. It manages Students, Groups, and Events with support for dynamic fields and type safety.

Written in modern C++ (C++23), it leverages the `cereal` library for serialization and uses contemporary language features such as concepts, template metaprogramming, and (optionally) ranges.

## Features

- **Lightweight File Storage**: Binary persistence for efficient, cross‑platform data handling.
- **Dynamic Field Support**: Define flexible schemas via `SchemaDef` and access fields safely through `DynamicWrapper`.
- **Type Safety**: Compile‑time and runtime checks prevent mismatched field access.
- **Modern C++**: Employs concepts, `constexpr`, and clean template utilities.
- **Automatic Serialization**: Uses `cereal` for seamless save/load of entities.
- **Lambda Queries**: Filter and retrieve entities with expressive predicates.

## Core Components

### Main Classes
1. `SecScoreDB` – Central database interface.
2. `Student` – Student entity with dynamic metadata.
3. `Group` – Group entity for organizing students.
4. `Event` – Records historical operations (auditing / change tracking).
5. `DynamicWrapper<T>` – Provides schema‑validated dynamic field access (`wrapper["field"]`).
6. `DataBaseFile` – Helper for low‑level file persistence.

### Dynamic Field System
A schema (`SchemaDef`) defines dynamic metadata fields and their types:
- `Int` (integer)
- `Double` (floating‑point)
- `String` (UTF‑8 text)

Usage pattern:
```cpp
SchemaDef stuSchema{{"name", FieldType::String}, {"age", FieldType::Int}, {"score", FieldType::Double}};
SecScoreDB db(path);
db.initStudentSchema(stuSchema);
auto s = db.createStudent(1001);
s["name"] = std::string("Alice");
s["age"] = 19;
s["score"] = 95.5;
std::string name = (std::string)s["name"]; // type‑checked
int age = (int)s["age"];                  // throws if schema/type mismatch
```

## Build & Installation

### Requirements
- C++23‑capable compiler (MSVC 19.3x+ / Visual Studio 2022 recommended, or GCC 13+, Clang 17+)
- CMake ≥ 3.26
- [vcpkg](https://github.com/microsoft/vcpkg) for dependency management

### Dependencies
- `cereal`
- `nlohmann-json`

### Windows Example (PowerShell)
```powershell
# Clone repository
git clone <repository-url>
cd SecScoreDB

# (Optional) Bootstrap vcpkg if not already available
# ./vcpkg/bootstrap-vcpkg.bat

# Configure (replace <vcpkg-root> with the path to your vcpkg clone)
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="<vcpkg-root>/scripts/buildsystems/vcpkg.cmake" -G Ninja

# Build
cmp-ninja; cmake --build build --target SecScoreDBApp -j 6
```
*(If not using Ninja, omit `-G Ninja` and rely on the default generator.)*

### Linux/macOS Example
```bash
git clone <repository-url>
cd SecScoreDB
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
cmake --build build --target SecScoreDBApp -j$(nproc)
```

## Test & Usage Example
See `tests/main.cpp` for a comprehensive scenario covering:
- Schema initialization
- Dynamic field assignment and retrieval
- Lambda queries and filtered deletion
- Event creation and logical erase
- Persistence across database re‑open

Run (after build):
```bash
./build/SecScoreDBApp   # Linux/macOS
# or on Windows (Ninja/VS out dir)
./build/SecScoreDBApp.exe
```

## Error Handling Philosophy
- Field type mismatches throw `std::runtime_error`.
- Numeric conversions validate full consumption of the source string (rejects partial parses like "123abc").
- Missing fields or schema violations surface early via exceptions.

## Roadmap / Possible Extensions
- Additional field types (Bool, Date/Time, Enum)
- Indexing and secondary query acceleration
- Transaction journaling & WAL
- Incremental snapshot merging
- Optional JSON export/import layer (leveraging `nlohmann-json`)

## License
Licensed under GNU Lesser General Public License v2.1. See [LICENSE](LICENSE) for details.

## Contributing
Issues and Pull Requests are welcome. Please:
1. Keep changes focused.
2. Add/adjust tests for new behaviors.
3. Document new public APIs in both `README.md` and `README_EN.md`.

---
*This English README mirrors the content of the Chinese `README.md` and adds minor clarifications for international users.*

