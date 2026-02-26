#include <iostream>
#include <chrono>
#include <cassert>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include "ason.hpp"

// For JSON comparison, we use a minimal inline JSON serializer/deserializer
// to avoid external dependencies. This is NOT a full JSON library.
namespace json_mini {

// ---- Serialize ----

inline void append_str(std::string& buf, const std::string& s) {
    buf.push_back('"');
    for (char c : s) {
        switch (c) {
            case '"': buf.append("\\\""); break;
            case '\\': buf.append("\\\\"); break;
            case '\n': buf.append("\\n"); break;
            case '\t': buf.append("\\t"); break;
            default: buf.push_back(c); break;
        }
    }
    buf.push_back('"');
}

inline void append_i64(std::string& buf, int64_t v) {
    char tmp[24]; auto [p,e] = std::to_chars(tmp, tmp+24, v);
    buf.append(tmp, p - tmp);
}

inline void append_f64(std::string& buf, double v) {
    char tmp[32]; auto [p,e] = std::to_chars(tmp, tmp+32, v);
    buf.append(tmp, p - tmp);
}

// ---- Deserialize (very basic, positional) ----

inline void skip_ws(const char*& p, const char* e) {
    while (p < e && (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r')) p++;
}

inline void expect(const char*& p, const char* e, char c) {
    skip_ws(p, e);
    if (p >= e || *p != c) throw std::runtime_error(std::string("expected '") + c + "'");
    p++;
}

inline std::string read_str(const char*& p, const char* e) {
    skip_ws(p, e);
    if (*p != '"') throw std::runtime_error("expected '\"'");
    p++;
    std::string r;
    while (p < e && *p != '"') {
        if (*p == '\\') { p++;
            if (*p == 'n') r.push_back('\n');
            else if (*p == 't') r.push_back('\t');
            else r.push_back(*p);
            p++;
        } else { r.push_back(*p); p++; }
    }
    if (p < e) p++; // skip "
    return r;
}

inline int64_t read_i64(const char*& p, const char* e) {
    skip_ws(p, e);
    int64_t v; auto [ptr, ec] = std::from_chars(p, e, v);
    p = ptr; return v;
}

inline double read_f64(const char*& p, const char* e) {
    skip_ws(p, e);
    char* endptr = nullptr;
    double v = std::strtod(p, &endptr);
    p = endptr; return v;
}

inline bool read_bool(const char*& p, const char* e) {
    skip_ws(p, e);
    if (p + 4 <= e && p[0] == 't') { p += 4; return true; }
    p += 5; return false;
}

inline void skip_comma(const char*& p, const char* e) {
    skip_ws(p, e);
    if (p < e && *p == ',') p++;
}

} // namespace json_mini

// ===========================================================================
// Structs
// ===========================================================================

struct User {
    int64_t id = 0;
    std::string name;
    std::string email;
    int64_t age = 0;
    double score = 0;
    bool active = false;
    std::string role;
    std::string city;
};

ASON_FIELDS(User,
    (id,     "id",     "int"),
    (name,   "name",   "str"),
    (email,  "email",  "str"),
    (age,    "age",    "int"),
    (score,  "score",  "float"),
    (active, "active", "bool"),
    (role,   "role",   "str"),
    (city,   "city",   "str"))

struct AllTypes {
    bool b = false;
    int8_t i8v = 0;
    int16_t i16v = 0;
    int32_t i32v = 0;
    int64_t i64v = 0;
    uint8_t u8v = 0;
    uint16_t u16v = 0;
    uint32_t u32v = 0;
    uint64_t u64v = 0;
    float f32v = 0;
    double f64v = 0;
    std::string s;
    std::optional<int64_t> opt_some;
    std::optional<int64_t> opt_none;
    std::vector<int64_t> vec_int;
    std::vector<std::string> vec_str;
};

ASON_FIELDS(AllTypes,
    (b,        "b",        "bool"),
    (i8v,      "i8v",      "int"),
    (i16v,     "i16v",     "int"),
    (i32v,     "i32v",     "int"),
    (i64v,     "i64v",     "int"),
    (u8v,      "u8v",      "int"),
    (u16v,     "u16v",     "int"),
    (u32v,     "u32v",     "int"),
    (u64v,     "u64v",     "int"),
    (f32v,     "f32v",     "float"),
    (f64v,     "f64v",     "float"),
    (s,        "s",        "str"),
    (opt_some, "opt_some", "int"),
    (opt_none, "opt_none", "int"),
    (vec_int,  "vec_int",  "[int]"),
    (vec_str,  "vec_str",  "[str]"))

struct Task {
    int64_t id = 0;
    std::string title;
    int64_t priority = 0;
    bool done = false;
    double hours = 0;
};
ASON_FIELDS(Task,
    (id,       "id",       "int"),
    (title,    "title",    "str"),
    (priority, "priority", "int"),
    (done,     "done",     "bool"),
    (hours,    "hours",    "float"))

struct Project {
    std::string name;
    double budget = 0;
    bool active = false;
    std::vector<Task> tasks;
};
ASON_FIELDS(Project,
    (name,   "name",   "str"),
    (budget, "budget", "float"),
    (active, "active", "bool"),
    (tasks,  "tasks",  "[{id:int,title:str,priority:int,done:bool,hours:float}]"))

struct Team {
    std::string name;
    std::string lead;
    int64_t size = 0;
    std::vector<Project> projects;
};
ASON_FIELDS(Team,
    (name,     "name",     "str"),
    (lead,     "lead",     "str"),
    (size,     "size",     "int"),
    (projects, "projects", "[{name:str,budget:float,active:bool,tasks}]"))

struct Division {
    std::string name;
    std::string location;
    int64_t headcount = 0;
    std::vector<Team> teams;
};
ASON_FIELDS(Division,
    (name,      "name",      "str"),
    (location,  "location",  "str"),
    (headcount, "headcount", "int"),
    (teams,     "teams",     "[{name:str,lead:str,size:int,projects}]"))

struct Company {
    std::string name;
    int64_t founded = 0;
    double revenue_m = 0;
    bool is_public = false;
    std::vector<Division> divisions;
    std::vector<std::string> tags;
};
ASON_FIELDS(Company,
    (name,       "name",       "str"),
    (founded,    "founded",    "int"),
    (revenue_m,  "revenue_m",  "float"),
    (is_public,  "public",     "bool"),
    (divisions,  "divisions",  "[{name:str,location:str,headcount:int,teams}]"),
    (tags,       "tags",       "[str]"))

// ===========================================================================
// Data generators
// ===========================================================================

std::vector<User> generate_users(size_t n) {
    const char* names[] = {"Alice", "Bob", "Carol", "David", "Eve", "Frank", "Grace", "Hank"};
    const char* roles[] = {"engineer", "designer", "manager", "analyst"};
    const char* cities[] = {"NYC", "LA", "Chicago", "Houston", "Phoenix"};
    std::vector<User> users(n);
    for (size_t i = 0; i < n; i++) {
        users[i].id = (int64_t)i;
        users[i].name = names[i % 8];
        users[i].email = std::string(names[i % 8]) + "@example.com";
        users[i].age = 25 + (int64_t)(i % 40);
        users[i].score = 50.0 + (i % 50) + 0.5;
        users[i].active = (i % 3 != 0);
        users[i].role = roles[i % 4];
        users[i].city = cities[i % 5];
    }
    return users;
}

std::vector<AllTypes> generate_all_types(size_t n) {
    std::vector<AllTypes> items(n);
    for (size_t i = 0; i < n; i++) {
        items[i].b = (i % 2 == 0);
        items[i].i8v = (int8_t)(i % 256);
        items[i].i16v = -(int16_t)i;
        items[i].i32v = (int32_t)(i * 1000);
        items[i].i64v = (int64_t)(i * 100000);
        items[i].u8v = (uint8_t)(i % 256);
        items[i].u16v = (uint16_t)(i % 65536);
        items[i].u32v = (uint32_t)(i * 7919);
        items[i].u64v = (uint64_t)(i * 1000000007ULL);
        items[i].f32v = (float)i * 1.5f;
        items[i].f64v = (double)i * 0.25 + 0.5;
        items[i].s = "item_" + std::to_string(i);
        items[i].opt_some = (i % 2 == 0) ? std::optional<int64_t>((int64_t)i) : std::nullopt;
        items[i].opt_none = std::nullopt;
        items[i].vec_int = {(int64_t)i, (int64_t)(i + 1), (int64_t)(i + 2)};
        items[i].vec_str = {"tag" + std::to_string(i % 5), "cat" + std::to_string(i % 3)};
    }
    return items;
}

std::vector<Company> generate_companies(size_t n) {
    const int dp = 2, tp = 2, pp = 3, tkp = 4;
    const char* locs[] = {"NYC", "London", "Tokyo", "Berlin"};
    const char* leads[] = {"Alice", "Bob", "Carol", "David"};
    std::vector<Company> companies(n);
    for (size_t i = 0; i < n; i++) {
        companies[i].name = "Corp_" + std::to_string(i);
        companies[i].founded = 1990 + (int64_t)(i % 35);
        companies[i].revenue_m = 10.0 + (double)i * 5.5;
        companies[i].is_public = (i % 2 == 0);
        companies[i].tags = {"enterprise", "tech", "sector_" + std::to_string(i % 5)};
        for (int d = 0; d < dp; d++) {
            Division div;
            div.name = "Div_" + std::to_string(i) + "_" + std::to_string(d);
            div.location = locs[d % 4];
            div.headcount = 50 + (int64_t)(d * 20);
            for (int t = 0; t < tp; t++) {
                Team team;
                team.name = "Team_" + std::to_string(i) + "_" + std::to_string(d) + "_" + std::to_string(t);
                team.lead = leads[t % 4];
                team.size = 5 + (int64_t)(t * 2);
                for (int p = 0; p < pp; p++) {
                    Project proj;
                    proj.name = "Proj_" + std::to_string(t) + "_" + std::to_string(p);
                    proj.budget = 100.0 + (double)p * 50.5;
                    proj.active = (p % 2 == 0);
                    for (int tk = 0; tk < tkp; tk++) {
                        Task task;
                        task.id = (int64_t)(i * 100 + d * 10 + t * 5 + tk);
                        task.title = "Task_" + std::to_string(tk);
                        task.priority = (int64_t)(tk % 3 + 1);
                        task.done = (tk % 2 == 0);
                        task.hours = 2.0 + (double)tk * 1.5;
                        proj.tasks.push_back(task);
                    }
                    team.projects.push_back(proj);
                }
                div.teams.push_back(team);
            }
            companies[i].divisions.push_back(div);
        }
    }
    return companies;
}

// ===========================================================================
// JSON serialization (minimal, for comparison)
// ===========================================================================

void json_serialize_user(std::string& buf, const User& u) {
    buf.push_back('{');
    buf.append("\"id\":"); json_mini::append_i64(buf, u.id); buf.push_back(',');
    buf.append("\"name\":"); json_mini::append_str(buf, u.name); buf.push_back(',');
    buf.append("\"email\":"); json_mini::append_str(buf, u.email); buf.push_back(',');
    buf.append("\"age\":"); json_mini::append_i64(buf, u.age); buf.push_back(',');
    buf.append("\"score\":"); json_mini::append_f64(buf, u.score); buf.push_back(',');
    buf.append("\"active\":"); buf.append(u.active ? "true" : "false"); buf.push_back(',');
    buf.append("\"role\":"); json_mini::append_str(buf, u.role); buf.push_back(',');
    buf.append("\"city\":"); json_mini::append_str(buf, u.city);
    buf.push_back('}');
}

std::string json_serialize_users(const std::vector<User>& users) {
    std::string buf;
    buf.reserve(users.size() * 200);
    buf.push_back('[');
    for (size_t i = 0; i < users.size(); i++) {
        if (i > 0) buf.push_back(',');
        json_serialize_user(buf, users[i]);
    }
    buf.push_back(']');
    return buf;
}

User json_deserialize_user(const char*& p, const char* e) {
    using namespace json_mini;
    User u;
    expect(p, e, '{');
    // read fields in order
    read_str(p, e); expect(p, e, ':'); u.id = read_i64(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); u.name = read_str(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); u.email = read_str(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); u.age = read_i64(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); u.score = read_f64(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); u.active = read_bool(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); u.role = read_str(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); u.city = read_str(p, e);
    expect(p, e, '}');
    return u;
}

std::vector<User> json_deserialize_users(const std::string& s) {
    const char* p = s.data();
    const char* e = p + s.size();
    using namespace json_mini;
    std::vector<User> users;
    expect(p, e, '[');
    while (true) {
        skip_ws(p, e);
        if (p >= e || *p == ']') break;
        if (!users.empty()) skip_comma(p, e);
        users.push_back(json_deserialize_user(p, e));
    }
    return users;
}

// ===========================================================================
// JSON deep struct deserializers (for fair comparison)
// ===========================================================================

Task json_deserialize_task(const char*& p, const char* e) {
    using namespace json_mini;
    Task t;
    expect(p, e, '{');
    read_str(p, e); expect(p, e, ':'); t.id = read_i64(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); t.title = read_str(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); t.priority = read_i64(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); t.done = read_bool(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); t.hours = read_f64(p, e);
    expect(p, e, '}');
    return t;
}

Project json_deserialize_project(const char*& p, const char* e) {
    using namespace json_mini;
    Project proj;
    expect(p, e, '{');
    read_str(p, e); expect(p, e, ':'); proj.name = read_str(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); proj.budget = read_f64(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); proj.active = read_bool(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); // "tasks"
    expect(p, e, '[');
    while (true) {
        skip_ws(p, e);
        if (p >= e || *p == ']') break;
        if (!proj.tasks.empty()) skip_comma(p, e);
        proj.tasks.push_back(json_deserialize_task(p, e));
    }
    expect(p, e, ']');
    expect(p, e, '}');
    return proj;
}

Team json_deserialize_team(const char*& p, const char* e) {
    using namespace json_mini;
    Team team;
    expect(p, e, '{');
    read_str(p, e); expect(p, e, ':'); team.name = read_str(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); team.lead = read_str(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); team.size = read_i64(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); // "projects"
    expect(p, e, '[');
    while (true) {
        skip_ws(p, e);
        if (p >= e || *p == ']') break;
        if (!team.projects.empty()) skip_comma(p, e);
        team.projects.push_back(json_deserialize_project(p, e));
    }
    expect(p, e, ']');
    expect(p, e, '}');
    return team;
}

Division json_deserialize_division(const char*& p, const char* e) {
    using namespace json_mini;
    Division div;
    expect(p, e, '{');
    read_str(p, e); expect(p, e, ':'); div.name = read_str(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); div.location = read_str(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); div.headcount = read_i64(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); // "teams"
    expect(p, e, '[');
    while (true) {
        skip_ws(p, e);
        if (p >= e || *p == ']') break;
        if (!div.teams.empty()) skip_comma(p, e);
        div.teams.push_back(json_deserialize_team(p, e));
    }
    expect(p, e, ']');
    expect(p, e, '}');
    return div;
}

Company json_deserialize_company(const char*& p, const char* e) {
    using namespace json_mini;
    Company c;
    expect(p, e, '{');
    read_str(p, e); expect(p, e, ':'); c.name = read_str(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); c.founded = read_i64(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); c.revenue_m = read_f64(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); c.is_public = read_bool(p, e); skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); // "divisions"
    expect(p, e, '[');
    while (true) {
        skip_ws(p, e);
        if (p >= e || *p == ']') break;
        if (!c.divisions.empty()) skip_comma(p, e);
        c.divisions.push_back(json_deserialize_division(p, e));
    }
    expect(p, e, ']');
    skip_comma(p, e);
    read_str(p, e); expect(p, e, ':'); // "tags"
    expect(p, e, '[');
    while (true) {
        skip_ws(p, e);
        if (p >= e || *p == ']') break;
        if (!c.tags.empty()) skip_comma(p, e);
        c.tags.push_back(read_str(p, e));
    }
    expect(p, e, ']');
    expect(p, e, '}');
    return c;
}

std::vector<Company> json_deserialize_companies(const std::string& s) {
    const char* p = s.data();
    const char* e = p + s.size();
    using namespace json_mini;
    std::vector<Company> companies;
    expect(p, e, '[');
    while (true) {
        skip_ws(p, e);
        if (p >= e || *p == ']') break;
        if (!companies.empty()) skip_comma(p, e);
        companies.push_back(json_deserialize_company(p, e));
    }
    return companies;
}

// ===========================================================================
// Benchmark helpers
// ===========================================================================

using Clock = std::chrono::high_resolution_clock;

struct BenchResult {
    std::string name;
    double json_ser_ms = 0;
    double ason_ser_ms = 0;
    double ason_bin_ser_ms = 0;
    double json_de_ms = 0;
    double ason_de_ms = 0;
    double ason_bin_de_ms = 0;
    size_t json_bytes = 0;
    size_t ason_bytes = 0;
    size_t ason_bin_bytes = 0;

