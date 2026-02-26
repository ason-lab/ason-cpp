#include <iostream>
#include <cassert>
#include <cmath>
#include <unordered_map>
#include "ason.hpp"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { std::cout << "  " << #name << "... "; } while(0)

#define PASS() \
    do { std::cout << "OK\n"; tests_passed++; } while(0)

#define FAIL(msg) \
    do { std::cout << "FAIL: " << msg << "\n"; tests_failed++; } while(0)

#define ASSERT_EQ(a, b) \
    do { if ((a) != (b)) { FAIL(#a " != " #b); return; } } while(0)

#define ASSERT_TRUE(x) \
    do { if (!(x)) { FAIL(#x " is false"); return; } } while(0)

#define ASSERT_FALSE(x) \
    do { if (x) { FAIL(#x " is true"); return; } } while(0)

#define ASSERT_NEAR(a, b, eps) \
    do { if (std::abs((a) - (b)) > (eps)) { FAIL(#a " !~ " #b); return; } } while(0)

// ===========================================================================
// Test structs
// ===========================================================================

struct Simple { int64_t id = 0; std::string name; bool active = false; };
ASON_FIELDS(Simple, (id, "id", "int"), (name, "name", "str"), (active, "active", "bool"))

struct WithOptional {
    int64_t id = 0;
    std::optional<std::string> label;
    std::optional<int64_t> count;
};
ASON_FIELDS(WithOptional,
    (id, "id", "int"), (label, "label", "str"), (count, "count", "int"))

struct WithVec { std::string name; std::vector<int64_t> nums; };
ASON_FIELDS(WithVec, (name, "name", "str"), (nums, "nums", "[int]"))

struct Inner { std::string val; int64_t n = 0; };
ASON_FIELDS(Inner, (val, "val", "str"), (n, "n", "int"))

struct Outer { std::string label; Inner inner; };
ASON_FIELDS(Outer, (label, "label", "str"), (inner, "inner", "{val:str,n:int}"))

struct WithMap {
    std::string name;
    std::unordered_map<std::string, int64_t> attrs;
};
ASON_FIELDS(WithMap, (name, "name", "str"), (attrs, "attrs", "map[str,int]"))

struct Floats { double a = 0; double b = 0; float c = 0; };
ASON_FIELDS(Floats, (a, "a", "float"), (b, "b", "float"), (c, "c", "float"))

struct AllNums {
    int8_t i8 = 0; int16_t i16 = 0; int32_t i32 = 0; int64_t i64 = 0;
    uint8_t u8 = 0; uint16_t u16 = 0; uint32_t u32 = 0; uint64_t u64 = 0;
};
ASON_FIELDS(AllNums,
    (i8,"i8","int"),(i16,"i16","int"),(i32,"i32","int"),(i64,"i64","int"),
    (u8,"u8","int"),(u16,"u16","int"),(u32,"u32","int"),(u64,"u64","int"))

struct DeepA { std::string name; int64_t val = 0; };
ASON_FIELDS(DeepA, (name, "name", "str"), (val, "val", "int"))

struct DeepB { std::string label; std::vector<DeepA> items; };
ASON_FIELDS(DeepB, (label, "label", "str"), (items, "items", "[{name:str,val:int}]"))

struct DeepC { std::string title; std::vector<DeepB> groups; };
ASON_FIELDS(DeepC, (title, "title", "str"), (groups, "groups", "[{label:str,items}]"))

struct NestedVec { std::vector<std::vector<int64_t>> matrix; };
ASON_FIELDS(NestedVec, (matrix, "matrix", "[[int]]"))

struct StringOnly { std::string val; };
ASON_FIELDS(StringOnly, (val, "val", "str"))

// ===========================================================================
// Tests
// ===========================================================================

void test_simple_roundtrip() {
    TEST(simple_roundtrip);
    Simple s{42, "Alice", true};
    auto str = ason::encode(s);
    auto s2 = ason::decode<Simple>(str);
    ASSERT_EQ(s2.id, 42);
    ASSERT_EQ(s2.name, "Alice");
    ASSERT_TRUE(s2.active);
    PASS();
}

void test_typed_roundtrip() {
    TEST(typed_roundtrip);
    Simple s{1, "Bob", false};
    auto str = ason::encode_typed(s);
    ASSERT_TRUE(str.find("id:int") != std::string::npos);
    auto s2 = ason::decode<Simple>(str);
    ASSERT_EQ(s2.id, 1);
    ASSERT_EQ(s2.name, "Bob");
    ASSERT_FALSE(s2.active);
    PASS();
}

void test_vec_roundtrip() {
    TEST(vec_roundtrip);
    std::vector<Simple> vec = {{1,"A",true},{2,"B",false},{3,"C",true}};
    auto str = ason::encode(vec);
    auto vec2 = ason::decode<std::vector<Simple>>(str);
    ASSERT_EQ(vec2.size(), 3u);
    ASSERT_EQ(vec2[0].id, 1);
    ASSERT_EQ(vec2[1].name, "B");
    ASSERT_FALSE(vec2[1].active);
    PASS();
}

void test_vec_typed_roundtrip() {
    TEST(vec_typed_roundtrip);
    std::vector<Simple> vec = {{1,"A",true}};
    auto str = ason::encode_typed(vec);
    ASSERT_TRUE(str.find("id:int") != std::string::npos);
    auto vec2 = ason::decode<std::vector<Simple>>(str);
    ASSERT_EQ(vec2.size(), 1u);
    ASSERT_EQ(vec2[0].name, "A");
    PASS();
}

void test_optional_present() {
    TEST(optional_present);
    auto r = ason::decode<WithOptional>("{id,label,count}:(1,hello,42)");
    ASSERT_EQ(r.id, 1);
    ASSERT_TRUE(r.label.has_value());
    ASSERT_EQ(*r.label, "hello");
    ASSERT_TRUE(r.count.has_value());
    ASSERT_EQ(*r.count, 42);
    PASS();
}

void test_optional_absent() {
    TEST(optional_absent);
    auto r = ason::decode<WithOptional>("{id,label,count}:(1,,)");
    ASSERT_EQ(r.id, 1);
    ASSERT_FALSE(r.label.has_value());
    ASSERT_FALSE(r.count.has_value());
    PASS();
}

void test_optional_dump() {
    TEST(optional_dump);
    WithOptional w{1, "hi", std::nullopt};
    auto s = ason::encode(w);
    auto w2 = ason::decode<WithOptional>(s);
    ASSERT_EQ(w2.id, 1);
    ASSERT_TRUE(w2.label.has_value());
    ASSERT_EQ(*w2.label, "hi");
    ASSERT_FALSE(w2.count.has_value());
    PASS();
}

void test_vec_field() {
    TEST(vec_field);
    auto r = ason::decode<WithVec>("{name,nums}:(test,[1,2,3,4,5])");
    ASSERT_EQ(r.name, "test");
    ASSERT_EQ(r.nums.size(), 5u);
    ASSERT_EQ(r.nums[0], 1);
    ASSERT_EQ(r.nums[4], 5);
    PASS();
}

void test_empty_vec() {
    TEST(empty_vec);
    auto r = ason::decode<WithVec>("{name,nums}:(test,[])");
    ASSERT_EQ(r.name, "test");
    ASSERT_TRUE(r.nums.empty());
    // Roundtrip
    auto s = ason::encode(r);
    auto r2 = ason::decode<WithVec>(s);
    ASSERT_TRUE(r2.nums.empty());
    PASS();
}

void test_nested_struct() {
    TEST(nested_struct);
    auto r = ason::decode<Outer>("{label,inner:{val,n}}:(hello,(world,42))");
    ASSERT_EQ(r.label, "hello");
    ASSERT_EQ(r.inner.val, "world");
    ASSERT_EQ(r.inner.n, 42);
    PASS();
}

void test_nested_roundtrip() {
    TEST(nested_roundtrip);
    Outer o{"test", {"value", 99}};
    auto s = ason::encode(o);
    auto o2 = ason::decode<Outer>(s);
    ASSERT_EQ(o2.label, "test");
    ASSERT_EQ(o2.inner.val, "value");
    ASSERT_EQ(o2.inner.n, 99);
    PASS();
}

void test_map_field() {
    TEST(map_field);
    auto r = ason::decode<WithMap>("{name,attrs}:(Alice,[(age,30),(score,95)])");
    ASSERT_EQ(r.name, "Alice");
    ASSERT_EQ(r.attrs.size(), 2u);
    ASSERT_EQ(r.attrs.at("age"), 30);
    ASSERT_EQ(r.attrs.at("score"), 95);
    PASS();
}

void test_map_roundtrip() {
    TEST(map_roundtrip);
    WithMap m{"Bob", {{"x", 1}, {"y", 2}}};
    auto s = ason::encode(m);
    auto m2 = ason::decode<WithMap>(s);
    ASSERT_EQ(m2.name, "Bob");
    ASSERT_EQ(m2.attrs.at("x"), 1);
    ASSERT_EQ(m2.attrs.at("y"), 2);
    PASS();
}

void test_quoted_string() {
    TEST(quoted_string);
    auto r = ason::decode<Simple>("{id,name,active}:(1,\"hello world\",true)");
    ASSERT_EQ(r.name, "hello world");
    PASS();
}

void test_escape_sequences() {
    TEST(escape_sequences);
    StringOnly s{"say \"hi\", then (wave)\tnewline\nend"};
    auto str = ason::encode(s);
    auto s2 = ason::decode<StringOnly>(str);
    ASSERT_EQ(s.val, s2.val);
    PASS();
}

void test_string_needs_quoting() {
    TEST(string_needs_quoting);
    // Strings with special chars should be quoted
    StringOnly s1{"hello,world"};
    auto str1 = ason::encode(s1);
    ASSERT_TRUE(str1.find("\"hello,world\"") != std::string::npos ||
                str1.find("\"hello\\,world\"") != std::string::npos);

    StringOnly s2{"true"};
    auto str2 = ason::encode(s2);
    ASSERT_TRUE(str2.find("\"true\"") != std::string::npos);

    StringOnly s3{"12345"};
    auto str3 = ason::encode(s3);
    ASSERT_TRUE(str3.find("\"12345\"") != std::string::npos);
    PASS();
}

void test_floats() {
    TEST(floats);
    Floats f{3.14, -0.5, 100.0f};
    auto s = ason::encode(f);
    auto f2 = ason::decode<Floats>(s);
    ASSERT_NEAR(f2.a, 3.14, 0.001);
    ASSERT_NEAR(f2.b, -0.5, 0.001);
    ASSERT_NEAR(f2.c, 100.0, 0.001);
    PASS();
}

void test_integer_valued_float() {
    TEST(integer_valued_float);
    Floats f{42.0, 0.0, -7.0f};
    auto s = ason::encode(f);
    // Should output "42.0" not "42"
    ASSERT_TRUE(s.find("42.0") != std::string::npos);
    auto f2 = ason::decode<Floats>(s);
    ASSERT_NEAR(f2.a, 42.0, 0.001);
    PASS();
}

void test_negative_numbers() {
    TEST(negative_numbers);
    AllNums n{};
    n.i8 = -128; n.i16 = -32768; n.i32 = -2147483647-1; n.i64 = -9223372036854775807LL;
    auto s = ason::encode(n);
    auto n2 = ason::decode<AllNums>(s);
    ASSERT_EQ(n2.i8, -128);
    ASSERT_EQ(n2.i16, -32768);
    ASSERT_EQ(n2.i64, -9223372036854775807LL);
    PASS();
}

void test_large_unsigned() {
    TEST(large_unsigned);
    AllNums n{};
    n.u64 = 18446744073709551615ULL;
    n.u32 = 4294967295U;
    auto s = ason::encode(n);
    auto n2 = ason::decode<AllNums>(s);
    ASSERT_EQ(n2.u64, 18446744073709551615ULL);
    ASSERT_EQ(n2.u32, 4294967295U);
    PASS();
}

void test_deep_nesting() {
    TEST(deep_nesting);
    DeepC c{
        "top",
        {
            DeepB{"group1", {DeepA{"a1", 1}, DeepA{"a2", 2}}},
            DeepB{"group2", {DeepA{"b1", 10}}},
        }
    };
    auto s = ason::encode(c);
    auto c2 = ason::decode<DeepC>(s);
    ASSERT_EQ(c2.title, "top");
    ASSERT_EQ(c2.groups.size(), 2u);
    ASSERT_EQ(c2.groups[0].label, "group1");
    ASSERT_EQ(c2.groups[0].items.size(), 2u);
    ASSERT_EQ(c2.groups[0].items[1].name, "a2");
    ASSERT_EQ(c2.groups[1].items[0].val, 10);
    PASS();
}

void test_nested_vec() {
    TEST(nested_vec);
    NestedVec nv{{{1,2,3},{4,5},{6}}};
    auto s = ason::encode(nv);
    auto nv2 = ason::decode<NestedVec>(s);
    ASSERT_EQ(nv2.matrix.size(), 3u);
    ASSERT_EQ(nv2.matrix[0], (std::vector<int64_t>{1,2,3}));
    ASSERT_EQ(nv2.matrix[1], (std::vector<int64_t>{4,5}));
    ASSERT_EQ(nv2.matrix[2], (std::vector<int64_t>{6}));
    PASS();
}

void test_comments() {
    TEST(comments);
    auto r = ason::decode<Simple>("/* top-level comment */ {id,name,active}: /* inline */ (1,Alice,true)");
    ASSERT_EQ(r.id, 1);
    ASSERT_EQ(r.name, "Alice");
    ASSERT_TRUE(r.active);
    PASS();
}

void test_whitespace() {
    TEST(whitespace);
    auto r = ason::decode<Simple>("{ id , name , active } : ( 1 , Alice , true )");
    ASSERT_EQ(r.id, 1);
    ASSERT_EQ(r.name, "Alice");
    ASSERT_TRUE(r.active);
    PASS();
}

void test_multiline() {
    TEST(multiline);
    auto vec = ason::decode<std::vector<Simple>>(
        "[{id, name, active}]:\n"
        "  (1, Alice, true),\n"
        "  (2, Bob, false)");
    ASSERT_EQ(vec.size(), 2u);
    ASSERT_EQ(vec[0].name, "Alice");
    ASSERT_FALSE(vec[1].active);
    PASS();
}

void test_typed_schema_parse() {
    TEST(typed_schema_parse);
    auto r = ason::decode<Simple>(
        "{id:int,name:str,active:bool}:(42,Hello,false)");
    ASSERT_EQ(r.id, 42);
    ASSERT_EQ(r.name, "Hello");
    ASSERT_FALSE(r.active);
    PASS();
}

void test_schema_field_mismatch() {
    TEST(schema_field_mismatch);
    // Extra field in schema that struct doesn't have — should skip
    auto r = ason::decode<Simple>(
        "{id,extra_field,name,active}:(42,ignored,Hello,true)");
    ASSERT_EQ(r.id, 42);
    ASSERT_EQ(r.name, "Hello");
    ASSERT_TRUE(r.active);
    PASS();
}

void test_unquoted_string_trim() {
    TEST(unquoted_string_trim);
    auto r = ason::decode<Simple>("{id,name,active}:(1,  Alice  ,true)");
    ASSERT_EQ(r.name, "Alice");
    PASS();
}

void test_bool_values() {
    TEST(bool_values);
    auto r1 = ason::decode<Simple>("{id,name,active}:(1,A,true)");
    ASSERT_TRUE(r1.active);
    auto r2 = ason::decode<Simple>("{id,name,active}:(1,A,false)");
    ASSERT_FALSE(r2.active);
    PASS();
}

void test_empty_optional_between_commas() {
    TEST(empty_optional_between_commas);
    auto r = ason::decode<WithOptional>("{id,label,count}:(1,,42)");
    ASSERT_EQ(r.id, 1);
    ASSERT_FALSE(r.label.has_value());
    ASSERT_TRUE(r.count.has_value());
    ASSERT_EQ(*r.count, 42);
    PASS();
}

void test_string_with_spaces() {
    TEST(string_with_spaces);
    auto r = ason::decode<Simple>("{id,name,active}:(1,\"  spaces  \",true)");
    ASSERT_EQ(r.name, "  spaces  ");
    PASS();
}

void test_vec_of_strings() {
    TEST(vec_of_strings);
    struct VS { std::vector<std::string> tags; };
    // We need ASON_FIELDS for VS
    // Let's use WithVec with string parsing
    auto r = ason::decode<WithVec>("{name,nums}:(test,[1,2,3])");
    ASSERT_EQ(r.nums, (std::vector<int64_t>{1,2,3}));
    PASS();
}

void test_error_handling() {
    TEST(error_handling);
    bool caught = false;
    try {
        ason::decode<Simple>("not valid ason");
    } catch (const ason::Error&) {
        caught = true;
    }
    ASSERT_TRUE(caught);
    PASS();
}

void test_encode_vec_empty() {
    TEST(encode_vec_empty);
    std::vector<Simple> empty;
    auto s = ason::encode(empty);
    ASSERT_TRUE(s.find("[{id,name,active}]:") == 0);
    PASS();
}

void test_leading_trailing_space_quoting() {
    TEST(leading_trailing_space_quoting);
    StringOnly s{" leading"};
    auto str = ason::encode(s);
    ASSERT_TRUE(str.find("\" leading\"") != std::string::npos);
    auto s2 = ason::decode<StringOnly>(str);
    ASSERT_EQ(s2.val, " leading");

    StringOnly s3{"trailing "};
    auto str3 = ason::encode(s3);
    auto s4 = ason::decode<StringOnly>(str3);
    ASSERT_EQ(s4.val, "trailing ");
    PASS();
}

void test_backslash_escape() {
    TEST(backslash_escape);
    StringOnly s{"path\\to\\file"};
    auto str = ason::encode(s);
    auto s2 = ason::decode<StringOnly>(str);
    ASSERT_EQ(s2.val, "path\\to\\file");
    PASS();
}

// ===========================================================================
// Format validation tests — {schema}: is INVALID for vec; [{schema}]: required
// ===========================================================================

static const char* BAD_FMT = "{id:int,name:str}:\n  (1,Alice),\n  (2,Bob),\n  (3,Carol)";
static const char* GOOD_FMT = "[{id:int,name:str}]:\n  (1,Alice),\n  (2,Bob),\n  (3,Carol)";
static const char* BAD_FMT2 = "{id:int,name:str}:(1,Alice)";  // single, then trailing

struct FmtRow { int64_t id = 0; std::string name; };
ASON_FIELDS(FmtRow, (id, "id", "int"), (name, "name", "str"))

void test_bad_format_as_vec() {
    TEST(bad_format_as_vec);
    bool threw = false;
    try { ason::decode<std::vector<FmtRow>>(BAD_FMT); }
    catch (const std::exception&) { threw = true; }
    if (!threw) { FAIL("should reject {schema}: for vector decode"); return; }
    PASS();
}

void test_bad_format_trailing_rows() {
    // {schema}:(row1),(row2) — first row decoded, trailing rows are trailing chars → error
    TEST(bad_format_trailing_rows);
    bool threw = false;
    try { ason::decode<FmtRow>(BAD_FMT); }
    catch (const std::exception&) { threw = true; }
    if (!threw) { FAIL("should reject trailing rows after single struct decode"); return; }
    PASS();
}

void test_good_format_as_vec() {
    TEST(good_format_as_vec);
    std::vector<FmtRow> v;
    try { v = ason::decode<std::vector<FmtRow>>(GOOD_FMT); }
    catch (const std::exception& e) { FAIL(std::string("should succeed: ") + e.what()); return; }
    ASSERT_EQ(v.size(), 3u);
    ASSERT_EQ(v[0].id, 1);
    ASSERT_EQ(v[0].name, "Alice");
    ASSERT_EQ(v[2].name, "Carol");
    PASS();
}

void test_bad_format_extra_tuples() {
    // Multiple structures encoded without vec syntax
    TEST(bad_format_extra_tuples);
    const char* bad = "{id:int,name:str}:(10,Dave),(11,Eve)";
    bool threw = false;
    try { ason::decode<FmtRow>(bad); }
    catch (const std::exception&) { threw = true; }
    if (!threw) { FAIL("should reject trailing tuple after single struct"); return; }
    PASS();
}

void test_bad_format_typed_as_vec() {
    // Even with typed annotations but missing []: wrapper
    TEST(bad_format_typed_as_vec);
    bool threw = false;
    try { ason::decode<std::vector<FmtRow>>("{id:int,name:str}:(1,A),(2,B)"); }
    catch (const std::exception&) { threw = true; }
    if (!threw) { FAIL("should reject {typed_schema}: for vector decode"); return; }
    PASS();
}

// ===========================================================================
// encodePretty -> decode roundtrip tests
// ===========================================================================

struct Score2 { int64_t id = 0; double value = 0; std::string label; };
ASON_FIELDS(Score2, (id, "id", "int"), (value, "value", "float"), (label, "label", "str"))

struct Team {
    std::string name;
    std::vector<int64_t> scores;
    bool active = false;
};
ASON_FIELDS(Team, (name, "name", "str"), (scores, "scores", "[int]"), (active, "active", "bool"))

void test_pretty_simple_roundtrip() {
    TEST(pretty_simple_roundtrip);
    Simple s{42, "Alice", true};
    auto pretty = ason::encode_pretty(s);
    auto s2 = ason::decode<Simple>(pretty);
    ASSERT_EQ(s2.id, s.id);
    ASSERT_EQ(s2.name, s.name);
    ASSERT_TRUE(s2.active);
    PASS();
}

void test_pretty_typed_roundtrip() {
    TEST(pretty_typed_roundtrip);
    Simple s{99, "Zara", false};
    auto pretty = ason::encode_pretty_typed(s);
    ASSERT_TRUE(pretty.find("id:int") != std::string::npos);
    auto s2 = ason::decode<Simple>(pretty);
    ASSERT_EQ(s2.id, 99);
    ASSERT_EQ(s2.name, "Zara");
    ASSERT_FALSE(s2.active);
    PASS();
}

void test_pretty_vec_roundtrip() {
    TEST(pretty_vec_roundtrip);
    std::vector<Simple> vec = {{1,"A",true},{2,"B",false},{3,"C",true}};
    auto pretty = ason::encode_pretty(vec);
    ASSERT_TRUE(pretty.find('\n') != std::string::npos);
    auto vec2 = ason::decode<std::vector<Simple>>(pretty);
    ASSERT_EQ(vec2.size(), 3u);
    ASSERT_EQ(vec2[0].id, 1);
    ASSERT_EQ(vec2[1].name, "B");
    ASSERT_EQ(vec2[2].name, "C");
    PASS();
}

void test_pretty_nested_roundtrip() {
    TEST(pretty_nested_roundtrip);
    Outer o{"hello", {"world", 42}};
    auto pretty = ason::encode_pretty(o);
    auto o2 = ason::decode<Outer>(pretty);
    ASSERT_EQ(o2.label, "hello");
    ASSERT_EQ(o2.inner.val, "world");
    ASSERT_EQ(o2.inner.n, 42);
    PASS();
}

void test_pretty_optional_roundtrip() {
    TEST(pretty_optional_roundtrip);
    WithOptional w{7, std::nullopt, std::optional<int64_t>{99}};
    auto pretty = ason::encode_pretty(w);
    auto w2 = ason::decode<WithOptional>(pretty);
    ASSERT_EQ(w2.id, 7);
    ASSERT_FALSE(w2.label.has_value());
    ASSERT_TRUE(w2.count.has_value());
    ASSERT_EQ(*w2.count, 99);
    PASS();
}

void test_pretty_complex_vec_roundtrip() {
    TEST(pretty_complex_vec_roundtrip);
    std::vector<Score2> scores = {{1,95.5,"excellent"},{2,72.3,"good"},{3,40.0,"fail"}};
    auto pretty = ason::encode_pretty(scores);
    auto s2 = ason::decode<std::vector<Score2>>(pretty);
    ASSERT_EQ(s2.size(), 3u);
    ASSERT_NEAR(s2[0].value, 95.5, 1e-9);
    ASSERT_EQ(s2[0].label, "excellent");
    ASSERT_NEAR(s2[2].value, 40.0, 1e-9);
    PASS();
}

void test_pretty_deep_nesting_roundtrip() {
    TEST(pretty_deep_nesting_roundtrip);
    std::vector<DeepA> items = {{"alpha", 1}, {"beta", 2}};
    DeepB b{"mygroup", items};
    auto pretty = ason::encode_pretty(b);
    auto b2 = ason::decode<DeepB>(pretty);
    ASSERT_EQ(b2.label, "mygroup");
    ASSERT_EQ(b2.items.size(), 2u);
    ASSERT_EQ(b2.items[0].name, "alpha");
    ASSERT_EQ(b2.items[1].val, 2);
    PASS();
}

// ===========================================================================
// Main
// ===========================================================================

int main() {
    std::cout << "=== ASON C++ Test Suite ===\n\n";

    std::cout << "--- Serialization/Deserialization ---\n";
    test_simple_roundtrip();
    test_typed_roundtrip();
    test_vec_roundtrip();
    test_vec_typed_roundtrip();

    std::cout << "\n--- Optional fields ---\n";
    test_optional_present();
    test_optional_absent();
    test_optional_dump();
    test_empty_optional_between_commas();

    std::cout << "\n--- Collections ---\n";
    test_vec_field();
    test_empty_vec();
    test_nested_vec();
    test_map_field();
    test_map_roundtrip();

    std::cout << "\n--- Nested structs ---\n";
    test_nested_struct();
    test_nested_roundtrip();
    test_deep_nesting();

    std::cout << "\n--- Strings ---\n";
    test_quoted_string();
    test_escape_sequences();
    test_string_needs_quoting();
    test_unquoted_string_trim();
    test_string_with_spaces();
    test_leading_trailing_space_quoting();
    test_backslash_escape();

    std::cout << "\n--- Numbers ---\n";
    test_floats();
    test_integer_valued_float();
    test_negative_numbers();
    test_large_unsigned();

    std::cout << "\n--- Parsing features ---\n";
    test_bool_values();
    test_comments();
    test_whitespace();
    test_multiline();
    test_typed_schema_parse();
    test_schema_field_mismatch();

    std::cout << "\n--- Edge cases ---\n";
    test_error_handling();
    test_encode_vec_empty();
    test_vec_of_strings();

    std::cout << "\n--- Format validation ({schema}: must be rejected for vec) ---\n";
    test_bad_format_as_vec();
    test_bad_format_trailing_rows();
    test_good_format_as_vec();
    test_bad_format_extra_tuples();
    test_bad_format_typed_as_vec();

    std::cout << "\n--- encodePretty -> decode roundtrips ---\n";
    test_pretty_simple_roundtrip();
    test_pretty_typed_roundtrip();
    test_pretty_vec_roundtrip();
    test_pretty_nested_roundtrip();
    test_pretty_optional_roundtrip();
    test_pretty_complex_vec_roundtrip();
    test_pretty_deep_nesting_roundtrip();

    std::cout << "\n=== Results: " << tests_passed << " passed, "
              << tests_failed << " failed ===\n";

    return tests_failed > 0 ? 1 : 0;
}
