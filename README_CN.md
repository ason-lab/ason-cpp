# ason-cpp

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)](#)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

高性能、仅头文件的 C++17 [ASON](https://github.com/athxx/ason)（Array-Schema Object Notation）序列化/反序列化库 —— 面向 LLM 交互和大规模数据传输的高效序列化格式。SIMD 加速（NEON / SSE2），零拷贝解析，零外部依赖。

[English](README.md)

## 什么是 ASON？

ASON 将 **Schema** 与 **数据** 分离，消除了 JSON 中每个对象都重复出现 Key 的冗余。Schema 只声明一次，数据行仅保留纯值：

```text
JSON (100 tokens):
{"users":[{"id":1,"name":"Alice","active":true},{"id":2,"name":"Bob","active":false}]}

ASON (~35 tokens, 节省 65%):
[{id:int, name:str, active:bool}]:(1,Alice,true),(2,Bob,false)
```

| 方面         | JSON         | ASON             |
| ------------ | ------------ | ---------------- |
| Token 效率   | 100%         | 30–70% ✓         |
| Key 重复     | 每个对象都有 | 声明一次 ✓       |
| 人类可读     | 是           | 是 ✓             |
| 嵌套结构     | ✓            | ✓                |
| 类型注解     | 无           | 可选 ✓           |
| 序列化速度   | 1x           | **~2.4x 更快** ✓ |
| 反序列化速度 | 1x           | **~1.9x 更快** ✓ |
| 数据体积     | 100%         | **40–50%** ✓     |

## 特性

- **仅头文件** —— 只需 `#include "ason.hpp"`，无需编译步骤或链接
- **零依赖** —— 仅使用标准库，无 Boost，无第三方库
- **SIMD 加速** —— 支持 NEON（ARM64/Apple Silicon）和 SSE2（x86_64），带标量回退
- **零拷贝解析** —— 基于 `string_view` 的解析，最小化内存分配
- **编译期反射** —— `ASON_FIELDS` 宏注册结构体，最多支持 20 个字段
- **完整类型支持** —— bool、所有整数类型（i8–i64、u8–u64）、float、double、string、`optional<T>`、`vector<T>`、`unordered_map<K,V>`、嵌套结构体
- **Schema 驱动** —— Schema 声明一次，数据行无限扩展
- **类型注解** —— 可选的类型提示，用于文档和 LLM prompt

## 快速开始

### 1. 复制头文件

```bash
cp include/ason.hpp /your/project/include/
```

### 2. 定义结构体

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

### 3. 序列化与反序列化

```cpp
// 序列化
User user{1, "Alice", true};
std::string s = ason::dump(user);
// → "{id,name,active}:(1,Alice,true)"

// 带类型注解的序列化
std::string s2 = ason::dump_typed(user);
// → "{id:int,name:str,active:bool}:(1,Alice,true)"

// 反序列化
auto user2 = ason::load<User>(s);
assert(user2.id == 1);
assert(user2.name == "Alice");
```

### 4. Vec 序列化（Schema 驱动）

对于 `vector<T>`，ASON 只写入一次 Schema，每个元素作为紧凑元组输出 —— 这是相比 JSON 的核心优势：

```cpp
std::vector<User> users = {
    {1, "Alice", true},
    {2, "Bob", false},
    {3, "Carol Smith", true},
};

// Schema 只写一次，数据为元组
auto s = ason::dump_vec(users);
// → "[{id,name,active}]:(1,Alice,true),(2,Bob,false),(3,Carol Smith,true)"

// 反序列化
auto users2 = ason::load_vec<User>(s);
assert(users2.size() == 3);
```

## 支持的类型

| 类型                      | ASON 表示           | 示例                          |
| ------------------------- | ------------------- | ----------------------------- |
| 整数 (i8–i64, u8–u64)     | 纯数字              | `42`, `-100`                  |
| 浮点 (float, double)      | 小数                | `3.14`, `-0.5`                |
| 布尔                      | 字面量              | `true`, `false`               |
| 字符串                    | 无引号或带引号      | `Alice`, `"Carol Smith"`      |
| `std::optional<T>`        | 有值或空            | `hello` 或 _(空白)_ 表示 None |
| `std::vector<T>`          | `[v1,v2,v3]`        | `[rust,go,python]`            |
| `std::unordered_map<K,V>` | `[(k1,v1),(k2,v2)]` | `[(age,30),(score,95)]`       |
| 嵌套结构体                | `(field1,field2)`   | `(Engineering,500000)`        |
| Char                      | 单字符              | `A`                           |

### 嵌套结构体

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

// Schema 反映嵌套关系:
// {name:str,dept:{title:str}}:(Alice,(Engineering))
```

### 可选字段

```cpp
struct Item {
    int64_t id = 0;
    std::optional<std::string> label;
};
ASON_FIELDS(Item,
    (id, "id", "int"),
    (label, "label", "str"))

// 有值:   {id,label}:(1,hello)
// 空值:   {id,label}:(1,)
```

### 数组与字典

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

### 类型注解（可选）

带注解和不带注解的 Schema 完全等价 —— 反序列化器对两者的处理完全一致：

```text
// 不带注解（dump / dump_vec 的默认输出）
{id,name,salary,active}:(1,Alice,5000.50,true)

// 带注解（dump_typed / dump_vec_typed 的输出）
{id:int,name:str,salary:float,active:bool}:(1,Alice,5000.50,true)
```

### 注释

```text
/* 用户列表 */
[{id:int, name:str, active:bool}]:(1,Alice,true),(2,Bob,false)
```

### 多行格式

```text
[{id:int, name:str, active:bool}]:
  (1, Alice, true),
  (2, Bob, false),
  (3, "Carol Smith", true)
```

## API 参考

| 函数                        | 说明                                              |
| --------------------------- | ------------------------------------------------- |
| `ason::dump(obj)`           | 序列化结构体 → 无注解 Schema `{id,name}:`         |
| `ason::dump_typed(obj)`     | 序列化结构体 → 带注解 Schema `{id:int,name:str}:` |
| `ason::dump_vec(vec)`       | 序列化 vector → 无注解 Schema                     |
| `ason::dump_vec_typed(vec)` | 序列化 vector → 带注解 Schema                     |
| `ason::load<T>(str)`        | 反序列化结构体（同时支持带注解和无注解）          |
| `ason::load_vec<T>(str)`    | 反序列化 vector（同时支持带注解和无注解）         |

### 反射宏

```cpp
ASON_FIELDS(StructName,
    (field_member, "schema_name", "type_hint"),
    ...)
```

- `field_member` —— C++ 结构体成员名
- `"schema_name"` —— ASON Schema 中的字段名
- `"type_hint"` —— 类型注解字符串（用于 `dump_typed`）

每个结构体最多支持 **20 个字段**。

## 性能

在 Apple Silicon M 系列（arm64, NEON SIMD）上测试，`-O3` Release 模式，对比最小化内联 JSON 序列化/反序列化器：

### 序列化（ASON 快 2.2–2.8 倍）

| 场景                | JSON     | ASON     | 加速比    |
| ------------------- | -------- | -------- | --------- |
| 平坦结构 × 100      | 3.41 ms  | 1.35 ms  | **2.52x** |
| 平坦结构 × 500      | 13.37 ms | 4.85 ms  | **2.76x** |
| 平坦结构 × 1000     | 19.81 ms | 7.60 ms  | **2.61x** |
| 平坦结构 × 5000     | 90.36 ms | 36.98 ms | **2.44x** |
| 5 层嵌套 × 10       | 6.12 ms  | 2.51 ms  | **2.43x** |
| 5 层嵌套 × 50       | 29.90 ms | 13.56 ms | **2.21x** |
| 5 层嵌套 × 100      | 60.94 ms | 28.20 ms | **2.16x** |
| 大载荷 (10k 条记录) | 19.44 ms | 7.76 ms  | **2.51x** |

### 反序列化（ASON 快 1.3–2.1 倍）

| 场景                | JSON      | ASON     | 加速比    |
| ------------------- | --------- | -------- | --------- |
| 平坦结构 × 100      | 3.18 ms   | 1.72 ms  | **1.85x** |
| 平坦结构 × 500      | 11.50 ms  | 5.61 ms  | **2.05x** |
| 平坦结构 × 1000     | 17.99 ms  | 9.71 ms  | **1.85x** |
| 平坦结构 × 5000     | 101.41 ms | 55.73 ms | **1.82x** |
| 5 层嵌套 × 10       | 9.18 ms   | 5.06 ms  | **1.81x** |
| 5 层嵌套 × 50       | 44.85 ms  | 27.95 ms | **1.60x** |
| 5 层嵌套 × 100      | 91.41 ms  | 67.97 ms | **1.34x** |
| 大载荷 (10k 条记录) | 21.60 ms  | 11.72 ms | **1.84x** |

### 吞吐量

| 指标           | JSON        | ASON         | 加速比    |
| -------------- | ----------- | ------------ | --------- |
| 序列化 条/秒   | 550 万条/秒 | 1310 万条/秒 | **2.39x** |
| 序列化 MB/s    | 639 MB/s    | 711 MB/s     | 1.11x     |
| 反序列化 条/秒 | 500 万条/秒 | 930 万条/秒  | **1.85x** |

### 体积节省

| 场景                | JSON     | ASON     | 节省    |
| ------------------- | -------- | -------- | ------- |
| 平坦结构 × 100      | 12,071 B | 5,612 B  | **54%** |
| 平坦结构 × 1000     | 121 KB   | 57 KB    | **53%** |
| 5 层嵌套 × 10       | 42,594 B | 16,893 B | **60%** |
| 5 层嵌套 × 100      | 431 KB   | 175 KB   | **60%** |
| 大载荷 (10k 条记录) | 1.2 MB   | 0.6 MB   | **53%** |

### 为什么 ASON 更快？

1. **零 Key 哈希** —— Schema 只解析一次；数据字段按位置索引 O(1) 映射，无须逐行做字符串哈希。
2. **Schema 驱动解析** —— 反序列化器已知每个字段的预期类型，可直接解析（`parse_int()`），无需运行时类型推断。CPU 分支预测命中率 ~100%。
3. **最小化内存分配** —— 所有数据行共享同一 Schema 引用，无重复的 Key 字符串分配。
4. **SIMD 字符串扫描** —— NEON/SSE2 向量化字符扫描，用于引号检测、转义序列解析和带引号字符串解析。每个周期处理 16 字节。
5. **快速数字格式化** —— 两位数查找表加速整数序列化；常见情况（整数、1–2 位小数）的浮点数快速路径。

## 测试套件

36 项全面测试覆盖所有功能：

### 序列化与反序列化（4 项测试）

- 简单结构体往返测试
- 带类型注解的 Schema 往返测试
- Vec 往返测试（Schema 驱动）
- Vec 带注解往返测试

### 可选字段（4 项测试）

- 有值解析
- 空值解析（None/nullopt）
- 可选字段序列化
- 逗号之间的空可选字段 `(1,,3)`

### 集合类型（5 项测试）

- Vector 字段解析与往返
- 空 Vector 处理
- 嵌套 Vector `[[1,2],[3,4]]`
- Map 字段解析 `[(key,val)]`
- Map 往返测试

### 嵌套结构体（3 项测试）

- 嵌套结构体解析
- 嵌套结构体往返
- 3 层深度嵌套（DeepC > DeepB > DeepA）

### 字符串处理（7 项测试）

- 带引号字符串解析
- 完整转义序列支持（`\"`、`\\`、`\n`、`\t`、`\,`、`\(`、`\)`、`\[`、`\]`）
- 自动引号检测（SIMD 加速）
- 无引号字符串裁剪
- 含内部空格的字符串
- 前后空格引号处理
- 反斜杠转义往返

### 数字处理（4 项测试）

- 浮点精度（double、float）
- 整数值浮点格式化（如 `42.0`）
- 负数（整数和浮点数）
- 大无符号整数（uint64 最大值）

### 解析特性（6 项测试）

- 布尔值（`true`/`false`）
- 块注释解析 `/* ... */`
- 空白字符处理
- 多行格式解析
- 带类型注解的 Schema 解析
- Schema 字段不匹配（多余/缺失字段）

### 边界情况（3 项测试）

- 错误处理（无效输入抛出 `ason::Error`）
- 空 Vector 序列化 `[]`
- 含特殊字符的字符串 Vector

### 复杂示例（18 项测试）

- 带数组的嵌套结构体
- 带嵌套结构体的 Vec
- Map/字典字段
- 转义字符串往返
- 全类型结构体（16 个字段：bool、i8–i64、u8–u64、float、double、string、optional、vectors）
- 5 层深度嵌套（Country > Region > City > District > Street > Building）
- 7 层深度嵌套（Universe > Galaxy > SolarSystem > Planet > Continent > Nation > State）
- 复杂配置结构体（嵌套 + Map + Optional）
- 100 条数据大结构体往返
- 带类型注解序列化的各种变体
- 边界情况（空 Vec、特殊字符、类布尔字符串、类数字字符串）
- 三层嵌套数组 `[[[1,2],[3,4]]]`
- 真实数据中的注释解析

## 构建与运行

```bash
# 构建所有目标（Release 模式以获得最佳性能）
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 运行测试
./build/ason_test

# 运行示例
./build/basic              # 11 个基础示例
./build/complex_example    # 18 个复杂示例

# 运行性能测试
./build/bench              # ASON vs JSON 全面性能对比
```

## ASON 二进制格式

ASON-BIN 是一种紧凑的二进制序列化格式，与 ASON 文本共享相同的 Schema。采用小端固定宽度编码，无字段名称，纯位置式编码，追求极致吞吐量。

### 编码格式

| 类型               | 编码方式                                  |
| ------------------ | ----------------------------------------- |
| `bool`             | 1 字节（0 或 1）                         |
| `int8/16/32/64`    | 1/2/4/8 字节小端                          |
| `float/double`     | 4/8 字节小端（IEEE 754 位转换）           |
| `string`           | u32 长度（小端）+ UTF-8 字节             |
| `string_view`      | 同上——load 结果直接指向源缓冲区（零拷贝）|
| `vector<T>`        | u32 元素数（小端）+ N × 元素编码         |
| 结构体             | 各字段顺序编码                            |

### API 用法

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

// 序列化为二进制
std::string bin = ason::dump_bin(user);

// 反序列化
User user2 = ason::load_bin<User>(bin);

// 批量（结构体 vector）
std::vector<User> users = { ... };
std::string batch = ason::dump_bin(users);
auto users2 = ason::load_bin<std::vector<User>>(batch);
```

#### 零拷贝反序列化

使用 `std::string_view` 字段避免字符串堆分配：

```cpp
const char* pos = bin.data();
const char* end = bin.data() + bin.size();
std::string_view id_view, payload_view;
int64_t seq = 0;
ason::load_bin_value(pos, end, id_view);      // 零拷贝——指向 bin 缓冲区
ason::load_bin_value(pos, end, seq);
ason::load_bin_value(pos, end, payload_view);
```

### 性能测试（macOS arm64 NEON，100 次迭代）

| 测试场景        | JSON 序列化 | BIN 序列化 | 序列化倍速    | JSON 反序列化 | BIN 反序列化 | 反序列化倍速  | 体积节省 |
| --------------- | ----------- | ---------- | ------------- | ------------- | ------------ | ------------- | -------- |
| 扁平结构 ×100   | 2.28 ms     | 0.58 ms    | **3.90x**     | 2.25 ms       | 0.24 ms      | **9.35x**     | 38%      |
| 扁平结构 ×1000  | 17.72 ms    | 4.73 ms    | **3.75x**     | 18.92 ms      | 2.07 ms      | **9.14x**     | 39%      |
| 全类型结构 ×100 | 2.12 ms     | 1.10 ms    | **1.93x**     | 3.17 ms       | 0.80 ms      | **3.96x**     | 27%      |
| 五层嵌套 ×100   | 60.30 ms    | 16.27 ms   | **3.71x**     | 86.88 ms      | 10.10 ms     | **8.60x**     | 48%      |

> 深层嵌套结构体收益最大：反序列化最高快 **9 倍**。同质数值数组通过 bulk `memcpy` 隐式利用 SIMD 加速。

## ASON 格式规范

完整规范见 [ASON Spec](https://github.com/athxx/ason/blob/main/docs/ASON_SPEC_CN.md)，包含语法规则、BNF 语法、转义规则、类型系统和 LLM 集成最佳实践。

### 语法快速参考

| 元素     | Schema                      | 数据                |
| -------- | --------------------------- | ------------------- |
| 对象     | `{field1:type,field2:type}` | `(val1,val2)`       |
| 数组     | `field:[type]`              | `[v1,v2,v3]`        |
| 对象数组 | `field:[{f1:type,f2:type}]` | `[(v1,v2),(v3,v4)]` |
| 字典     | `field:map[K,V]`            | `[(k1,v1),(k2,v2)]` |
| 嵌套对象 | `field:{f1:type,f2:type}`   | `(v1,(v3,v4))`      |
| 空值     | —                           | _(空白)_            |
| 空字符串 | —                           | `""`                |
| 注释     | —                           | `/* ... */`         |

## License

MIT
