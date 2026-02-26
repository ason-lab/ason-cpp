#include "ason.hpp"
#include <iostream>
#include <vector>

struct Detail {
    int64_t id;
    std::string name;
    int32_t age;
    bool gender;
};

ASON_FIELDS(Detail, (id, "ID", "int"), (name, "Name", "str"), (age, "Age", "int"), (gender, "Gender", "bool"))

struct User {
    std::vector<Detail> details;
};

ASON_FIELDS(User, (details, "details", "[{ID:int,Name:str,Age:int,Gender:bool}]"))

struct Person {
    int64_t id;
    std::string name;
    int32_t age;
};

ASON_FIELDS(Person, (id, "ID", "int"), (name, "Name", "str"), (age, "Age", "int"))

struct Human {
    std::vector<Person> details;
};

ASON_FIELDS(Human, (details, "details", "[{ID:int,Name:str,Age:int}]"))

int main() {
    std::vector<User> users = {
        User{
            {
                Detail{1, "Alice", 30, true},
                Detail{2, "Bob", 25, false}
            }
        }
    };

    // Encode
    auto ason_str = ason::encode(users);
    std::cout << "Encoded ASON:\n" << ason_str << std::endl;

    // Decode into Human
    auto decoded = ason::decode<std::vector<Human>>(ason_str);
    std::cout << "\nDecoded into Human list:\n";
    for (const auto& h : decoded) {
        std::cout << "Human{details=[";
        for (size_t i = 0; i < h.details.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << "Person{ID=" << h.details[i].id << ", Name=\"" << h.details[i].name << "\", Age=" << h.details[i].age << "}";
        }
        std::cout << "]}" << std::endl;
    }

    return 0;
}