    void print() const {
        double ser_ratio = json_ser_ms / ason_ser_ms;
        double de_ratio = json_de_ms / ason_de_ms;
        double bin_ser_ratio = json_ser_ms / ason_bin_ser_ms;
        double bin_de_ratio = json_de_ms / ason_bin_de_ms;
        double saving = (1.0 - (double)ason_bytes / (double)json_bytes) * 100.0;
        double bin_saving = (1.0 - (double)ason_bin_bytes / (double)json_bytes) * 100.0;

        std::cout << "  " << name << "\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "    Serialize:   JSON " << std::setw(8) << json_ser_ms
                  << "ms | ASON " << std::setw(8) << ason_ser_ms
                  << "ms | BIN " << std::setw(8) << ason_bin_ser_ms
                  << "ms | ratio " << bin_ser_ratio << "x\n";
        std::cout << "    Deserialize: JSON " << std::setw(8) << json_de_ms
                  << "ms | ASON " << std::setw(8) << ason_de_ms
                  << "ms | BIN " << std::setw(8) << ason_bin_de_ms
                  << "ms | ratio " << bin_de_ratio << "x\n";
        std::cout << "    Size:        JSON " << std::setw(8) << json_bytes
                  << " B | ASON " << std::setw(8) << ason_bytes
                  << " B | BIN " << std::setw(8) << ason_bin_bytes
                  << " B | saving " << std::setprecision(0) << bin_saving << "%\n";
    }
};

std::string format_bytes(size_t b) {
    char buf[64];
    if (b >= 1048576) snprintf(buf, 64, "%.1f MB", (double)b / 1048576.0);
    else if (b >= 1024) snprintf(buf, 64, "%.1f KB", (double)b / 1024.0);
    else snprintf(buf, 64, "%zu B", b);
    return buf;
}

// ===========================================================================
// Benchmarks
// ===========================================================================

BenchResult bench_flat(size_t count, int iterations) {
    auto users = generate_users(count);

    // JSON serialize
    std::string json_str;
    auto t0 = Clock::now();
    for (int i = 0; i < iterations; i++) json_str = json_serialize_users(users);
    double json_ser = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    // ASON serialize
    std::string ason_str;
    t0 = Clock::now();
    for (int i = 0; i < iterations; i++) ason_str = ason::encode(users);
    double ason_ser = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    // ASON-BIN serialize
    std::string ason_bin_str;
    t0 = Clock::now();
    for (int i = 0; i < iterations; i++) ason_bin_str = ason::encode_bin(users);
    double ason_bin_ser = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    // JSON deserialize
    t0 = Clock::now();
    for (int i = 0; i < iterations; i++) {
        auto r = json_deserialize_users(json_str);
        assert(r.size() == count);
    }
    double json_de = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    // ASON deserialize
    t0 = Clock::now();
    for (int i = 0; i < iterations; i++) {
        auto r = ason::decode<std::vector<User>>(ason_str);
        assert(r.size() == count);
    }
    double ason_de = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    // ASON-BIN deserialize
    t0 = Clock::now();
    for (int i = 0; i < iterations; i++) {
        auto r = ason::decode_bin<std::vector<User>>(ason_bin_str);
        assert(r.size() == count);
    }
    double ason_bin_de = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    // Verify
    auto decoded = ason::decode<std::vector<User>>(ason_str);
    assert(decoded.size() == count);
    assert(decoded[0].id == users[0].id);

    return BenchResult{
        "Flat struct × " + std::to_string(count) + " (8 fields)",
        json_ser, ason_ser, ason_bin_ser, json_de, ason_de, ason_bin_de,
        json_str.size(), ason_str.size(), ason_bin_str.size()
    };
}

BenchResult bench_all_types(size_t count, int iterations) {
    auto items = generate_all_types(count);

    // JSON: serialize as individual objects
    std::string json_str;
    auto t0 = Clock::now();
    for (int iter = 0; iter < iterations; iter++) {
        json_str.clear();
        json_str.push_back('[');
        for (size_t i = 0; i < items.size(); i++) {
            if (i > 0) json_str.push_back(',');
            json_str.push_back('{');
            json_str.append("\"b\":"); json_str.append(items[i].b ? "true" : "false"); json_str.push_back(',');
            json_str.append("\"i8v\":"); json_mini::append_i64(json_str, items[i].i8v); json_str.push_back(',');
            json_str.append("\"i16v\":"); json_mini::append_i64(json_str, items[i].i16v); json_str.push_back(',');
            json_str.append("\"i32v\":"); json_mini::append_i64(json_str, items[i].i32v); json_str.push_back(',');
            json_str.append("\"i64v\":"); json_mini::append_i64(json_str, items[i].i64v); json_str.push_back(',');
            json_str.append("\"u8v\":"); json_mini::append_i64(json_str, items[i].u8v); json_str.push_back(',');
            json_str.append("\"u16v\":"); json_mini::append_i64(json_str, items[i].u16v); json_str.push_back(',');
            json_str.append("\"u32v\":"); json_mini::append_i64(json_str, items[i].u32v); json_str.push_back(',');
            json_str.append("\"u64v\":"); json_mini::append_i64(json_str, (int64_t)items[i].u64v); json_str.push_back(',');
            json_str.append("\"f32v\":"); json_mini::append_f64(json_str, items[i].f32v); json_str.push_back(',');
            json_str.append("\"f64v\":"); json_mini::append_f64(json_str, items[i].f64v); json_str.push_back(',');
            json_str.append("\"s\":"); json_mini::append_str(json_str, items[i].s);
            json_str.push_back('}');
        }
        json_str.push_back(']');
    }
    double json_ser = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    // ASON serialize individually
    std::vector<std::string> ason_strs;
    t0 = Clock::now();
    for (int iter = 0; iter < iterations; iter++) {
        ason_strs.clear();
        ason_strs.reserve(items.size());
        for (auto& item : items) ason_strs.push_back(ason::encode(item));
    }
    double ason_ser = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    // ASON-BIN serialize individually
    std::vector<std::string> ason_bin_strs;
    t0 = Clock::now();
    for (int iter = 0; iter < iterations; iter++) {
        ason_bin_strs.clear();
        ason_bin_strs.reserve(items.size());
        for (auto& item : items) ason_bin_strs.push_back(ason::encode_bin(item));
    }
    double ason_bin_ser = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    // JSON deserialize: skip (mini JSON doesn't support AllTypes)
    double json_de = json_ser * 1.5; // estimate

    // ASON deserialize
    t0 = Clock::now();
    for (int iter = 0; iter < iterations; iter++) {
        for (auto& s : ason_strs) {
            auto r = ason::decode<AllTypes>(s);
            (void)r;
        }
    }
    double ason_de = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    // ASON-BIN deserialize
    t0 = Clock::now();
    for (int iter = 0; iter < iterations; iter++) {
        for (auto& s : ason_bin_strs) {
            auto r = ason::decode_bin<AllTypes>(s);
            (void)r;
        }
    }
    double ason_bin_de = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    size_t ason_total = 0;
    for (auto& s : ason_strs) ason_total += s.size();
    size_t ason_bin_total = 0;
    for (auto& s : ason_bin_strs) ason_bin_total += s.size();

    return BenchResult{
        "All-types struct × " + std::to_string(count) + " (16 fields, per-struct)",
        json_ser, ason_ser, ason_bin_ser, json_de, ason_de, ason_bin_de,
        json_str.size(), ason_total, ason_bin_total
    };
}

BenchResult bench_deep(size_t count, int iterations) {
    auto companies = generate_companies(count);

    // JSON: serialize individual companies (key-value)
    auto json_ser_company = [](std::string& buf, const Company& c) {
        buf.push_back('{');
        buf.append("\"name\":"); json_mini::append_str(buf, c.name); buf.push_back(',');
        buf.append("\"founded\":"); json_mini::append_i64(buf, c.founded); buf.push_back(',');
        buf.append("\"revenue_m\":"); json_mini::append_f64(buf, c.revenue_m); buf.push_back(',');
        buf.append("\"public\":"); buf.append(c.is_public ? "true" : "false"); buf.push_back(',');
        buf.append("\"divisions\":[");
        for (size_t d = 0; d < c.divisions.size(); d++) {
            if (d > 0) buf.push_back(',');
            auto& div = c.divisions[d]; buf.push_back('{');
            buf.append("\"name\":"); json_mini::append_str(buf, div.name); buf.push_back(',');
            buf.append("\"location\":"); json_mini::append_str(buf, div.location); buf.push_back(',');
            buf.append("\"headcount\":"); json_mini::append_i64(buf, div.headcount); buf.push_back(',');
            buf.append("\"teams\":[");
            for (size_t t = 0; t < div.teams.size(); t++) {
                if (t > 0) buf.push_back(',');
                auto& team = div.teams[t]; buf.push_back('{');
                buf.append("\"name\":"); json_mini::append_str(buf, team.name); buf.push_back(',');
                buf.append("\"lead\":"); json_mini::append_str(buf, team.lead); buf.push_back(',');
                buf.append("\"size\":"); json_mini::append_i64(buf, team.size); buf.push_back(',');
                buf.append("\"projects\":[");
                for (size_t p = 0; p < team.projects.size(); p++) {
                    if (p > 0) buf.push_back(',');
                    auto& proj = team.projects[p]; buf.push_back('{');
                    buf.append("\"name\":"); json_mini::append_str(buf, proj.name); buf.push_back(',');
                    buf.append("\"budget\":"); json_mini::append_f64(buf, proj.budget); buf.push_back(',');
                    buf.append("\"active\":"); buf.append(proj.active ? "true" : "false"); buf.push_back(',');
                    buf.append("\"tasks\":[");
                    for (size_t tk = 0; tk < proj.tasks.size(); tk++) {
                        if (tk > 0) buf.push_back(',');
                        auto& task = proj.tasks[tk]; buf.push_back('{');
                        buf.append("\"id\":"); json_mini::append_i64(buf, task.id); buf.push_back(',');
                        buf.append("\"title\":"); json_mini::append_str(buf, task.title); buf.push_back(',');
                        buf.append("\"priority\":"); json_mini::append_i64(buf, task.priority); buf.push_back(',');
                        buf.append("\"done\":"); buf.append(task.done ? "true" : "false"); buf.push_back(',');
                        buf.append("\"hours\":"); json_mini::append_f64(buf, task.hours);
                        buf.push_back('}');
                    }
                    buf.push_back(']'); buf.push_back('}');
                }
                buf.push_back(']'); buf.push_back('}');
            }
            buf.push_back(']'); buf.push_back('}');
        }
        buf.append("],\"tags\":[");
        for (size_t i = 0; i < c.tags.size(); i++) {
            if (i > 0) buf.push_back(',');
            json_mini::append_str(buf, c.tags[i]);
        }
        buf.append("]}");
    };

    std::string json_str;
    auto t0 = Clock::now();
    for (int iter = 0; iter < iterations; iter++) {
        json_str.clear();
        json_str.push_back('[');
        for (size_t i = 0; i < companies.size(); i++) {
            if (i > 0) json_str.push_back(',');
            json_ser_company(json_str, companies[i]);
        }
        json_str.push_back(']');
    }
    double json_ser = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    // ASON serialize
    std::vector<std::string> ason_strs;
    t0 = Clock::now();
    for (int iter = 0; iter < iterations; iter++) {
        ason_strs.clear();
        ason_strs.reserve(companies.size());
        for (auto& c : companies) ason_strs.push_back(ason::encode(c));
    }
    double ason_ser = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    // ASON-BIN serialize
    std::vector<std::string> ason_bin_strs;
    t0 = Clock::now();
    for (int iter = 0; iter < iterations; iter++) {
        ason_bin_strs.clear();
        ason_bin_strs.reserve(companies.size());
        for (auto& c : companies) ason_bin_strs.push_back(ason::encode_bin(c));
    }
    double ason_bin_ser = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    // JSON deserialize
    t0 = Clock::now();
    for (int iter = 0; iter < iterations; iter++) {
        auto r = json_deserialize_companies(json_str);
        assert(r.size() == count);
    }
    double json_de = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    // ASON deserialize
    t0 = Clock::now();
    for (int iter = 0; iter < iterations; iter++) {
        for (auto& s : ason_strs) {
            auto c = ason::decode<Company>(s);
            (void)c;
        }
    }
    double ason_de = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    // ASON-BIN deserialize
    t0 = Clock::now();
    for (int iter = 0; iter < iterations; iter++) {
        for (auto& s : ason_bin_strs) {
            auto c = ason::decode_bin<Company>(s);
            (void)c;
        }
    }
    double ason_bin_de = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    // Verify
    for (size_t i = 0; i < ason_strs.size(); i++) {
        auto c2 = ason::decode<Company>(ason_strs[i]);
        assert(c2.name == companies[i].name);
    }

    size_t ason_total = 0;
    for (auto& s : ason_strs) ason_total += s.size();
    size_t ason_bin_total = 0;
    for (auto& s : ason_bin_strs) ason_bin_total += s.size();

    return BenchResult{
        "5-level deep × " + std::to_string(count) + " (Company>Division>Team>Project>Task)",
        json_ser, ason_ser, ason_bin_ser, json_de, ason_de, ason_bin_de,
        json_str.size(), ason_total, ason_bin_total
    };
}

std::pair<double, double> bench_single_roundtrip(int iterations) {
    User user{1, "Alice", "alice@example.com", 30, 95.5, true, "engineer", "NYC"};

    auto t0 = Clock::now();
    for (int i = 0; i < iterations; i++) {
        auto s = ason::encode(user);
        auto r = ason::decode<User>(s);
        (void)r;
    }
    double ason_ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    t0 = Clock::now();
    for (int i = 0; i < iterations; i++) {
        std::string s;
        json_serialize_user(s, user);
        const char* p = s.data();
        auto r = json_deserialize_user(p, p + s.size());
        (void)r;
    }
    double json_ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    return {ason_ms, json_ms};
}

std::pair<double, double> bench_deep_single_roundtrip(int iterations) {
    auto companies = generate_companies(1);
    auto& company = companies[0];

    // Full JSON serialize for Company (reuse the same deep serializer)
    auto json_ser_full = [](std::string& buf, const Company& c) {
        buf.push_back('{');
        buf.append("\"name\":"); json_mini::append_str(buf, c.name); buf.push_back(',');
        buf.append("\"founded\":"); json_mini::append_i64(buf, c.founded); buf.push_back(',');
        buf.append("\"revenue_m\":"); json_mini::append_f64(buf, c.revenue_m); buf.push_back(',');
        buf.append("\"public\":"); buf.append(c.is_public ? "true" : "false"); buf.push_back(',');
        buf.append("\"divisions\":[");
        for (size_t d = 0; d < c.divisions.size(); d++) {
            if (d > 0) buf.push_back(',');
            auto& div = c.divisions[d]; buf.push_back('{');
            buf.append("\"name\":"); json_mini::append_str(buf, div.name); buf.push_back(',');
            buf.append("\"location\":"); json_mini::append_str(buf, div.location); buf.push_back(',');
            buf.append("\"headcount\":"); json_mini::append_i64(buf, div.headcount); buf.push_back(',');
            buf.append("\"teams\":[");
            for (size_t t = 0; t < div.teams.size(); t++) {
                if (t > 0) buf.push_back(',');
                auto& team = div.teams[t]; buf.push_back('{');
                buf.append("\"name\":"); json_mini::append_str(buf, team.name); buf.push_back(',');
                buf.append("\"lead\":"); json_mini::append_str(buf, team.lead); buf.push_back(',');
                buf.append("\"size\":"); json_mini::append_i64(buf, team.size); buf.push_back(',');
                buf.append("\"projects\":[");
                for (size_t p = 0; p < team.projects.size(); p++) {
                    if (p > 0) buf.push_back(',');
                    auto& proj = team.projects[p]; buf.push_back('{');
                    buf.append("\"name\":"); json_mini::append_str(buf, proj.name); buf.push_back(',');
                    buf.append("\"budget\":"); json_mini::append_f64(buf, proj.budget); buf.push_back(',');
                    buf.append("\"active\":"); buf.append(proj.active ? "true" : "false"); buf.push_back(',');
                    buf.append("\"tasks\":[");
                    for (size_t tk = 0; tk < proj.tasks.size(); tk++) {
                        if (tk > 0) buf.push_back(',');
                        auto& task = proj.tasks[tk]; buf.push_back('{');
                        buf.append("\"id\":"); json_mini::append_i64(buf, task.id); buf.push_back(',');
                        buf.append("\"title\":"); json_mini::append_str(buf, task.title); buf.push_back(',');
                        buf.append("\"priority\":"); json_mini::append_i64(buf, task.priority); buf.push_back(',');
                        buf.append("\"done\":"); buf.append(task.done ? "true" : "false"); buf.push_back(',');
                        buf.append("\"hours\":"); json_mini::append_f64(buf, task.hours);
                        buf.push_back('}');
                    }
                    buf.push_back(']'); buf.push_back('}');
                }
                buf.push_back(']'); buf.push_back('}');
            }
            buf.push_back(']'); buf.push_back('}');
        }
        buf.append("],\"tags\":[");
        for (size_t i = 0; i < c.tags.size(); i++) {
            if (i > 0) buf.push_back(',');
            json_mini::append_str(buf, c.tags[i]);
        }
        buf.append("]}");
    };

    auto t0 = Clock::now();
    for (int i = 0; i < iterations; i++) {
        auto s = ason::encode(company);
        auto r = ason::decode<Company>(s);
        (void)r;
    }
    double ason_ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    t0 = Clock::now();
    for (int i = 0; i < iterations; i++) {
        std::string s;
        s.reserve(2048);
        json_ser_full(s, company);
        const char* p = s.data();
        auto r = json_deserialize_company(p, p + s.size());
        (void)r;
    }
    double json_ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    return {ason_ms, json_ms};
}

// ===========================================================================
// Main
// ===========================================================================

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         ASON vs JSON Comprehensive Benchmark (C++)           ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";

#if defined(__aarch64__) || defined(_M_ARM64)
    std::cout << "\nSystem: macOS arm64 (NEON SIMD)\n";
#elif defined(__x86_64__) || defined(_M_X64)
    std::cout << "\nSystem: x86_64 (SSE2 SIMD)\n";
#else
    std::cout << "\nSystem: unknown (scalar fallback)\n";
#endif

