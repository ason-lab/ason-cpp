#include "ason.hpp"
#include <cassert>
#include <iostream>

struct User {
  int64_t id = 0;
  std::string name;
  bool active = false;
};

ASON_FIELDS(User, (id, "id", "int"), (name, "name", "str"),
            (active, "active", "bool"))

struct Item {
  int64_t id = 0;
  std::optional<std::string> label;
};

ASON_FIELDS(Item, (id, "id", "int"), (label, "label", "str"))

struct Tagged {
  std::string name;
  std::vector<std::string> tags;
};

ASON_FIELDS(Tagged, (name, "name", "str"), (tags, "tags", "[str]"))

int main() {
  std::cout << "=== ASON Basic Examples (C++) ===\n\n";

  // 1. Serialize a single struct
  User user{1, "Alice", true};
  auto ason_str = ason::encode(user);
  std::cout << "1. Serialize single struct:\n   " << ason_str << "\n\n";

  // 2. Serialize with type annotations
  auto typed_str = ason::encode_typed(user);
  std::cout << "2. Serialize with type annotations:\n   " << typed_str
            << "\n\n";
  assert(typed_str.find("{id:int,name:str,active:bool}:") == 0);

  // 3. Deserialize from ASON
  auto loaded =
      ason::decode<User>("{id:int,name:str,active:bool}:(1,Alice,true)");
  std::cout << "3. Deserialize single struct:\n   "
            << "User{id=" << loaded.id << ", name=\"" << loaded.name
            << "\", active=" << (loaded.active ? "true" : "false") << "}\n\n";
  assert(loaded.id == 1);
  assert(loaded.name == "Alice");
  assert(loaded.active == true);

  // 4. Serialize a vec of structs
  std::vector<User> users = {
      {1, "Alice", true},
      {2, "Bob", false},
      {3, "Carol Smith", true},
  };
  auto ason_vec = ason::encode(users);
  std::cout << "4. Serialize vec (schema-driven):\n   " << ason_vec << "\n\n";

  // 5. Serialize vec with type annotations
  auto typed_vec = ason::encode_typed(users);
  std::cout << "5. Serialize vec with type annotations:\n   " << typed_vec
            << "\n\n";
  assert(typed_vec.find("[{id:int,name:str,active:bool}]:") == 0);

  // 6. Deserialize vec
  auto input6 = std::string_view("[{id:int,name:str,active:bool}]:(1,Alice,true),"
                                 "(2,Bob,false),(3,\"Carol Smith\",true)");
  auto users2 = ason::decode<std::vector<User>>(input6);
  std::cout << "6. Deserialize vec:\n";
  for (auto &u : users2) {
    std::cout << "   User{id=" << u.id << ", name=\"" << u.name
              << "\", active=" << (u.active ? "true" : "false") << "}\n";
  }
  assert(users2.size() == 3);
  assert(users2[2].name == "Carol Smith");

  // 7. Multiline format
  std::cout << "\n7. Multiline format:\n";
  auto multiline = std::string_view("[{id:int, name:str, active:bool}]:\n"
                                    "  (1, Alice, true),\n"
                                    "  (2, Bob, false),\n"
                                    "  (3, \"Carol Smith\", true)");
  auto users3 = ason::decode<std::vector<User>>(multiline);
  for (auto &u : users3) {
    std::cout << "   User{id=" << u.id << ", name=\"" << u.name
              << "\", active=" << (u.active ? "true" : "false") << "}\n";
  }
  assert(users3.size() == 3);

  // 8. Roundtrip (ASON-text + ASON-bin)
  std::cout << "\n8. Roundtrip (ASON-text vs ASON-bin):\n";
  User original{42, "Test User", true};

  // ASON text roundtrip
  auto serialized = ason::encode(original);
  auto deserialized = ason::decode<User>(serialized);
  std::cout << "   ASON text:    " << serialized << " (" << serialized.size()
            << " B)\n";
  assert(deserialized.id == original.id);
  assert(deserialized.name == original.name);

  // ASON binary roundtrip
  auto binary = ason::encode_bin(original);
  auto deserialized_bin = ason::decode_bin<User>(binary);
  std::cout << "   ASON binary:  " << binary.size() << " B\n";
  assert(deserialized_bin.id == original.id);
  assert(deserialized_bin.name == original.name);

  std::cout << "   BIN is "
            << (1.0 - (double)binary.size() / (double)serialized.size()) * 100.0
            << "% smaller than text\n";
  std::cout << "   ✓ both formats roundtrip OK\n";

  // 9. Optional fields
  std::cout << "\n9. Optional fields:\n";
  auto item1 = ason::decode<Item>("{id,label}:(1,hello)");
  std::cout << "   with value: Item{id=" << item1.id << ", label="
            << (item1.label ? "\"" + *item1.label + "\"" : "nullopt") << "}\n";
  assert(item1.label.has_value());
  assert(*item1.label == "hello");

  auto item2 = ason::decode<Item>("{id,label}:(2,)");
  std::cout << "   with null:  Item{id=" << item2.id << ", label="
            << (item2.label ? "\"" + *item2.label + "\"" : "nullopt") << "}\n";
  assert(!item2.label.has_value());

  // 10. Array fields
  std::cout << "\n10. Array fields:\n";
  auto t = ason::decode<Tagged>("{name,tags}:(Alice,[rust,go,python])");
  std::cout << "   Tagged{name=\"" << t.name << "\", tags=[";
  for (size_t i = 0; i < t.tags.size(); i++) {
    if (i > 0)
      std::cout << ",";
    std::cout << t.tags[i];
  }
  std::cout << "]}\n";
  assert(t.tags.size() == 3);

  // 11. Comments
  std::cout << "\n11. With comments:\n";
  auto user4 =
      ason::decode<User>("/* user list */ {id,name,active}:(1,Alice,true)");
  std::cout << "   User{id=" << user4.id << ", name=\"" << user4.name
            << "\"}\n";
  assert(user4.id == 1);

  std::cout << "\n=== All examples passed! ===\n";
  return 0;
}
