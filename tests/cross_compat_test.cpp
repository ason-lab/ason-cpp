#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <string>
#include <unordered_map>
#include "ason.hpp"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { std::cout << "  " << #name << "... "; } while(0)
#define PASS() do { std::cout << "OK\n"; tests_passed++; } while(0)
#define FAIL(msg) do { std::cout << "FAIL: " << msg << "\n"; tests_failed++; return; } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { FAIL(#a " != " #b); return; } } while(0)
#define ASSERT_TRUE(x) do { if (!(x)) { FAIL(#x " is false"); return; } } while(0)
#define ASSERT_FALSE(x) do { if (x) { FAIL(#x " is true"); return; } } while(0)
#define ASSERT_NEAR(a, b, eps) do { if (std::abs((a)-(b)) > (eps)) { FAIL(#a " !~ " #b); return; } } while(0)

// ============================================================================
// Source structs (written by encoder)
// ============================================================================

struct FullUser { int64_t id=0; std::string name; int32_t age=0; bool active=false; double score=0; };
ASON_FIELDS(FullUser, (id,"id","int"),(name,"name","str"),(age,"age","int"),(active,"active","bool"),(score,"score","float"))

struct MiniUser { int64_t id=0; std::string name; };
ASON_FIELDS(MiniUser, (id,"id","int"),(name,"name","str"))

// Dim 2
struct RichProfile { int64_t id=0; std::string name; std::vector<std::string> tags; std::vector<int64_t> scores; };
ASON_FIELDS(RichProfile, (id,"id","int"),(name,"name","str"),(tags,"tags","[str]"),(scores,"scores","[int]"))

struct ThinProfile { int64_t id=0; std::string name; };
ASON_FIELDS(ThinProfile, (id,"id","int"),(name,"name","str"))

// Dim 3: nested
struct InnerFull { int64_t x=0; int64_t y=0; double z=0; bool w=false; };
ASON_FIELDS(InnerFull, (x,"x","int"),(y,"y","int"),(z,"z","float"),(w,"w","bool"))

struct OuterFull { std::string name; InnerFull inner; bool flag=false; };
ASON_FIELDS(OuterFull, (name,"name","str"),(inner,"inner","{x:int,y:int,z:float,w:bool}"),(flag,"flag","bool"))

struct InnerThin { int64_t x=0; int64_t y=0; };
ASON_FIELDS(InnerThin, (x,"x","int"),(y,"y","int"))

struct OuterThin { std::string name; InnerThin inner; };
ASON_FIELDS(OuterThin, (name,"name","str"),(inner,"inner","{x:int,y:int}"))

// Dim 4: vec of nested
struct TaskFull { std::string title; bool done=false; int64_t priority=0; double weight=0; };
ASON_FIELDS(TaskFull, (title,"title","str"),(done,"done","bool"),(priority,"priority","int"),(weight,"weight","float"))

struct ProjectFull { std::string name; std::vector<TaskFull> tasks; };
ASON_FIELDS(ProjectFull, (name,"name","str"),(tasks,"tasks","[{title:str,done:bool,priority:int,weight:float}]"))

struct TaskThin { std::string title; bool done=false; };
ASON_FIELDS(TaskThin, (title,"title","str"),(done,"done","bool"))

struct ProjectThin { std::string name; std::vector<TaskThin> tasks; };
ASON_FIELDS(ProjectThin, (name,"name","str"),(tasks,"tasks","[{title:str,done:bool}]"))

// Dim 5: deep 3-level
struct L3Full { int64_t a=0; std::string b; bool c=false; };
ASON_FIELDS(L3Full, (a,"a","int"),(b,"b","str"),(c,"c","bool"))

struct L2Full { std::string name; L3Full sub; int64_t code=0; std::vector<std::string> tags; };
ASON_FIELDS(L2Full, (name,"name","str"),(sub,"sub","{a:int,b:str,c:bool}"),(code,"code","int"),(tags,"tags","[str]"))