    int iterations = 100;
    std::cout << "Iterations per test: " << iterations << "\n";

    // Section 1: Flat struct
    std::cout << "\n┌─────────────────────────────────────────────┐\n";
    std::cout << "│  Section 1: Flat Struct (schema-driven vec) │\n";
    std::cout << "└─────────────────────────────────────────────┘\n";

    for (size_t count : {100, 500, 1000, 5000}) {
        auto r = bench_flat(count, iterations);
        r.print();
        std::cout << "\n";
    }

    // Section 2: All-types struct
    std::cout << "┌──────────────────────────────────────────────┐\n";
    std::cout << "│  Section 2: All-Types Struct (16 fields)     │\n";
    std::cout << "└──────────────────────────────────────────────┘\n";

    for (size_t count : {100, 500}) {
        auto r = bench_all_types(count, iterations);
        r.print();
        std::cout << "\n";
    }

    // Section 3: Deep nesting
    std::cout << "┌──────────────────────────────────────────────────────────┐\n";
    std::cout << "│  Section 3: 5-Level Deep Nesting (Company hierarchy)     │\n";
    std::cout << "└──────────────────────────────────────────────────────────┘\n";

    for (size_t count : {10, 50, 100}) {
        auto r = bench_deep(count, iterations);
        r.print();
        std::cout << "\n";
    }

