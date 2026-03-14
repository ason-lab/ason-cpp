# ason-cpp

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)](#)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Header-only C++17 support for [ASON](https://github.com/ason-lab/ason), a schema-driven data format for compact structured payloads.

[中文文档](README_CN.md)

## Why ASON

ASON writes the schema once and stores repeated rows as tuples:

```json
[
  {"id": 1, "name": "Alice", "active": true},
  {"id": 2, "name": "Bob", "active": false}
]
```

```text
[{id@int,name@str,active@bool}]:(1,Alice,true),(2,Bob,false)
```

That cuts repeated keys, reduces payload size, and keeps typed structure visible.

## Highlights

- Header-only, just `#include "ason.hpp"`
- Current API uses `encode` / `decode`, not the older `dump` / `load` names
- Text and binary formats
- SIMD-aware parser and zero-copy-friendly decoding
- Support for `std::optional`, `std::vector`, nested structs, and entry-list collections

## Quick Start

```cpp
#include "ason.hpp"

struct User {
  int64_t id = 0;
  std::string name;
  bool active = false;
};

ASON_FIELDS(User,
    (id, "id", "int"),
    (name, "name", "str"),
    (active, "active", "bool"))
```

### Encode and decode a struct

```cpp
User user{1, "Alice", true};

std::string text = ason::encode(user);
// {id,name,active}:(1,Alice,true)

std::string typed = ason::encode_typed(user);
// {id@int,name@str,active@bool}:(1,Alice,true)

User decoded = ason::decode<User>(text);
```

### Modeling key-value collections

ASON C++ no longer provides a native map/dictionary field syntax.
Model key-value data as arrays of entry structs instead:

```cpp
struct EnvEntry {
  std::string key;
  std::string value;
};

ASON_FIELDS(EnvEntry,
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

auto text = ason::encode(users);
// [{id,name,active}]:(1,Alice,true),(2,Bob,false)

auto typed = ason::encode_typed(users);
auto decoded = ason::decode<std::vector<User>>(text);
```

### Binary roundtrip

```cpp
std::string bin = ason::encode_bin(user);
User decoded = ason::decode_bin<User>(bin);
```

## Current API

| Function | Purpose |
| --- | --- |
| `ason::encode` / `ason::encode_typed` | Encode to text |
| `ason::decode<T>` | Decode from text |
| `ason::encode_pretty` / `ason::encode_pretty_typed` | Pretty text output |
| `ason::encode_bin` | Encode to binary |
| `ason::decode_bin<T>` | Decode from binary |

## Build and Run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/basic
./build/complex_example
./build/bench
ctest --test-dir build
```

## Latest Benchmarks

Measured on this machine with:

```bash
./build/bench
```

Headline numbers:

- Flat 1,000-record dataset: ASON text serialize `11.66ms` vs JSON `29.05ms`, deserialize `34.63ms` vs JSON `44.75ms`
- Throughput summary: ASON text was `2.49x` faster than JSON for serialize and `1.29x` faster for deserialize
- Size summary for 1,000 flat records: JSON `121,675 B`, ASON text `56,718 B` (`53%` smaller), ASON binary `74,454 B` (`39%` smaller)
- Binary decode was especially strong: `5.97ms` vs JSON `44.75ms` on flat 1,000-record data, or `7.50x` faster

For 100 deep company objects, ASON text serialized `170,183 B` vs JSON `431,612 B` and decoded `2.45x` faster.

## Contributors

- [Athan](https://github.com/athxx)

## License

MIT
