#pragma once

#include <cstdint>

#if defined(_MSC_VER)

#include <intrin.h>

namespace intrinsics {

	// assumes at least one set bit
	inline std::uint32_t find_largest_set(std::uint32_t mask) {
		unsigned long res;
		_BitScanReverse(&res, static_cast<unsigned long>(mask));
		return static_cast<std::uint32_t>(res);
	}

	// assumes at least one set bit
	inline std::uint32_t find_smallest_set(std::uint32_t mask) {
		unsigned long res;
		_BitScanForward(&res, static_cast<unsigned long>(mask));
		return static_cast<std::uint32_t>(res);
	}

}

#elif defined(__GNUC__)

namespace intrinsics {

	// assumes at least one set bit
	inline std::uint32_t find_largest_set(std::uint32_t mask) {
		return 31u - static_cast<std::uint32_t>(__builtin_clz(mask));
	}

	// assumes at least one set bit
	inline std::uint32_t find_smallest_set(std::uint32_t mask) {
		return static_cast<std::uint32_t>(__builtin_ctz(mask));
	}

}

#endif