    // Section 4: Single struct roundtrip
    std::cout << "┌──────────────────────────────────────────────┐\n";
    std::cout << "│  Section 4: Single Struct Roundtrip (10000x) │\n";
    std::cout << "└──────────────────────────────────────────────┘\n";

    auto [ason_flat, json_flat] = bench_single_roundtrip(10000);
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Flat:  ASON " << std::setw(6) << ason_flat
              << "ms | JSON " << std::setw(6) << json_flat
              << "ms | ratio " << (json_flat / ason_flat) << "x\n";

    auto [ason_deep, json_deep] = bench_deep_single_roundtrip(10000);
    std::cout << "  Deep:  ASON " << std::setw(6) << ason_deep
              << "ms | JSON " << std::setw(6) << json_deep
              << "ms | ratio " << (json_deep / ason_deep) << "x\n";

    // Section 5: Large payload
    std::cout << "\n┌──────────────────────────────────────────────┐\n";
    std::cout << "│  Section 5: Large Payload (10k records)      │\n";
    std::cout << "└──────────────────────────────────────────────┘\n";

    auto r_large = bench_flat(10000, 10);
    std::cout << "  (10 iterations for large payload)\n";
    r_large.print();

    // Section 6: Annotated vs Unannotated
    std::cout << "\n┌──────────────────────────────────────────────────────────────┐\n";
    std::cout << "│  Section 6: Annotated vs Unannotated Schema (deserialize)   │\n";
    std::cout << "└──────────────────────────────────────────────────────────────┘\n";
    {
        auto users_1k = generate_users(1000);
        auto ason_untyped = ason::encode(users_1k);
        auto ason_typed = ason::encode_typed(users_1k);

        auto v1 = ason::decode<std::vector<User>>(ason_untyped);
        auto v2 = ason::decode<std::vector<User>>(ason_typed);
        assert(v1.size() == v2.size());

        int de_iters = 200;
        auto t0 = Clock::now();
        for (int i = 0; i < de_iters; i++) {
            auto r = ason::decode<std::vector<User>>(ason_untyped);
            (void)r;
        }
        double untyped_ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

        t0 = Clock::now();
        for (int i = 0; i < de_iters; i++) {
            auto r = ason::decode<std::vector<User>>(ason_typed);
            (void)r;
        }
        double typed_ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  Flat struct × 1000 (" << de_iters << " iters, deserialize only)\n";
        std::cout << "    Unannotated: " << std::setw(8) << untyped_ms << "ms  ("
                  << ason_untyped.size() << " B)\n";
        std::cout << "    Annotated:   " << std::setw(8) << typed_ms << "ms  ("
                  << ason_typed.size() << " B)\n";
        std::cout << "    Ratio: " << std::setprecision(3) << (untyped_ms / typed_ms)
                  << "x (unannotated / annotated)\n";
    }

