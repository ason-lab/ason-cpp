# asun-cpp

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)](#)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Header-only C++17 support for [ASUN](https://github.com/asunLab/asun), a schema-driven data format for compact structured payloads.

[中文文档](https://github.com/asunLab/asun-cpp/blob/main/README_CN.md)

## Why ASUN?

**json**

Standard JSON repeats every field name in every record. When you send structured data to an LLM, over an API, or across services, that repetition wastes tokens, bytes, and attention:

```json
[
  { "id": 1, "name": "Alice", "active": true },
  { "id": 2, "name": "Bob", "active": false },
  { "id": 3, "name": "Carol", "active": true }
]
```

**asun**

ASUN declares the schema **once** and streams data as compact tuples:

```asun
[{id, name, active}]:
  (1,Alice,true),
  (2,Bob,false),
  (3,Carol,true)
```

**Fewer tokens. Smaller payloads. Clearer structure, and faster parsing than repeated-object JSON.**

---

## Highlights

- Header-only, just `#include "asun.hpp"`
- Current API uses `encode` / `decode`, not the older `dump` / `load` names
- Text and binary formats
- SIMD-aware parser and zero-copy-friendly decoding
- Support for `std::optional`, `std::vector`, nested structs, and entry-list collections

## Quick Start

```cpp
#include "asun.hpp"

struct User {
  int64_t id = 0;
  std::string name;
  bool active = false;
};

ASUN_FIELDS(User,
    (id, "id", "int"),
    (name, "name", "str"),
    (active, "active", "bool"))
```

### Encode and decode a struct

```cpp
User user{1, "Alice", true};

std::string text = asun::encode(user);
// {id,name,active}:(1,Alice,true)

std::string typed = asun::encode_typed(user);
// {id@int,name@str,active@bool}:(1,Alice,true)

User decoded = asun::decode<User>(text);
```

### Modeling key-value collections

ASUN C++ no longer provides a native map/dictionary field syntax.
Model key-value data as arrays of entry structs instead:

```cpp
struct EnvEntry {
  std::string key;
  std::string value;
};

ASUN_FIELDS(EnvEntry,
    (key, "key", "str"),
    (value, "value", "str"))

struct ServiceConfig {
  std::vector<EnvEntry> env;
};
```

### Encode and decode a vector

```cpp
std::vector<User> users = {
    {1, "Alice", true},
    {2, "Bob", false},
};

auto text = asun::encode(users);
// [{id,name,active}]:(1,Alice,true),(2,Bob,false)

auto typed = asun::encode_typed(users);
auto decoded = asun::decode<std::vector<User>>(text);
```

### Binary roundtrip

```cpp
std::string bin = asun::encode_bin(user);
User decoded = asun::decode_bin<User>(bin);
```

## Current API

| Function                                            | Purpose            |
| --------------------------------------------------- | ------------------ |
| `asun::encode` / `asun::encode_typed`               | Encode to text     |
| `asun::decode<T>`                                   | Decode from text   |
| `asun::encode_pretty` / `asun::encode_pretty_typed` | Pretty text output |
| `asun::encode_bin`                                  | Encode to binary   |
| `asun::decode_bin<T>`                               | Decode from binary |

## Build and Run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/basic
./build/complex_example
./build/bench
ctest --test-dir build
```

## Package Manager Readiness

`asun-cpp` can now be consumed as a standard header-only CMake package:

```cmake
find_package(asun CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE asun::asun)
```

### Conan

The repository ships a ready-to-use [conanfile.py](conanfile.py).

```bash
cd asun-cpp
conan create . --build=missing
```

### vcpkg

An overlay port is included under [vcpkg/ports/asun-cpp](vcpkg/ports/asun-cpp).

```bash
vcpkg install asun-cpp --overlay-ports=/path/to/asun-cpp/vcpkg/ports
```

### Homebrew

A formula template is included at [homebrew/asun-cpp.rb](homebrew/asun-cpp.rb).
Before publishing to a tap, replace `REPLACE_WITH_RELEASE_SHA256` with the actual release tarball hash.

## Latest Benchmarks

Measured on this machine with:

```bash
./build/bench
```

Headline numbers:

- Flat 1,000-record dataset: ASUN text serialize `11.66ms` vs JSON `29.05ms`, deserialize `34.63ms` vs JSON `44.75ms`
- Throughput summary: ASUN text was `2.49x` faster than JSON for serialize and `1.29x` faster for deserialize
- Size summary for 1,000 flat records: JSON `121,675 B`, ASUN text `56,718 B` (`53%` smaller), ASUN binary `74,454 B` (`39%` smaller)
- Binary decode was especially strong: `5.97ms` vs JSON `44.75ms` on flat 1,000-record data, or `7.50x` faster

For 100 deep company objects, ASUN text serialized `170,183 B` vs JSON `431,612 B` and decoded `2.45x` faster.

## Contributors

- [Athan](https://github.com/athxx)

## License

MIT
