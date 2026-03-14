# ason-cpp

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)](#)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

面向 [ASON](https://github.com/ason-lab/ason) 的 C++17 仅头文件实现。ASON 是一种适合紧凑结构化载荷的 Schema 驱动数据格式。

[English](README.md)

## 为什么用 ASON

ASON 只写一次 Schema，重复行以元组形式保存：

```json
[
  {"id": 1, "name": "Alice", "active": true},
  {"id": 2, "name": "Bob", "active": false}
]
```

```text
[{id@int,name@str,active@bool}]:(1,Alice,true),(2,Bob,false)
```

这可以减少重复键名、减小体积，同时保留清晰的类型结构。

## 特性

- 仅头文件，直接 `#include "ason.hpp"`
- 当前 API 是 `encode` / `decode`，不再是旧文档里的 `dump` / `load`
- 同时支持文本格式和二进制格式
- SIMD 优化解析，尽量零拷贝解码
- 支持 `std::optional`、`std::vector`、嵌套结构体，以及条目列表集合

## 快速开始

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

### 编码和解码单个结构体

```cpp
User user{1, "Alice", true};

std::string text = ason::encode(user);
// {id,name,active}:(1,Alice,true)

std::string typed = ason::encode_typed(user);
// {id@int,name@str,active@bool}:(1,Alice,true)

User decoded = ason::decode<User>(text);
```

### 如何表示键值集合

ASON C++ 已不再提供原生 map/dictionary 字段语法。
如需表达键值数据，请改用“条目结构体数组”：

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

### 编码和解码 vector

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

### 二进制往返

```cpp
std::string bin = ason::encode_bin(user);
User decoded = ason::decode_bin<User>(bin);
```

## 当前 API

| 函数 | 作用 |
| --- | --- |
| `ason::encode` / `ason::encode_typed` | 编码为文本 |
| `ason::decode<T>` | 从文本解码 |
| `ason::encode_pretty` / `ason::encode_pretty_typed` | 生成更易读的文本 |
| `ason::encode_bin` | 编码为二进制 |
| `ason::decode_bin<T>` | 从二进制解码 |

## 构建和运行

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/basic
./build/complex_example
./build/bench
ctest --test-dir build
```

## 最新基准

在当前这台机器上通过下面命令实测：

```bash
./build/bench
```

关键结果：

- 扁平 1,000 条记录：ASON 文本序列化 `11.66ms`，JSON `29.05ms`；反序列化 ASON `34.63ms`，JSON `44.75ms`
- 吞吐总结：ASON 文本序列化比 JSON 快 `2.49x`，反序列化快 `1.29x`
- 1,000 条扁平记录体积：JSON `121,675 B`，ASON 文本 `56,718 B`（缩小 `53%`），ASON 二进制 `74,454 B`（缩小 `39%`）
- 二进制解码尤其明显：在 1,000 条扁平记录上 `5.97ms` 对比 JSON `44.75ms`，约快 `7.50x`

对于 100 条深层 company 数据，ASON 文本大小约 `170,183 B`，JSON 约 `431,612 B`，而且文本解码快 `2.45x`。

## Contributors

- [Athan](https://github.com/athxx)

## 许可证

MIT
