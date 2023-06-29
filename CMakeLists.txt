cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    atomic-bitset
    VERSION 0.1.0
    DESCRIPTION "Short description"
    HOMEPAGE_URL "https://example.com/"
    LANGUAGES NONE
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(atomic-bitset_atomic-bitset INTERFACE)
add_library(atomic-bitset::atomic-bitset ALIAS atomic-bitset_atomic-bitset)

set_property(
    TARGET atomic-bitset_atomic-bitset PROPERTY
    EXPORT_NAME atomic-bitset
)

target_include_directories(
    atomic-bitset_atomic-bitset ${warning_guard}
    INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_compile_features(atomic-bitset_atomic-bitset INTERFACE cxx_std_20)

find_package(fmt REQUIRED)
target_link_libraries(atomic-bitset_atomic-bitset INTERFACE fmt::fmt)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT atomic-bitset_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of atomic-bitset"
  )
endif()

include(cmake/dev-mode.cmake)