struct L1Full { int64_t id=0; L2Full child; std::string extra; };
ASON_FIELDS(L1Full, (id,"id","int"),(child,"child","{name:str,sub:{a:int,b:str,c:bool},code:int,tags:[str]}"),(extra,"extra","str"))

struct L3Thin { int64_t a=0; };
ASON_FIELDS(L3Thin, (a,"a","int"))

struct L2Thin { std::string name; L3Thin sub; };
ASON_FIELDS(L2Thin, (name,"name","str"),(sub,"sub","{a:int}"))

struct L1Thin { int64_t id=0; L2Thin child; };
ASON_FIELDS(L1Thin, (id,"id","int"),(child,"child","{name:str,sub:{a:int}}"))

// Dim 6: field reorder
struct OrderABC { int64_t a=0; std::string b; bool c=false; };
ASON_FIELDS(OrderABC, (a,"a","int"),(b,"b","str"),(c,"c","bool"))

struct OrderCAB { bool c=false; int64_t a=0; std::string b; };
ASON_FIELDS(OrderCAB, (c,"c","bool"),(a,"a","int"),(b,"b","str"))

// Dim 7: reorder + drop
struct BigRecord { int64_t id=0; std::string name; double score=0; bool active=false; int64_t level=0; };
ASON_FIELDS(BigRecord, (id,"id","int"),(name,"name","str"),(score,"score","float"),(active,"active","bool"),(level,"level","int"))

struct SmallReordered { double score=0; int64_t id=0; };
ASON_FIELDS(SmallReordered, (score,"score","float"),(id,"id","int"))

// Dim 8: target extra fields
struct SrcSmall { int64_t id=0; std::string name; };
ASON_FIELDS(SrcSmall, (id,"id","int"),(name,"name","str"))

struct DstBig { int64_t id=0; std::string name; bool missing=false; double extra=0; };
ASON_FIELDS(DstBig, (id,"id","int"),(name,"name","str"),(missing,"missing","bool"),(extra,"extra","float"))

// Dim 9: optional
struct SrcWithOpt { int64_t id=0; std::optional<std::string> label; std::optional<double> score_opt; bool flag=false; };
ASON_FIELDS(SrcWithOpt, (id,"id","int"),(label,"label","str"),(score_opt,"score","float"),(flag,"flag","bool"))

struct DstFewerOpt { int64_t id=0; std::optional<std::string> label; };
ASON_FIELDS(DstFewerOpt, (id,"id","int"),(label,"label","str"))

// Dim 10: special string
struct SrcSpecialStr { int64_t id=0; std::string name; std::string bio; };
ASON_FIELDS(SrcSpecialStr, (id,"id","int"),(name,"name","str"),(bio,"bio","str"))

struct DstNoStr { int64_t id=0; };
ASON_FIELDS(DstNoStr, (id,"id","int"))

// Dim 11: trailing arrays
struct SrcNestedArr { int64_t id=0; std::vector<int64_t> matrix; std::vector<std::string> tags; };
ASON_FIELDS(SrcNestedArr, (id,"id","int"),(matrix,"matrix","[int]"),(tags,"tags","[str]"))

// Dim 14: negative
struct SrcNegative { int64_t a=0; int64_t b=0; double c=0; std::string d; };
ASON_FIELDS(SrcNegative, (a,"a","int"),(b,"b","int"),(c,"c","float"),(d,"d","str"))

struct DstNegThin { int64_t a=0; int64_t b=0; };
ASON_FIELDS(DstNegThin, (a,"a","int"),(b,"b","int"))

// Dim 15: empty string
struct SrcEmpty { int64_t id=0; std::string name; std::string bio; };
ASON_FIELDS(SrcEmpty, (id,"id","int"),(name,"name","str"),(bio,"bio","str"))

struct DstEmptyThin { int64_t id=0; };
ASON_FIELDS(DstEmptyThin, (id,"id","int"))

// Dim 16: map
struct SrcWithMap { int64_t id=0; std::string name; std::unordered_map<std::string,int64_t> meta; };
ASON_FIELDS(SrcWithMap, (id,"id","int"),(name,"name","str"),(meta,"meta","map[str,int]"))

