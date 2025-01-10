from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain

class GameAsioProjectConan(ConanFile):
    name = "GameAsio"
    version = "0.1"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    requires = (
        "boost/1.86.0",
        "glm/cci.20230113",
        "catch2/3.7.1",
        "libpqxx/7.10.0"
    )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
