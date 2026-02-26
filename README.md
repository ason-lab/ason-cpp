# ason-cpp

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)](#)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A high-performance, header-only C++17 library for [ASON](https://github.com/athxx/ason) (Array-Schema Object Notation) — a token-efficient, schema-driven data format designed for LLM interactions and large-scale data transmission. SIMD-accelerated (NEON / SSE2), zero-copy parsing, zero external dependencies.

[中文文档](README_CN.md)

## What is ASON?

ASON separates **schema** from **data**, eliminating repetitive keys found in JSON. The schema is declared once, and data rows carry only values:

```text
JSON (100 tokens):
{"users":[{"id":1,"name":"Alice","active":true},{"id":2,"name":"Bob","active":false}]}

ASON (~35 tokens, 65% saving):
[{id:int, name:str, active:bool}]:(1,Alice,true),(2,Bob,false)
```

| Aspect                | JSON         | ASON               |
| --------------------- | ------------ | ------------------ |
| Token efficiency      | 100%         | 30–70% ✓           |
| Key repetition        | Every object | Declared once ✓    |
| Human readable        | Yes          | Yes ✓              |
| Nested structs        | ✓            | ✓                  |
| Type annotations      | No           | Optional ✓         |
| Serialization speed   | 1x           | **~2.4x faster** ✓ |
| Deserialization speed | 1x           | **~1.9x faster** ✓ |
| Data size             | 100%         | **40–50%** ✓       |

## Features

- **Header-only** — single `#include "ason.hpp"`, no build steps or linking required
- **Zero dependencies** — standard library only, no Boost, no third-party libs
- **SIMD-accelerated** — NEON (ARM64/Apple Silicon) and SSE2 (x86_64) with scalar fallback
- **Zero-copy parsing** — `string_view`-based parsing, minimal allocations
- **Compile-time reflection** — `ASON_FIELDS` macro for struct registration, up to 20 fields
- **Full type support** — bool, all integers (i8–i64, u8–u64), float, double, string, `optional<T>`, `vector<T>`, `unordered_map<K,V>`, nested structs
- **Schema-driven** — schema declared once, unlimited data rows
- **Type annotations** — optional type hints in schema for documentation and LLM prompts

## Quick Start

### 1. Copy the Header

```bash
cp include/ason.hpp /your/project/include/
```

### 2. Define Your Struct

```cpp
#include "ason.hpp"

struct User {
    int64_t id = 0;
    std::string name;
    bool active = false;
};

ASON_FIELDS(User,
    (id,     "id",     "int"),
    (name,   "name",   "str"),
    (active, "active", "bool"))
```

### 3. Serialize & Deserialize

```cpp
// Serialize
User user{1, "Alice", true};
std::string s = ason::dump(user);
// → "{id,name,active}:(1,Alice,true)"

// Serialize with type annotations
std::string s2 = ason::dump_typed(user);
// → "{id:int,name:str,active:bool}:(1,Alice,true)"

// Deserialize
auto user2 = ason::load<User>(s);
assert(user2.id == 1);
assert(user2.name == "Alice");
```

### 4. Vec Serialization (Schema-Driven)

For `vector<T>`, ASON writes the schema **once** and emits each element as a compact tuple — the key advantage over JSON:

```cpp
std::vector<User> users = {
    {1, "Alice", true},
    {2, "Bob", false},
    {3, "Carol Smith", true},
};

// Schema written once, data as tuples
auto s = ason::dump_vec(users);
// → "[{id,name,active}]:(1,Alice,true),(2,Bob,false),(3,Carol Smith,true)"

// Deserialize
auto users2 = ason::load_vec<User>(s);
assert(users2.size() == 3);
```

## Supported Types

| Type                      | ASON Representation | Example                       |
| ------------------------- | ------------------- | ----------------------------- |
| Integers (i8–i64, u8–u64) | Plain number        | `42`, `-100`                  |
| Floats (float, double)    | Decimal number      | `3.14`, `-0.5`                |
| Bool                      | Literal             | `true`, `false`               |
| String                    | Unquoted or quoted  | `Alice`, `"Carol Smith"`      |
| `std::optional<T>`        | Value or empty      | `hello` or _(blank)_ for None |
| `std::vector<T>`          | `[v1,v2,v3]`        | `[rust,go,python]`            |
| `std::unordered_map<K,V>` | `[(k1,v1),(k2,v2)]` | `[(age,30),(score,95)]`       |
| Nested struct             | `(field1,field2)`   | `(Engineering,500000)`        |
| Char                      | Single character    | `A`                           |

### Nested Structs

```cpp
struct Dept { std::string title; };
ASON_FIELDS(Dept, (title, "title", "str"))

struct Employee {
    std::string name;
    Dept dept;
};
ASON_FIELDS(Employee,
    (name, "name", "str"),
    (dept, "dept", "{title:str}"))

// Schema reflects nesting:
// {name:str,dept:{title:str}}:(Alice,(Engineering))
```

### Optional Fields

```cpp
struct Item {
    int64_t id = 0;
    std::optional<std::string> label;
};
ASON_FIELDS(Item,
    (id, "id", "int"),
    (label, "label", "str"))

// With value:   {id,label}:(1,hello)
// With None:    {id,label}:(1,)
```

### Arrays & Maps

```cpp
struct Tagged {
    std::string name;
    std::vector<std::string> tags;
};
ASON_FIELDS(Tagged,
    (name, "name", "str"),
    (tags, "tags", "[str]"))
// {name,tags}:(Alice,[rust,go,python])

struct Profile {
    std::string name;
    std::unordered_map<std::string, int64_t> attrs;
};
ASON_FIELDS(Profile,
    (name, "name", "str"),
    (attrs, "attrs", "map[str,int]"))
// {name,attrs}:(Alice,[(age,30),(score,95)])
```

### Type Annotations (Optional)

Both annotated and unannotated schemas are fully equivalent — the deserializer handles them identically:

```text
// Without annotations (default output of dump / dump_vec)
{id,name,salary,active}:(1,Alice,5000.50,true)

// With annotations (output of dump_typed / dump_vec_typed)
{id:int,name:str,salary:float,active:bool}:(1,Alice,5000.50,true)
```

### Comments

```text
/* user list */
[{id:int, name:str, active:bool}]:(1,Alice,true),(2,Bob,false)
```

### Multiline Format

```text
[{id:int, name:str, active:bool}]:
  (1, Alice, true),
  (2, Bob, false),
  (3, "Carol Smith", true)
```

## API Reference

| Function                    | Description                                                   |
| --------------------------- | ------------------------------------------------------------- |
| `ason::dump(obj)`           | Serialize a struct → unannotated schema `{id,name}:`          |
| `ason::dump_typed(obj)`     | Serialize a struct → annotated schema `{id:int,name:str}:`    |
| `ason::dump_vec(vec)`       | Serialize a vector → unannotated schema                       |
| `ason::dump_vec_typed(vec)` | Serialize a vector → annotated schema                         |
| `ason::load<T>(str)`        | Deserialize a struct (accepts both annotated and unannotated) |
| `ason::load_vec<T>(str)`    | Deserialize a vector (accepts both annotated and unannotated) |

### Reflection Macro

```cpp
ASON_FIELDS(StructName,
    (field_member, "schema_name", "type_hint"),
    ...)
```

- `field_member` — the C++ struct member name
- `"schema_name"` — the field name in the ASON schema
- `"type_hint"` — type annotation string (used by `dump_typed`)

Supports up to **20 fields** per struct.

## Performance

Benchmarked on Apple Silicon M-series (arm64, NEON SIMD), `-O3` Release mode, comparing ASON against a minimal inline JSON serializer/deserializer:

### Serialization (ASON is 2.2–2.8x faster)

| Scenario            | JSON     | ASON     | Speedup   |
| ------------------- | -------- | -------- | --------- |
| Flat struct × 100   | 3.41 ms  | 1.35 ms  | **2.52x** |
| Flat struct × 500   | 13.37 ms | 4.85 ms  | **2.76x** |
| Flat struct × 1000  | 19.81 ms | 7.60 ms  | **2.61x** |
| Flat struct × 5000  | 90.36 ms | 36.98 ms | **2.44x** |
| 5-level deep × 10   | 6.12 ms  | 2.51 ms  | **2.43x** |
| 5-level deep × 50   | 29.90 ms | 13.56 ms | **2.21x** |
| 5-level deep × 100  | 60.94 ms | 28.20 ms | **2.16x** |
| Large payload (10k) | 19.44 ms | 7.76 ms  | **2.51x** |

### Deserialization (ASON is 1.3–2.1x faster)

| Scenario            | JSON      | ASON     | Speedup   |
| ------------------- | --------- | -------- | --------- |
| Flat struct × 100   | 3.18 ms   | 1.72 ms  | **1.85x** |
| Flat struct × 500   | 11.50 ms  | 5.61 ms  | **2.05x** |
| Flat struct × 1000  | 17.99 ms  | 9.71 ms  | **1.85x** |
| Flat struct × 5000  | 101.41 ms | 55.73 ms | **1.82x** |
| 5-level deep × 10   | 9.18 ms   | 5.06 ms  | **1.81x** |
| 5-level deep × 50   | 44.85 ms  | 27.95 ms | **1.60x** |
| 5-level deep × 100  | 91.41 ms  | 67.97 ms | **1.34x** |
| Large payload (10k) | 21.60 ms  | 11.72 ms | **1.84x** |

### Throughput

| Metric                | JSON           | ASON            | Speedup   |
| --------------------- | -------------- | --------------- | --------- |
| Serialize records/s   | 5.5M records/s | 13.1M records/s | **2.39x** |
| Serialize MB/s        | 639 MB/s       | 711 MB/s        | 1.11x     |
| Deserialize records/s | 5.0M records/s | 9.3M records/s  | **1.85x** |

### Size Savings

| Scenario            | JSON     | ASON     | Saving  |
| ------------------- | -------- | -------- | ------- |
| Flat struct × 100   | 12,071 B | 5,612 B  | **54%** |
| Flat struct × 1000  | 121 KB   | 57 KB    | **53%** |
| 5-level deep × 10   | 42,594 B | 16,893 B | **60%** |
| 5-level deep × 100  | 431 KB   | 175 KB   | **60%** |
| Large payload (10k) | 1.2 MB   | 0.6 MB   | **53%** |

### Why is ASON Faster?

1. **Zero key-hashing** — Schema is parsed once; data fields are mapped by position index O(1), no per-row key string hashing.
2. **Schema-driven parsing** — The deserializer knows the expected type of each field, enabling direct parsing instead of runtime type inference. CPU branch prediction hits ~100%.
3. **Minimal memory allocation** — All data rows share one schema reference. No repeated key string allocation.
4. **SIMD string scanning** — NEON/SSE2 vectorized character scanning for quoting detection, escape sequences, and quoted string parsing. Processes 16 bytes per cycle.
5. **Fast number formatting** — Two-digit lookup table for integer serialization; fast-path float formatting for common cases (integers, 1–2 decimal places).

## Test Suite

36 comprehensive tests covering all functionality:

### Serialization & Deserialization (4 tests)

- Simple struct roundtrip
- Typed schema roundtrip (with type annotations)
- Vec roundtrip (schema-driven)
- Vec typed roundtrip

### Optional Fields (4 tests)

- Present value parsing
- Absent value (None/nullopt)
- Optional field serialization
- Empty optional between commas `(1,,3)`

### Collections (5 tests)

- Vector field parsing and roundtrip
- Empty vector handling
- Nested vectors `[[1,2],[3,4]]`
- Map field parsing `[(key,val)]`
- Map roundtrip

### Nested Structs (3 tests)

- Nested struct parsing
- Nested struct roundtrip
- 3-level deep nesting (DeepC > DeepB > DeepA)

### String Handling (7 tests)

- Quoted string parsing
- Full escape sequence support (`\"`, `\\`, `\n`, `\t`, `\,`, `\(`, `\)`, `\[`, `\]`)
- Automatic quoting detection (SIMD-accelerated)
- Unquoted string trimming
- Strings with internal spaces
- Leading/trailing space quoting
- Backslash escape roundtrip

### Number Handling (4 tests)

- Float precision (double, float)
- Integer-valued float formatting (e.g., `42.0`)
- Negative numbers (integers and floats)
- Large unsigned integers (uint64 max)

### Parsing Features (6 tests)

- Boolean values (`true`/`false`)
- Block comment parsing `/* ... */`
- Whitespace handling
- Multiline format parsing
- Typed schema parsing (with type annotations)
- Schema field mismatch (extra/missing fields)

### Edge Cases (3 tests)

- Error handling (invalid input throws `ason::Error`)
- Empty vector serialization `[]`
- Vector of strings with special characters

### Complex Examples (18 tests)

- Nested struct with arrays
- Vec with nested structs
- Map/dict fields
- Escaped strings roundtrip
- All-types struct (16 fields: bool, i8–i64, u8–u64, float, double, string, optional, vectors)
- 5-level deep nesting (Country > Region > City > District > Street > Building)
- 7-level deep nesting (Universe > Galaxy > SolarSystem > Planet > Continent > Nation > State)
- Complex config struct (nested + map + optional)
- 100-entry large structure roundtrip
- Typed serialization variants
- Edge cases (empty vec, special chars, bool-like strings, number-like strings)
- Triple-nested arrays `[[[1,2],[3,4]]]`
- Comment parsing in real data

## Build & Run

```bash
# Build all targets (Release mode for best performance)
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run tests
./ason_test

# Run examples
./basic              # 11 basic examples
./complex_example    # 18 complex examples

# Run benchmark
./bench              # ASON vs JSON comprehensive benchmark
```

## ASON Binary Format

ASON-BIN is a compact binary serialization format that shares the same schema as ASON text. It uses little-endian fixed-width encoding with no field names — purely positional — for maximum throughput.

### Wire Format

| Type            | Encoding                              |
| --------------- | ------------------------------------- |
| `bool`          | 1 byte (0 or 1)                       |
| `int8/16/32/64` | 1/2/4/8 bytes LE                      |
| `float/double`  | 4/8 bytes LE (IEEE 754 bit-cast)      |
| `string`        | u32 length (LE) + UTF-8 bytes         |
| `string_view`   | same — load points into source buffer |
| `vector<T>`     | u32 count (LE) + N × element encoding |
| struct          | fields encoded sequentially           |

### API

```cpp
#include "ason.hpp"

struct User {
    std::string name;
    int64_t     age    = 0;
    double      score  = 0.0;
    bool        active = false;
};
ASON_FIELDS(User,
    (name,   "name",   "str"),
    (age,    "age",    "int"),
    (score,  "score",  "float"),
    (active, "active", "bool"))

// Serialize to binary
std::string bin = ason::dump_bin(user);

// Deserialize
User user2 = ason::load_bin<User>(bin);

// Batch (vector of structs)
std::vector<User> users = { ... };
std::string batch = ason::dump_bin(users);
auto users2 = ason::load_bin<std::vector<User>>(batch);
```

#### Zero-Copy Deserialization

Use `std::string_view` fields to avoid heap allocation for strings:

```cpp
struct Packet {
    std::string_view id;
    int64_t          seq = 0;
    std::string_view payload;
};
// (ASON_FIELDS for parsing; for binary-only zero-copy use load_bin_value directly)

const char* pos = bin.data();
const char* end = bin.data() + bin.size();
std::string_view id_view, payload_view;
int64_t seq = 0;
ason::load_bin_value(pos, end, id_view);      // zero-copy — points into `bin`
ason::load_bin_value(pos, end, seq);
ason::load_bin_value(pos, end, payload_view);
```

### Performance (macOS arm64 NEON, 100 iterations)

| Benchmark             | JSON Ser   | BIN Ser    | Ser Ratio  | JSON De    | BIN De     | De Ratio  | Size Saving |
| --------------------- | ---------- | ---------- | ---------- | ---------- | ---------- | --------- | ----------- |
| Flat struct ×100      | 2.28 ms    | 0.58 ms    | **3.90x**  | 2.25 ms    | 0.24 ms    | **9.35x** | 38%         |
| Flat struct ×1000     | 17.72 ms   | 4.73 ms    | **3.75x**  | 18.92 ms   | 2.07 ms    | **9.14x** | 39%         |
| All-types ×100        | 2.12 ms    | 1.10 ms    | **1.93x**  | 3.17 ms    | 0.80 ms    | **3.96x** | 27%         |
| 5-level deep ×100     | 60.30 ms   | 16.27 ms   | **3.71x**  | 86.88 ms   | 10.10 ms   | **8.60x** | 48%         |

> Deep nested structs see the largest gains: up to **9× faster** deserialization. Trivial numeric arrays use bulk `memcpy` via SIMD implicitly.

## ASON Format Specification

See the full [ASON Spec](https://github.com/athxx/ason/blob/main/docs/ASON_SPEC_CN.md) for syntax rules, BNF grammar, escape rules, type system, and LLM integration best practices.

### Syntax Quick Reference

| Element       | Schema                      | Data                |
| ------------- | --------------------------- | ------------------- |
| Object        | `{field1:type,field2:type}` | `(val1,val2)`       |
| Array         | `field:[type]`              | `[v1,v2,v3]`        |
| Object array  | `field:[{f1:type,f2:type}]` | `[(v1,v2),(v3,v4)]` |
| Map           | `field:map[K,V]`            | `[(k1,v1),(k2,v2)]` |
| Nested object | `field:{f1:type,f2:type}`   | `(v1,(v3,v4))`      |
| Null          | —                           | _(blank)_           |
| Empty string  | —                           | `""`                |
| Comment       | —                           | `/* ... */`         |

## License

MIT
