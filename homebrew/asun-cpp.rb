class AsunCpp < Formula
  desc "Header-only C++17 ASUN (Array-Schema Unified Notation) library"
  homepage "https://github.com/asun-lab/asun/tree/main/asun-cpp"
  url "https://github.com/asun-lab/asun/archive/refs/tags/v1.0.0.tar.gz"
  sha256 "REPLACE_WITH_RELEASE_SHA256"
  license "MIT"
  head "https://github.com/asun-lab/asun.git", branch: "main"

  depends_on "cmake" => :build

  def install
    system "cmake", "-S", "asun-cpp", "-B", "build",
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DASUN_BUILD_EXAMPLES=OFF",
                    "-DASUN_BUILD_TESTS=OFF",
                    *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    (testpath/"test.cpp").write <<~CPP
      #include <asun.hpp>
      #include <string>
      #include <vector>
      #include <cstdint>

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
          std::vector<User> rows = {{1, "Alice", true}, {2, "Bob", false}};
          auto text = asun::encode_typed(rows);
          auto out = asun::decode<std::vector<User>>(text);
          return out.size() == 2 ? 0 : 1;
      }
    CPP

    system ENV.cxx, "test.cpp", "-std=c++17", "-I#{include}", "-o", "test"
    system "./test"
  end
end