struct DstNoMap { int64_t id=0; std::string name; };
ASON_FIELDS(DstNoMap, (id,"id","int"),(name,"name","str"))

// Dim 20: bools
struct SrcBools { int64_t id=0; bool a=false; bool b=false; bool c=false; };
ASON_FIELDS(SrcBools, (id,"id","int"),(a,"a","bool"),(b,"b","bool"),(c,"c","bool"))

struct DstBoolsThin { int64_t id=0; };
ASON_FIELDS(DstBoolsThin, (id,"id","int"))

// Dim 21: five fields
struct SrcFive { int64_t a=0; std::string b; double c=0; bool d=false; int64_t e=0; };
ASON_FIELDS(SrcFive, (a,"a","int"),(b,"b","str"),(c,"c","float"),(d,"d","bool"),(e,"e","int"))

struct DstMiddleOnly { double c=0; };
ASON_FIELDS(DstMiddleOnly, (c,"c","float"))

struct DstLastOnly { int64_t e=0; };
ASON_FIELDS(DstLastOnly, (e,"e","int"))

// Dim 23: no overlap
struct SrcAlpha { int64_t x=0; std::string y; };
ASON_FIELDS(SrcAlpha, (x,"x","int"),(y,"y","str"))

struct DstBeta { int64_t p=0; std::string q; };
ASON_FIELDS(DstBeta, (p,"p","int"),(q,"q","str"))

// Dim 24: nested array of structs
struct WorkerFull { std::string name; std::vector<std::string> skills; int64_t years_xp=0; double rating=0; };
ASON_FIELDS(WorkerFull, (name,"name","str"),(skills,"skills","[str]"),(years_xp,"years_xp","int"),(rating,"rating","float"))

struct TeamFull { std::string lead; std::vector<WorkerFull> workers; double budget=0; };
ASON_FIELDS(TeamFull, (lead,"lead","str"),(workers,"workers","[{name:str,skills:[str],years_xp:int,rating:float}]"),(budget,"budget","float"))

struct WorkerThin { std::string name; std::vector<std::string> skills; };
ASON_FIELDS(WorkerThin, (name,"name","str"),(skills,"skills","[str]"))

struct TeamThin { std::string lead; std::vector<WorkerThin> workers; };
ASON_FIELDS(TeamThin, (lead,"lead","str"),(workers,"workers","[{name:str,skills:[str]}]"))

// Dim 26: wide
struct SrcWide { int64_t f1=0; std::string f2; bool f3=false; int64_t f4=0; std::string f5; bool f6=false; int64_t f7=0; std::string f8; bool f9=false; int64_t f10=0; };
ASON_FIELDS(SrcWide, (f1,"f1","int"),(f2,"f2","str"),(f3,"f3","bool"),(f4,"f4","int"),(f5,"f5","str"),(f6,"f6","bool"),(f7,"f7","int"),(f8,"f8","str"),(f9,"f9","bool"),(f10,"f10","int"))

struct DstNarrow { int64_t f1=0; };
ASON_FIELDS(DstNarrow, (f1,"f1","int"))

// Dim 28: ason-like string
struct SrcAsonLike { int64_t id=0; std::string data; std::string code; };
ASON_FIELDS(SrcAsonLike, (id,"id","int"),(data,"data","str"),(code,"code","str"))

// Dim 29: unicode
struct SrcUnicode { int64_t id=0; std::string name; std::string bio; };
ASON_FIELDS(SrcUnicode, (id,"id","int"),(name,"name","str"),(bio,"bio","str"))

// Dim 30: roundtrip
struct VersionA { int64_t id=0; std::string name; bool active=false; };
ASON_FIELDS(VersionA, (id,"id","int"),(name,"name","str"),(active,"active","bool"))

struct VersionB { int64_t id=0; std::string name; };
ASON_FIELDS(VersionB, (id,"id","int"),(name,"name","str"))

