cmake_minimum_required(VERSION 3.14)

project(atomic-bitsetTests LANGUAGES CXX)

include(../cmake/project-is-top-level.cmake)
include(../cmake/folders.cmake)

# ---- Dependencies ----

if(PROJECT_IS_TOP_LEVEL)
  find_package(atomic-bitset REQUIRED)
  enable_testing()
endif()

find_package(Catch2 REQUIRED)
include(Catch)

# ---- Tests ----

add_executable(atomic_bitset_test source/atomic_bitset_test.cpp)

target_link_libraries(
    atomic_bitset_test PRIVATE
    immutableoctet::atomic-bitset
    Catch2::Catch2WithMain
)
target_compile_features(atomic_bitset_test PRIVATE cxx_std_20)

catch_discover_tests(atomic_bitset_test)

# ---- End-of-file commands ----

add_folders(Test)
