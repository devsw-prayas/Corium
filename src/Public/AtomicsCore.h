/*
* Copyright (c) 2026 StormWeaver
*
* This file is part of the Corium Multithreading API
*
* Licensed under the MIT License. You may obtain a copy of the License at
* https://opensource.org/licenses/MIT
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND...
*/
#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>

// Header-only on purpose: nothing here is exported, so consumers need an include path and no link to Corium.

namespace Corium::Atomics {
	// Values match the legacy CORIUM_MEMORY_ORDER_* constants so the runtime-order shim can cast straight across.
	enum class MemoryOrder : uint8_t {
		RELAXED = 0,
		CONSUME = 1,
		ACQUIRE = 2,
		RELEASE = 3,
		ACQ_REL = 4,
		SEQ_CST = 5
	};

	// 1/2/4/8 byte trivially copyable types are atomic at their own width. No widening: a 1-byte atomic never touches its neighbours.
	template<typename T>
	concept AtomicWord = std::is_trivially_copyable_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T>
		&& (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);

	template<typename T>
	concept AtomicInteger = AtomicWord<T> && std::integral<T> && !std::same_as<T, bool>;

	template<MemoryOrder Order>
	concept LoadOrder = Order == MemoryOrder::RELAXED || Order == MemoryOrder::CONSUME
		|| Order == MemoryOrder::ACQUIRE || Order == MemoryOrder::SEQ_CST;

	template<MemoryOrder Order>
	concept StoreOrder = Order == MemoryOrder::RELAXED || Order == MemoryOrder::RELEASE
		|| Order == MemoryOrder::SEQ_CST;

	template<MemoryOrder Success, MemoryOrder Failure>
	concept CasOrders = LoadOrder<Failure>;

	namespace detail {
		template<typename T>
		using Raw = std::conditional_t<sizeof(T) == 1, uint8_t,
			std::conditional_t<sizeof(T) == 2, uint16_t,
			std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>>>;

		// Weakest legal failure order for a given success order.
		constexpr MemoryOrder failureOrderFor(MemoryOrder v_Success) noexcept {
			switch (v_Success) {
			case MemoryOrder::SEQ_CST: return MemoryOrder::SEQ_CST;
			case MemoryOrder::ACQ_REL:
			case MemoryOrder::ACQUIRE: return MemoryOrder::ACQUIRE;
			case MemoryOrder::CONSUME: return MemoryOrder::CONSUME;
			case MemoryOrder::RELEASE:
			case MemoryOrder::RELAXED: return MemoryOrder::RELAXED;
			}
			return MemoryOrder::SEQ_CST;
		}

		constexpr bool isAcquire(MemoryOrder v_Order) noexcept {
			return v_Order == MemoryOrder::CONSUME || v_Order == MemoryOrder::ACQUIRE
				|| v_Order == MemoryOrder::ACQ_REL || v_Order == MemoryOrder::SEQ_CST;
		}

		constexpr bool isRelease(MemoryOrder v_Order) noexcept {
			return v_Order == MemoryOrder::RELEASE || v_Order == MemoryOrder::ACQ_REL || v_Order == MemoryOrder::SEQ_CST;
		}
	}
}