// Dim 31: arr in middle
struct SrcWithArr { int64_t id=0; std::vector<std::string> items; int64_t score=0; };
ASON_FIELDS(SrcWithArr, (id,"id","int"),(items,"items","[str]"),(score,"score","int"))

struct DstWithArrThin { int64_t id=0; std::vector<std::string> items; };
ASON_FIELDS(DstWithArrThin, (id,"id","int"),(items,"items","[str]"))

// Dim 32: nested struct skip
struct InnerSkip { int64_t a=0; std::string b; };
ASON_FIELDS(InnerSkip, (a,"a","int"),(b,"b","str"))

struct SrcWithNested { int64_t id=0; InnerSkip inner; std::string tail; };
ASON_FIELDS(SrcWithNested, (id,"id","int"),(inner,"inner","{a:int,b:str}"),(tail,"tail","str"))

struct DstFlat { int64_t id=0; };
ASON_FIELDS(DstFlat, (id,"id","int"))

// Dim 13: float roundtrip
struct SrcFloat { int64_t id=0; double value=0; };
ASON_FIELDS(SrcFloat, (id,"id","int"),(value,"value","float"))

// Dim 25: typed mixed
struct SrcTyped { int64_t a=0; std::string b; double c=0; bool d=false; };
ASON_FIELDS(SrcTyped, (a,"a","int"),(b,"b","str"),(c,"c","float"),(d,"d","bool"))

struct DstMixed { std::string b; bool d=false; int64_t extra=0; double more=0; };
ASON_FIELDS(DstMixed, (b,"b","str"),(d,"d","bool"),(extra,"extra","int"),(more,"more","float"))

// Dim 19: nested vec + trailing
struct DetailFull { int64_t id=0; std::string name; int32_t age=0; bool gender=false; };
ASON_FIELDS(DetailFull, (id,"ID","int"),(name,"Name","str"),(age,"Age","int"),(gender,"Gender","bool"))

struct UserFull2 { std::vector<DetailFull> details; int64_t code=0; std::string label; };
ASON_FIELDS(UserFull2, (details,"details","[{ID:int,Name:str,Age:int,Gender:bool}]"),(code,"code","int"),(label,"label","str"))

struct PersonThin { int64_t id=0; std::string name; };
ASON_FIELDS(PersonThin, (id,"ID","int"),(name,"Name","str"))

struct HumanThin { std::vector<PersonThin> details; };
ASON_FIELDS(HumanThin, (details,"details","[{ID:int,Name:str}]"))

// ============================================================================
// Tests
// ============================================================================

void test_cross_trailing_fields_vec() {
    TEST(trailing_fields_vec);
    std::vector<FullUser> src = {{1,"Alice",30,true,95.5},{2,"Bob",25,false,87.0}};
    auto data = ason::encode(src);
    auto dst = ason::decode<std::vector<MiniUser>>(data);
    ASSERT_EQ(dst.size(), 2u);
    ASSERT_EQ(dst[0].id, 1); ASSERT_EQ(dst[0].name, "Alice");
    ASSERT_EQ(dst[1].id, 2); ASSERT_EQ(dst[1].name, "Bob");
    PASS();
}

void test_cross_trailing_fields_single() {
    TEST(trailing_fields_single);
    FullUser src{99,"Zara",40,true,100.0};
    auto data = ason::encode(src);
    auto dst = ason::decode<MiniUser>(data);
    ASSERT_EQ(dst.id, 99); ASSERT_EQ(dst.name, "Zara");
    PASS();
}

void test_cross_skip_trailing_array_map() {
    TEST(skip_trailing_array_map);
    RichProfile src{1,"Alice",{"go","rust"},{90,85,92}};
    auto data = ason::encode(src);
    auto dst = ason::decode<ThinProfile>(data);
    ASSERT_EQ(dst.id, 1); ASSERT_EQ(dst.name, "Alice");
    PASS();
}

void test_cross_nested_fewer_fields() {
    TEST(nested_fewer_fields);
    OuterFull src{"test",{10,20,3.14,true},true};
    auto data = ason::encode(src);
    auto dst = ason::decode<OuterThin>(data);
    ASSERT_EQ(dst.name, "test");
    ASSERT_EQ(dst.inner.x, 10); ASSERT_EQ(dst.inner.y, 20);
    PASS();
}

