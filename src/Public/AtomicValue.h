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

#include "Atomics.h"

namespace Corium::Atomics {
	// One value, any 1/2/4/8 byte atomic word. sizeof(AtomicValue<T>) == sizeof(T): no padding to 4, no widening.
	template<AtomicWord T>
	struct alignas(sizeof(T)) AtomicValue final {
	private:
		T m_Value;

	public:
		AtomicValue() noexcept = default;
		~AtomicValue() = default;

		explicit AtomicValue(T v_Value) noexcept : m_Value(v_Value) {}

		AtomicValue(const AtomicValue&) = default;
		AtomicValue& operator=(const AtomicValue&) = default;
		AtomicValue(AtomicValue&&) noexcept = default;
		AtomicValue& operator=(AtomicValue&&) noexcept = default;

		template<MemoryOrder Order = MemoryOrder::SEQ_CST> requires LoadOrder<Order>
		[[nodiscard]] CORIUM_FORCEINLINE T load() const noexcept { return Atomics::load<Order>(&m_Value); }

		template<MemoryOrder Order = MemoryOrder::SEQ_CST> requires StoreOrder<Order>
		CORIUM_FORCEINLINE void store(T v_Value) noexcept { Atomics::store<Order>(&m_Value, v_Value); }

		template<MemoryOrder Order = MemoryOrder::SEQ_CST>
		[[nodiscard]] CORIUM_FORCEINLINE T exchange(T v_Value) noexcept { return Atomics::exchange<Order>(&m_Value, v_Value); }

		template<MemoryOrder Success = MemoryOrder::SEQ_CST, MemoryOrder Failure = detail::failureOrderFor(Success)>
			requires CasOrders<Success, Failure>
		CORIUM_FORCEINLINE bool compareExchange(T& r_Expected, T v_Desired) noexcept {
			return Atomics::compareExchange<Success, Failure>(&m_Value, r_Expected, v_Desired);
		}

		template<MemoryOrder Order = MemoryOrder::SEQ_CST> requires AtomicInteger<T>
		CORIUM_FORCEINLINE T fetchAdd(T v_Value) noexcept { return Atomics::fetchAdd<Order>(&m_Value, v_Value); }

		template<MemoryOrder Order = MemoryOrder::SEQ_CST> requires AtomicInteger<T>
		CORIUM_FORCEINLINE T fetchSub(T v_Value) noexcept { return Atomics::fetchSub<Order>(&m_Value, v_Value); }

		template<MemoryOrder Order = MemoryOrder::SEQ_CST> requires AtomicInteger<T>
		CORIUM_FORCEINLINE T fetchAnd(T v_Value) noexcept { return Atomics::fetchAnd<Order>(&m_Value, v_Value); }

		template<MemoryOrder Order = MemoryOrder::SEQ_CST> requires AtomicInteger<T>
		CORIUM_FORCEINLINE T fetchOr(T v_Value) noexcept { return Atomics::fetchOr<Order>(&m_Value, v_Value); }

		template<MemoryOrder Order = MemoryOrder::SEQ_CST> requires AtomicInteger<T>
		CORIUM_FORCEINLINE T fetchXor(T v_Value) noexcept { return Atomics::fetchXor<Order>(&m_Value, v_Value); }

		template<MemoryOrder Order = MemoryOrder::SEQ_CST> requires AtomicInteger<T>
		CORIUM_FORCEINLINE T fetchNand(T v_Value) noexcept { return Atomics::fetchNand<Order>(&m_Value, v_Value); }

		template<MemoryOrder Order = MemoryOrder::SEQ_CST> requires AtomicInteger<T>
		CORIUM_FORCEINLINE T fetchMin(T v_Value) noexcept { return Atomics::fetchMin<Order>(&m_Value, v_Value); }

		template<MemoryOrder Order = MemoryOrder::SEQ_CST> requires AtomicInteger<T>
		CORIUM_FORCEINLINE T fetchMax(T v_Value) noexcept { return Atomics::fetchMax<Order>(&m_Value, v_Value); }

		// Return the NEW value, matching the legacy increment/decrement.
		template<MemoryOrder Order = MemoryOrder::SEQ_CST> requires AtomicInteger<T>
		CORIUM_FORCEINLINE T increment() noexcept { return Atomics::increment<Order>(&m_Value); }

		template<MemoryOrder Order = MemoryOrder::SEQ_CST> requires AtomicInteger<T>
		CORIUM_FORCEINLINE T decrement() noexcept { return Atomics::decrement<Order>(&m_Value); }

		// Return whether the bit was set before the operation.
		template<MemoryOrder Order = MemoryOrder::SEQ_CST> requires AtomicInteger<T>
		CORIUM_FORCEINLINE bool testAndSet(unsigned v_Bit) noexcept { return Atomics::bitTestAndSet<Order>(&m_Value, v_Bit); }

		template<MemoryOrder Order = MemoryOrder::SEQ_CST> requires AtomicInteger<T>
		CORIUM_FORCEINLINE bool testAndReset(unsigned v_Bit) noexcept { return Atomics::bitTestAndReset<Order>(&m_Value, v_Bit); }

		CORIUM_FORCEINLINE T* data() noexcept { return &m_Value; }
		CORIUM_FORCEINLINE const T* data() const noexcept { return &m_Value; }
	};

	static_assert(sizeof(AtomicValue<uint8_t>) == 1 && sizeof(AtomicValue<uint16_t>) == 2
		&& sizeof(AtomicValue<uint32_t>) == 4 && sizeof(AtomicValue<uint64_t>) == 8, "AtomicValue must not widen");
}
