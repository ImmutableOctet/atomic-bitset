#include "atomic-bitset/atomic-bitset.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Name is atomic-bitset", "[library]")
{
  REQUIRE(name() == "atomic-bitset");
}
