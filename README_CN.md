# asun-cpp

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)](#)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

面向 [ASUN](https://github.com/asunLab/asun) 的 C++17 仅头文件实现。ASUN 是一种适合紧凑结构化载荷的 Schema 驱动数据格式。

[English](https://github.com/asunLab/asun-cpp/blob/main/README.md)

## 为什么选择 ASUN

**json**

标准 JSON 会在每条记录里重复所有字段名。无论是发给 LLM、通过 API 传输，还是服务之间交换数据，这种重复都会浪费 Token、带宽和阅读成本：

```json
[
  { "id": 1, "name": "Alice", "active": true },
  { "id": 2, "name": "Bob", "active": false },
  { "id": 3, "name": "Carol", "active": true }
]
```

**asun**

ASUN 只声明 **一次** Schema，后续每一行只保留值：

```asun
[{id, name, active}]:
  (1,Alice,true),
  (2,Bob,false),
  (3,Carol,true)
```

**这通常意味着更少的 token、更小的体积，更清晰的结构, 以及比重复键名 JSON 更快的解析。**

---

## 特性

- 仅头文件，直接 `#include "asun.hpp"`
- 当前 API 是 `encode` / `decode`，不再是旧文档里的 `dump` / `load`
- 同时支持文本格式和二进制格式
- SIMD 优化解析，尽量零拷贝解码
- 支持 `std::optional`、`std::vector`、嵌套结构体，以及条目列表集合

## 快速开始

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

### 编码和解码单个结构体

```cpp
User user{1, "Alice", true};

std::string text = asun::encode(user);
// {id,name,active}:(1,Alice,true)

std::string typed = asun::encode_typed(user);
// {id@int,name@str,active@bool}:(1,Alice,true)

User decoded = asun::decode<User>(text);
```

### 如何表示键值集合

ASUN C++ 已不再提供原生 map/dictionary 字段语法。
如需表达键值数据，请改用“条目结构体数组”：

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

### 编码和解码 vector

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

### 二进制往返

```cpp
std::string bin = asun::encode_bin(user);
User decoded = asun::decode_bin<User>(bin);
```

## 当前 API

| 函数                                                | 作用             |
| --------------------------------------------------- | ---------------- |
| `asun::encode` / `asun::encode_typed`               | 编码为文本       |
| `asun::decode<T>`                                   | 从文本解码       |
| `asun::encode_pretty` / `asun::encode_pretty_typed` | 生成更易读的文本 |
| `asun::encode_bin`                                  | 编码为二进制     |
| `asun::decode_bin<T>`                               | 从二进制解码     |

## 构建和运行

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/basic
./build/complex_example
./build/bench
ctest --test-dir build
```

## 包管理器接入

`asun-cpp` 现在可以作为标准的 header-only CMake package 使用：

```cmake
find_package(asun CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE asun::asun)
```

### Conan

仓库内已经提供可直接使用的 [conanfile.py](conanfile.py)。

```bash
cd asun-cpp
conan create . --build=missing
```

### vcpkg

仓库内已经提供 overlay port，位置在 [vcpkg/ports/asun-cpp](vcpkg/ports/asun-cpp)。

```bash
vcpkg install asun-cpp --overlay-ports=/path/to/asun-cpp/vcpkg/ports
```

### Homebrew

仓库内已经提供 formula 模板 [homebrew/asun-cpp.rb](homebrew/asun-cpp.rb)。
正式发布到 tap 前，需要把里面的 `REPLACE_WITH_RELEASE_SHA256` 替换成真实 release tarball 的 `sha256`。

## 最新基准

在当前这台机器上通过下面命令实测：

```bash
./build/bench
```

关键结果：

- 扁平 1,000 条记录：ASUN 文本序列化 `11.66ms`，JSON `29.05ms`；反序列化 ASUN `34.63ms`，JSON `44.75ms`
- 吞吐总结：ASUN 文本序列化比 JSON 快 `2.49x`，反序列化快 `1.29x`
- 1,000 条扁平记录体积：JSON `121,675 B`，ASUN 文本 `56,718 B`（缩小 `53%`），ASUN 二进制 `74,454 B`（缩小 `39%`）
- 二进制解码尤其明显：在 1,000 条扁平记录上 `5.97ms` 对比 JSON `44.75ms`，约快 `7.50x`

对于 100 条深层 company 数据，ASUN 文本大小约 `170,183 B`，JSON 约 `431,612 B`，而且文本解码快 `2.45x`。

## Contributors

- [Athan](https://github.com/athxx)

## 许可证

MIT