void test_cross_vec_nested_skip() {
    TEST(vec_nested_skip);
    std::vector<ProjectFull> src = {
        {"Alpha",{{"Design",true,1,0.5},{"Code",false,2,0.8}}},
        {"Beta",{{"Test",false,3,1.0}}},
    };
    auto data = ason::encode(src);
    auto dst = ason::decode<std::vector<ProjectThin>>(data);
    ASSERT_EQ(dst.size(), 2u);
    ASSERT_EQ(dst[0].name, "Alpha");
    ASSERT_EQ(dst[0].tasks.size(), 2u);
    ASSERT_EQ(dst[0].tasks[0].title, "Design"); ASSERT_TRUE(dst[0].tasks[0].done);
    ASSERT_EQ(dst[0].tasks[1].title, "Code"); ASSERT_FALSE(dst[0].tasks[1].done);
    ASSERT_EQ(dst[1].name, "Beta"); ASSERT_EQ(dst[1].tasks.size(), 1u);
    PASS();
}

void test_cross_deep_3_levels() {
    TEST(deep_3_levels);
    L1Full src{1, {"mid", {42,"hello",true}, 7, {"x","y"}}, "dropped"};
    auto data = ason::encode(src);
    auto dst = ason::decode<L1Thin>(data);
    ASSERT_EQ(dst.id, 1);
    ASSERT_EQ(dst.child.name, "mid");
    ASSERT_EQ(dst.child.sub.a, 42);
    PASS();
}

void test_cross_field_reorder() {
    TEST(field_reorder);
    OrderABC src{1,"hi",true};
    auto data = ason::encode(src);
    auto dst = ason::decode<OrderCAB>(data);
    ASSERT_EQ(dst.a, 1); ASSERT_EQ(dst.b, "hi"); ASSERT_TRUE(dst.c);
    PASS();
}

void test_cross_reorder_drop() {
    TEST(reorder_drop);
    std::vector<BigRecord> src = {{1,"A",9.5,true,3},{2,"B",8.0,false,1}};
    auto data = ason::encode(src);
    auto dst = ason::decode<std::vector<SmallReordered>>(data);
    ASSERT_EQ(dst.size(), 2u);
    ASSERT_NEAR(dst[0].score, 9.5, 1e-10); ASSERT_EQ(dst[0].id, 1);
    ASSERT_NEAR(dst[1].score, 8.0, 1e-10); ASSERT_EQ(dst[1].id, 2);
    PASS();
}

void test_cross_target_extra_fields() {
    TEST(target_extra_fields);
    SrcSmall src{42,"Alice"};
    auto data = ason::encode(src);
    auto dst = ason::decode<DstBig>(data);
    ASSERT_EQ(dst.id, 42); ASSERT_EQ(dst.name, "Alice");
    ASSERT_FALSE(dst.missing); ASSERT_NEAR(dst.extra, 0.0, 1e-10);
    PASS();
}

void test_cross_optional_skip() {
    TEST(optional_skip);
    SrcWithOpt src{1, "hello", 95.5, true};
    auto data = ason::encode(src);
    auto dst = ason::decode<DstFewerOpt>(data);
    ASSERT_EQ(dst.id, 1);
    ASSERT_TRUE(dst.label.has_value()); ASSERT_EQ(*dst.label, "hello");
    PASS();
}

void test_cross_optional_nil_skip() {
    TEST(optional_nil_skip);
    SrcWithOpt src{2, std::nullopt, std::nullopt, false};
    auto data = ason::encode(src);
    auto dst = ason::decode<DstFewerOpt>(data);
    ASSERT_EQ(dst.id, 2); ASSERT_FALSE(dst.label.has_value());
    PASS();
}

