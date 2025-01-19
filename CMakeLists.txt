cmake_minimum_required(VERSION 3.15)

#set(CMAKE_COLOR_DIAGNOSTICS ON CACHE BOOL "Enable color diagnostics")
message("CMAKE_COLOR_DIAGNOSTICS=${CMAKE_COLOR_DIAGNOSTICS}")

# Используем toolchain файл из образа Docker
include ($ENV{HOME}/asio_game/conan/conan_toolchain.cmake)

project(game_asio_server CXX)

# Suppress warning
SET(GCC_COVERAGE_COMPILE_FLAGS "-Wno-volatile")
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")
SET(Boost_NO_WARN_NEW_VERSIONS 1)

# 1.86.0 Boost
find_package(Boost REQUIRED
             COMPONENTS json)
# https://conan.io/center/recipes/libpqxx
find_package(libpqxx REQUIRED)
# https://conan.io/center/recipes/catch2
find_package(Catch2 REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

file(GLOB model_lib_SRC CONFIGURE_DEPENDS "src/lib/*.h" "src/lib/*.cpp")
add_library(model_lib STATIC ${model_lib_SRC})

# Добавляем сторонние библиотеки. Указываем видимость PUBLIC, т. к.
# они должны быть видны и в библиотеке и в зависимостях.
target_include_directories(model_lib PUBLIC ${CMAKE_INCLUDE_PATH})
target_link_libraries(model_lib PUBLIC boost::boost libpqxx::pqxx Threads::Threads)


file(GLOB game_server_SRC CONFIGURE_DEPENDS "src/app/*.h" "src/app/*.cpp")
add_executable(game_server ${game_server_SRC} src/app/game_db.h src/app/game_db.cpp)

file(GLOB game_server_tests_SRC CONFIGURE_DEPENDS "tests/*.h" "tests/*.cpp")
add_executable(game_server_tests ${game_server_tests_SRC})

# Добавляем зависимость целей от статической библиотеки.
# target_include_directories уже не нужен - он добавится автоматически из зависимой библиотеки.
target_link_libraries(game_server model_lib)
target_link_libraries(game_server_tests PRIVATE Catch2::Catch2WithMain model_lib)

set(CMAKE_INSTALL_PREFIX "/opt/asio_game/bin")

install(TARGETS game_server DESTINATION "${CMAKE_INSTALL_PREFIX}")