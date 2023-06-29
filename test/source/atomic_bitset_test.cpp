#include "atomic_bitset/atomic_bitset.hpp"

#include <iostream>
#include <limits>
#include <array>

int main()
{
	auto bitset = immutableoctet::basic_atomic_bitset<std::uint64_t, 512, std::numeric_limits<std::uint64_t>::max()>{}; // immutableoctet::atomic_bitset {};

	/*
	bitset.emplace_back(false);
	bitset.emplace_back(true);
	bitset.emplace_back(true);
	bitset.emplace_back(false);
	bitset.emplace_back(true);
	bitset.emplace_back(true);
	bitset.emplace_back(false);
	bitset.emplace_back(true);
	bitset.emplace_back(false);
	*/

	bitset[0] = false;

	bitset[99] = false;

	int sum = 0;

	for (const bool bit : bitset)
	{
		sum += bit;
	}

	std::cout << "Size: " << bitset.size() << '\n';
	std::cout << "Capacity: " << bitset.capacity() << '\n';
	std::cout << "Pages: " << bitset.page_count() << '\n';
	std::cout << "Sum: " << sum << '\n';

	/*
	for (const auto bit : bitset)
	{
		std::cout << (bit) ? '1' : '0';
	}
	*/

	return 0;
}