cmake_minimum_required(VERSION 3.12)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

# Suppress warning
SET(GCC_COVERAGE_COMPILE_FLAGS "-Wno-volatile")
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")


include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

find_package(Boost 1.81.0 REQUIRED)

find_path(GLM_INCLUDE_DIR NAMES glm PATHS ${CONAN_INCLUDE_DIRS_GLM} NO_CMAKE_FIND_ROOT_PATH)
set(GLM_INCLUDE_DIRS ${GLM_INCLUDE_DIR};${GLM_INCLUDE_DIR}/glm)


set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

file(GLOB model_lib_SRC CONFIGURE_DEPENDS "src/lib/*.h" "src/lib/*.cpp")
add_library(model_lib STATIC ${model_lib_SRC})

# Добавляем сторонние библиотеки. Указываем видимость PUBLIC, т. к.
# они должны быть видны и в библиотеке MyLib и в зависимостях.
target_include_directories(model_lib PUBLIC CONAN_PKG::boost ${GLM_INCLUDE_DIR})
target_link_libraries(model_lib PUBLIC Threads::Threads CONAN_PKG::boost)


file(GLOB game_server_SRC CONFIGURE_DEPENDS "src/app/*.h" "src/app/*.cpp")
add_executable(game_server ${game_server_SRC} src/app/game_db.h src/app/game_db.cpp)

file(GLOB game_server_tests_SRC CONFIGURE_DEPENDS "tests/*.h" "tests/*.cpp")
add_executable(game_server_tests ${game_server_tests_SRC})

# Добавляем зависимость целей от статической библиотеки.
# target_include_directories уже не нужен - он добавится автоматически из зависимой библиотеки.
target_link_libraries(game_server model_lib CONAN_PKG::libpqxx)
target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 model_lib)