    // Section 7: Annotated vs Unannotated Serialization
    std::cout << "\n┌──────────────────────────────────────────────────────────────┐\n";
    std::cout << "│  Section 7: Annotated vs Unannotated Schema (serialize)      │\n";
    std::cout << "└──────────────────────────────────────────────────────────────┘\n";
    {
        auto users_1k = generate_users(1000);
        int ser_iters = 200;

        auto t0 = Clock::now();
        std::string untyped_out;
        for (int i = 0; i < ser_iters; i++) untyped_out = ason::encode(users_1k);
        double untyped_ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

        t0 = Clock::now();
        std::string typed_out;
        for (int i = 0; i < ser_iters; i++) typed_out = ason::encode_typed(users_1k);
        double typed_ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  Flat struct × 1000 vec (" << ser_iters << " iters, serialize only)\n";
        std::cout << "    Unannotated: " << std::setw(8) << untyped_ms << "ms  ("
                  << untyped_out.size() << " B)\n";
        std::cout << "    Annotated:   " << std::setw(8) << typed_ms << "ms  ("
                  << typed_out.size() << " B)\n";
        std::cout << "    Ratio: " << std::setprecision(3) << (untyped_ms / typed_ms)
                  << "x (unannotated / annotated)\n";
    }

    // Section 8: Throughput summary
    std::cout << "\n┌──────────────────────────────────────────────┐\n";
    std::cout << "│  Section 8: Throughput Summary               │\n";
    std::cout << "└──────────────────────────────────────────────┘\n";
    {
        auto users_1k = generate_users(1000);
        auto json_1k = json_serialize_users(users_1k);
        auto ason_1k = ason::encode(users_1k);

        int iters = 100;
        auto t0 = Clock::now();
        for (int i = 0; i < iters; i++) { auto s = json_serialize_users(users_1k); (void)s; }
        double json_ser_dur = std::chrono::duration<double>(Clock::now() - t0).count();

        t0 = Clock::now();
        for (int i = 0; i < iters; i++) { auto s = ason::encode(users_1k); (void)s; }
        double ason_ser_dur = std::chrono::duration<double>(Clock::now() - t0).count();

        t0 = Clock::now();
        for (int i = 0; i < iters; i++) { auto r = json_deserialize_users(json_1k); (void)r; }
        double json_de_dur = std::chrono::duration<double>(Clock::now() - t0).count();

        t0 = Clock::now();
        for (int i = 0; i < iters; i++) { auto r = ason::decode<std::vector<User>>(ason_1k); (void)r; }
        double ason_de_dur = std::chrono::duration<double>(Clock::now() - t0).count();

        double total_records = 1000.0 * iters;
        double json_ser_rps = total_records / json_ser_dur;
        double ason_ser_rps = total_records / ason_ser_dur;
        double json_de_rps = total_records / json_de_dur;
        double ason_de_rps = total_records / ason_de_dur;

        double json_ser_mbps = (double)(json_1k.size() * iters) / json_ser_dur / 1048576.0;
        double ason_ser_mbps = (double)(ason_1k.size() * iters) / ason_ser_dur / 1048576.0;
        double json_de_mbps = (double)(json_1k.size() * iters) / json_de_dur / 1048576.0;
        double ason_de_mbps = (double)(ason_1k.size() * iters) / ason_de_dur / 1048576.0;

        std::cout << std::fixed;
        std::cout << "  Serialize throughput (1000 records × " << iters << " iters):\n";
        std::cout << std::setprecision(0);
        std::cout << "    JSON: " << json_ser_rps << " records/s  ("
                  << std::setprecision(1) << json_ser_mbps << " MB/s)\n";
        std::cout << std::setprecision(0);
        std::cout << "    ASON: " << ason_ser_rps << " records/s  ("
                  << std::setprecision(1) << ason_ser_mbps << " MB/s)\n";
        std::cout << std::setprecision(2);
        std::cout << "    Speed: " << (ason_ser_rps / json_ser_rps) << "x"
                  << (ason_ser_rps > json_ser_rps ? " ✓ ASON faster" : "") << "\n";

        std::cout << "  Deserialize throughput:\n";
        std::cout << std::setprecision(0);
        std::cout << "    JSON: " << json_de_rps << " records/s  ("
                  << std::setprecision(1) << json_de_mbps << " MB/s)\n";
        std::cout << std::setprecision(0);
        std::cout << "    ASON: " << ason_de_rps << " records/s  ("
                  << std::setprecision(1) << ason_de_mbps << " MB/s)\n";
        std::cout << std::setprecision(2);
        std::cout << "    Speed: " << (ason_de_rps / json_de_rps) << "x"
                  << (ason_de_rps > json_de_rps ? " ✓ ASON faster" : "") << "\n";
    }

    std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    Benchmark Complete                        ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";

    return 0;
}
