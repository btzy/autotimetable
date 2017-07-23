#pragma once

#include <cstdint>

#ifdef _MSC_VER

#include <intrin.h>

namespace intrinsics {

	// assumes at least one set bit
	inline std::uint32_t find_first_set(std::uint32_t mask) {
		unsigned long res;
		_BitScanReverse(&res, static_cast<unsigned long>(mask));
		return static_cast<std::uint32_t>(res);
	}

	// assumes at least one set bit
	inline std::uint32_t find_last_set(std::uint32_t mask) {
		unsigned long res;
		_BitScanForward(&res, static_cast<unsigned long>(mask));
		return static_cast<std::uint32_t>(res);
	}

}

#endif

