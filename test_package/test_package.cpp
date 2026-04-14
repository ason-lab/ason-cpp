#include <asun.hpp>

#include <cstdint>
#include <string>
#include <vector>

struct User {
    std::int64_t id = 0;
    std::string name;
    bool active = false;
};

ASUN_FIELDS(User,
    (id, "id", "int"),
    (name, "name", "str"),
    (active, "active", "bool"))

int main() {
    std::vector<User> users = {
        {1, "Alice", true},
        {2, "Bob", false},
    };

    auto encoded = asun::encode_typed(users);
    auto decoded = asun::decode<std::vector<User>>(encoded);
    return decoded.size() == 2 ? 0 : 1;
}
