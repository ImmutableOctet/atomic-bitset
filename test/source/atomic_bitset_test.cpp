#include <catch2/catch_test_macros.hpp>

#include <immutableoctet/atomic_bitset/atomic_bitset.hpp>

#include <thread>

#include <cstddef>
#include <cstdint>

TEST_CASE("immutableoctet::atomic_bitset", "[atomic-bitset]")
{
	using bitset_t = immutableoctet::basic_atomic_bitset<std::uint64_t, 512, std::numeric_limits<std::uint64_t>::max()>;

	SECTION("Free indexing")
	{
		auto bitset = bitset_t {};

		bitset[0] = false;

		REQUIRE(bitset.size() == 1);
		REQUIRE(bitset.element_count() == 1);
		REQUIRE(bitset.page_count() >= 1);

		bitset[99] = false;

		REQUIRE(bitset.size() == 100);
		REQUIRE(bitset.element_count() == 2);
		REQUIRE(bitset.page_count() >= 1);

		auto sum_of_bits = std::size_t {};

		for (const bool bit : bitset)
		{
			sum_of_bits += static_cast<std::size_t>(bit);
		}

		REQUIRE(sum_of_bits == 98);
	}

	SECTION("Simultaneous access")
	{
		auto bitset = bitset_t {};

		std::size_t n_elements = 4096 * 64; // 512

		auto work = [&bitset](std::size_t n_elements, std::size_t stride, std::size_t offset={})
		{
			for (std::size_t index = offset; index < n_elements; index += stride)
			{
				bitset[index] = true;
			}
		};

		{
			auto first = std::jthread
			{
				work,

				n_elements,

				static_cast<std::size_t>(3),
				static_cast<std::size_t>(0)
			};

			auto second = std::jthread
			{
				work,

				n_elements,

				static_cast<std::size_t>(3),
				static_cast<std::size_t>(1)
			};

			auto third = std::jthread
			{
				work,

				n_elements,

				static_cast<std::size_t>(3),
				static_cast<std::size_t>(2)
			};
		}

		std::size_t sum_of_bits = 0;

		for (const bool bit : bitset)
		{
			sum_of_bits += static_cast<std::size_t>(bit);
		}

		REQUIRE(sum_of_bits == n_elements);
	}
}