void test_cross_skip_special_string() {
    TEST(skip_special_string);
    SrcSpecialStr src{1, "comma,here", "paren(test) and \"quotes\""};
    auto data = ason::encode(src);
    auto dst = ason::decode<DstNoStr>(data);
    ASSERT_EQ(dst.id, 1);
    PASS();
}

void test_cross_skip_trailing_arrays() {
    TEST(skip_trailing_arrays);
    std::vector<SrcNestedArr> src = {{1,{1,2,3},{"a","b"}},{2,{4,5},{"c"}}};
    auto data = ason::encode(src);
    auto dst = ason::decode<std::vector<DstNoStr>>(data);
    ASSERT_EQ(dst.size(), 2u);
    ASSERT_EQ(dst[0].id, 1); ASSERT_EQ(dst[1].id, 2);
    PASS();
}

void test_cross_float_roundtrip() {
    TEST(float_roundtrip);
    SrcFloat src{1, 3.14159};
    auto data = ason::encode(src);
    auto dst = ason::decode<SrcFloat>(data);
    ASSERT_NEAR(dst.value, 3.14159, 1e-10);
    PASS();
}

void test_cross_negative_skip() {
    TEST(negative_skip);
    SrcNegative src{-1,-999999,-3.14,"neg"};
    auto data = ason::encode(src);
    auto dst = ason::decode<DstNegThin>(data);
    ASSERT_EQ(dst.a, -1); ASSERT_EQ(dst.b, -999999);
    PASS();
}

void test_cross_empty_string() {
    TEST(empty_string);
    SrcEmpty src{1,"",""};
    auto data = ason::encode(src);
    auto dst = ason::decode<DstEmptyThin>(data);
    ASSERT_EQ(dst.id, 1);
    PASS();
}

void test_cross_skip_map() {
    TEST(skip_map);
    SrcWithMap src{1,"Alice",{{"age",30},{"score",95}}};
    auto data = ason::encode(src);
    auto dst = ason::decode<DstNoMap>(data);
    ASSERT_EQ(dst.id, 1); ASSERT_EQ(dst.name, "Alice");
    PASS();
}

void test_cross_typed_vec() {
    TEST(typed_vec);
    std::vector<FullUser> src = {{1,"Alice",30,true,95.5}};
    auto data = ason::encode_typed(src);
    auto dst = ason::decode<std::vector<MiniUser>>(data);
    ASSERT_EQ(dst.size(), 1u);
    ASSERT_EQ(dst[0].id, 1); ASSERT_EQ(dst[0].name, "Alice");
    PASS();
}

void test_cross_typed_single() {
    TEST(typed_single);
    FullUser src{42,"Bob",25,false,88.0};
    auto data = ason::encode_typed(src);
    auto dst = ason::decode<MiniUser>(data);
    ASSERT_EQ(dst.id, 42); ASSERT_EQ(dst.name, "Bob");
    PASS();
}

void test_cross_nested_vec_trailing_outer() {
    TEST(nested_vec_trailing_outer);
    std::vector<UserFull2> src = {{{{1,"Alice",30,true},{2,"Bob",25,false}},42,"test"}};
    auto data = ason::encode(src);
    auto dst = ason::decode<std::vector<HumanThin>>(data);
    ASSERT_EQ(dst.size(), 1u);
    ASSERT_EQ(dst[0].details.size(), 2u);
    ASSERT_EQ(dst[0].details[0].id, 1); ASSERT_EQ(dst[0].details[0].name, "Alice");
    ASSERT_EQ(dst[0].details[1].id, 2); ASSERT_EQ(dst[0].details[1].name, "Bob");
    PASS();
}

void test_cross_skip_bools() {
    TEST(skip_bools);
    std::vector<SrcBools> src = {{1,true,false,true},{2,false,true,false}};
    auto data = ason::encode(src);
    auto dst = ason::decode<std::vector<DstBoolsThin>>(data);
    ASSERT_EQ(dst.size(), 2u);
    ASSERT_EQ(dst[0].id, 1); ASSERT_EQ(dst[1].id, 2);
    PASS();
}

