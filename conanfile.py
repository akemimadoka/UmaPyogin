from __future__ import generators
from conans import ConanFile, CMake, tools

class UmaPyoginConan(ConanFile):
    name = "UmaPyogin"
    version = "0.1"
    license = "MIT"

    settings = "os", "compiler", "build_type", "arch"

    requires = "simdjson/1.0.2", "fmt/8.1.1"

    generators = "cmake"

    exports_sources = "CMakeLists.txt", "src/*"

    def configure_cmake(self):
        cmake = CMake(self)
        cmake.configure()
        return cmake

    def build(self):
        cmake = self.configure_cmake()
        cmake.build()

    def package(self):
        cmake = self.configure_cmake()
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)
