from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy
import os


class AsunCppConan(ConanFile):
    name = "asun-cpp"
    version = "1.0.0"
    package_type = "header-library"
    license = "MIT"
    author = "asun contributors"
    url = "https://github.com/asunLab/asun/tree/main/asun-cpp"
    description = "Header-only C++17 ASUN (Array-Schema Unified Notation) library"
    topics = ("asun", "serialization", "schema", "header-only", "data-format")
    settings = "os", "arch", "compiler", "build_type"
    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "include/*",
        "README.md",
    )
    no_copy_source = True

    def package_id(self):
        self.info.clear()

    def layout(self):
        cmake_layout(self)

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.variables["ASUN_BUILD_EXAMPLES"] = False
        toolchain.variables["ASUN_BUILD_TESTS"] = False
        toolchain.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(
            self,
            "README.md",
            self.source_folder,
            os.path.join(self.package_folder, "share", "doc", "asun-cpp"),
        )

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "asun")
        self.cpp_info.set_property("cmake_target_name", "asun::asun")
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