void test_cross_pick_middle() {
    TEST(pick_middle);
    SrcFive src{1,"hi",3.14,true,99};
    auto data = ason::encode(src);
    auto dst = ason::decode<DstMiddleOnly>(data);
    ASSERT_NEAR(dst.c, 3.14, 1e-10);
    PASS();
}

void test_cross_pick_last() {
    TEST(pick_last);
    SrcFive src{1,"hi",3.14,true,42};
    auto data = ason::encode(src);
    auto dst = ason::decode<DstLastOnly>(data);
    ASSERT_EQ(dst.e, 42);
    PASS();
}

void test_cross_no_overlap() {
    TEST(no_overlap);
    SrcAlpha src{1,"hello"};
    auto data = ason::encode(src);
    auto dst = ason::decode<DstBeta>(data);
    ASSERT_EQ(dst.p, 0); ASSERT_EQ(dst.q, "");
    PASS();
}

void test_cross_nested_array_structs() {
    TEST(nested_array_structs);
    TeamFull src{"Alice",{{"Bob",{"go","rust"},5,4.5},{"Carol",{"python"},3,3.8}},100000.0};
    auto data = ason::encode(src);
    auto dst = ason::decode<TeamThin>(data);
    ASSERT_EQ(dst.lead, "Alice");
    ASSERT_EQ(dst.workers.size(), 2u);
    ASSERT_EQ(dst.workers[0].name, "Bob");
    ASSERT_EQ(dst.workers[0].skills, (std::vector<std::string>{"go","rust"}));
    ASSERT_EQ(dst.workers[1].name, "Carol");
    ASSERT_EQ(dst.workers[1].skills, (std::vector<std::string>{"python"}));
    PASS();
}

void test_cross_typed_mixed() {
    TEST(typed_mixed);
    SrcTyped src{1,"test",2.5,true};
    auto data = ason::encode_typed(src);
    auto dst = ason::decode<DstMixed>(data);
    ASSERT_EQ(dst.b, "test"); ASSERT_TRUE(dst.d);
    ASSERT_EQ(dst.extra, 0); ASSERT_NEAR(dst.more, 0.0, 1e-10);
    PASS();
}

void test_cross_many_trailing() {
    TEST(many_trailing);
    SrcWide src{42,"a",true,4,"b",false,7,"c",true,10};
    auto data = ason::encode(src);
    auto dst = ason::decode<DstNarrow>(data);
    ASSERT_EQ(dst.f1, 42);
    PASS();
}

void test_cross_vec_single_row() {
    TEST(vec_single_row);
    std::vector<FullUser> src = {{1,"Alice",30,true,95.5}};
    auto data = ason::encode(src);
    auto dst = ason::decode<std::vector<MiniUser>>(data);
    ASSERT_EQ(dst.size(), 1u);
    ASSERT_EQ(dst[0].id, 1); ASSERT_EQ(dst[0].name, "Alice");
    PASS();
}

void test_cross_ason_syntax_string() {
    TEST(ason_syntax_string);
    SrcAsonLike src{1, "{a,b}:(1,2)", "[(x,y),(z,w)]"};
    auto data = ason::encode(src);
    auto dst = ason::decode<DstNoStr>(data);
    ASSERT_EQ(dst.id, 1);
    PASS();
}

void test_cross_unicode_trailing() {
    TEST(unicode_trailing);
    SrcUnicode src{1, u8"日本語テスト", u8"中文描述，包含逗号"};
    auto data = ason::encode(src);
    auto dst = ason::decode<DstNoStr>(data);
    ASSERT_EQ(dst.id, 1);
    PASS();
}

void test_cross_roundtrip_abba() {
    TEST(roundtrip_abba);
    VersionA a{1,"test",true};
    auto da = ason::encode(a);
    auto b = ason::decode<VersionB>(da);
    ASSERT_EQ(b.id, 1); ASSERT_EQ(b.name, "test");
    auto db = ason::encode(b);
    auto a2 = ason::decode<VersionA>(db);
    ASSERT_EQ(a2.id, 1); ASSERT_EQ(a2.name, "test"); ASSERT_FALSE(a2.active);
    PASS();
}

void test_cross_empty_array_middle() {
    TEST(empty_array_middle);
    std::vector<SrcWithArr> src = {{1,{},10},{2,{"a","b"},20}};
    auto data = ason::encode(src);
    auto dst = ason::decode<std::vector<DstWithArrThin>>(data);
    ASSERT_EQ(dst.size(), 2u);
    ASSERT_EQ(dst[0].id, 1); ASSERT_TRUE(dst[0].items.empty());
    ASSERT_EQ(dst[1].id, 2); ASSERT_EQ(dst[1].items, (std::vector<std::string>{"a","b"}));
    PASS();
}

void test_cross_skip_nested_tuple() {
    TEST(skip_nested_tuple);
    SrcWithNested src{1, {10,"nested"}, "end"};
    auto data = ason::encode(src);
    auto dst = ason::decode<DstFlat>(data);
    ASSERT_EQ(dst.id, 1);
    PASS();
}

void test_cross_many_rows() {
    TEST(many_rows);
    std::vector<FullUser> src;
    for (int i = 0; i < 100; i++) {
        src.push_back({(int64_t)i, "user", i, i%2==0, (double)i*0.1});
    }
    auto data = ason::encode(src);
    auto dst = ason::decode<std::vector<MiniUser>>(data);
    ASSERT_EQ(dst.size(), 100u);
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(dst[i].id, (int64_t)i);
        ASSERT_EQ(dst[i].name, "user");
    }
    PASS();
}

void test_cross_typed_subset_reorder() {
    TEST(typed_subset_reorder);
    std::vector<BigRecord> src = {{1,"A",9.5,true,3},{2,"B",8.0,false,1}};
    auto data = ason::encode_typed(src);
    auto dst = ason::decode<std::vector<SmallReordered>>(data);
    ASSERT_EQ(dst.size(), 2u);
    ASSERT_NEAR(dst[0].score, 9.5, 1e-10); ASSERT_EQ(dst[0].id, 1);
    ASSERT_NEAR(dst[1].score, 8.0, 1e-10); ASSERT_EQ(dst[1].id, 2);
    PASS();
}

void test_cross_zero_value() {
    TEST(zero_value);
    FullUser src{0,"",0,false,0.0};
    auto data = ason::encode(src);
    auto dst = ason::decode<MiniUser>(data);
    ASSERT_EQ(dst.id, 0); ASSERT_EQ(dst.name, "");
    PASS();
}

int main() {
    std::cout << "=== ASON C++ Cross-Compat Test Suite ===\n\n";

    test_cross_trailing_fields_vec();
    test_cross_trailing_fields_single();
    test_cross_skip_trailing_array_map();
    test_cross_nested_fewer_fields();
    test_cross_vec_nested_skip();
    test_cross_deep_3_levels();
    test_cross_field_reorder();
    test_cross_reorder_drop();
    test_cross_target_extra_fields();
    test_cross_optional_skip();
    test_cross_optional_nil_skip();
    test_cross_skip_special_string();
    test_cross_skip_trailing_arrays();
    test_cross_float_roundtrip();
    test_cross_negative_skip();
    test_cross_empty_string();
    test_cross_skip_map();
    test_cross_typed_vec();
    test_cross_typed_single();
    test_cross_nested_vec_trailing_outer();
    test_cross_skip_bools();
    test_cross_pick_middle();
    test_cross_pick_last();
    test_cross_no_overlap();
    test_cross_nested_array_structs();
    test_cross_typed_mixed();
    test_cross_many_trailing();
    test_cross_vec_single_row();
    test_cross_ason_syntax_string();
    test_cross_unicode_trailing();
    test_cross_roundtrip_abba();
    test_cross_empty_array_middle();
    test_cross_skip_nested_tuple();
    test_cross_many_rows();
    test_cross_typed_subset_reorder();
    test_cross_zero_value();

    std::cout << "\n=== Results: " << tests_passed << " passed, "
              << tests_failed << " failed ===\n";
    return tests_failed > 0 ? 1 : 0;
}
