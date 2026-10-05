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

// Public atomics surface. Memory order is a template parameter, so an invalid order is a compile error
// and there is no runtime switch. Values are passed as T (not deduced), so store<O>(p, 1u) never fights the pointer.
// Callers must keep operands naturally aligned (AtomicValue guarantees it).

#include "AtomicsCore.h"
#include <bit>

#if defined(_MSC_VER) && !defined(__clang__)
#include "AtomicsMsvc.h"
#else
#include "AtomicsGnu.h"
#endif

namespace Corium::Atomics {
	template<MemoryOrder Order = MemoryOrder::SEQ_CST, AtomicWord T> requires LoadOrder<Order>
	[[nodiscard]] CORIUM_FORCEINLINE T load(const T* p_Memory) noexcept {
		return std::bit_cast<T>(detail::rawLoad<Order>(reinterpret_cast<const detail::Raw<T>*>(p_Memory)));
	}

	template<MemoryOrder Order = MemoryOrder::SEQ_CST, AtomicWord T> requires StoreOrder<Order>
	CORIUM_FORCEINLINE void store(T* p_Memory, std::type_identity_t<T> v_Value) noexcept {
		detail::rawStore<Order>(reinterpret_cast<detail::Raw<T>*>(p_Memory), std::bit_cast<detail::Raw<T>>(v_Value));
	}

	template<MemoryOrder Order = MemoryOrder::SEQ_CST, AtomicWord T>
	[[nodiscard]] CORIUM_FORCEINLINE T exchange(T* p_Memory, std::type_identity_t<T> v_Value) noexcept {
		return std::bit_cast<T>(detail::rawExchange<Order>(reinterpret_cast<detail::Raw<T>*>(p_Memory), std::bit_cast<detail::Raw<T>>(v_Value)));
	}

	// Strong compare-exchange. On failure r_Expected is updated with the observed value.
	template<MemoryOrder Success = MemoryOrder::SEQ_CST, MemoryOrder Failure = detail::failureOrderFor(Success), AtomicWord T>
		requires CasOrders<Success, Failure>
	CORIUM_FORCEINLINE bool compareExchange(T* p_Memory, T& r_Expected, std::type_identity_t<T> v_Desired) noexcept {
		auto expected = std::bit_cast<detail::Raw<T>>(r_Expected);
		const bool ok = detail::rawCompareExchange<Success, Failure>(
			reinterpret_cast<detail::Raw<T>*>(p_Memory), expected, std::bit_cast<detail::Raw<T>>(v_Desired));
		r_Expected = std::bit_cast<T>(expected);
		return ok;
	}

	// ---- read-modify-write: all return the PREVIOUS value ----

	template<MemoryOrder Order = MemoryOrder::SEQ_CST, AtomicInteger T>
	CORIUM_FORCEINLINE T fetchAdd(T* p_Memory, std::type_identity_t<T> v_Value) noexcept {
		return static_cast<T>(detail::rawFetchAdd<Order>(reinterpret_cast<detail::Raw<T>*>(p_Memory), static_cast<detail::Raw<T>>(v_Value)));
	}

	template<MemoryOrder Order = MemoryOrder::SEQ_CST, AtomicInteger T>
	CORIUM_FORCEINLINE T fetchSub(T* p_Memory, std::type_identity_t<T> v_Value) noexcept {
		return static_cast<T>(detail::rawFetchAdd<Order>(reinterpret_cast<detail::Raw<T>*>(p_Memory), static_cast<detail::Raw<T>>(0 - static_cast<detail::Raw<T>>(v_Value))));
	}

	template<MemoryOrder Order = MemoryOrder::SEQ_CST, AtomicInteger T>
	CORIUM_FORCEINLINE T fetchAnd(T* p_Memory, std::type_identity_t<T> v_Value) noexcept {
		return static_cast<T>(detail::rawFetchAnd<Order>(reinterpret_cast<detail::Raw<T>*>(p_Memory), static_cast<detail::Raw<T>>(v_Value)));
	}

	template<MemoryOrder Order = MemoryOrder::SEQ_CST, AtomicInteger T>
	CORIUM_FORCEINLINE T fetchOr(T* p_Memory, std::type_identity_t<T> v_Value) noexcept {
		return static_cast<T>(detail::rawFetchOr<Order>(reinterpret_cast<detail::Raw<T>*>(p_Memory), static_cast<detail::Raw<T>>(v_Value)));
	}

	template<MemoryOrder Order = MemoryOrder::SEQ_CST, AtomicInteger T>
	CORIUM_FORCEINLINE T fetchXor(T* p_Memory, std::type_identity_t<T> v_Value) noexcept {
		return static_cast<T>(detail::rawFetchXor<Order>(reinterpret_cast<detail::Raw<T>*>(p_Memory), static_cast<detail::Raw<T>>(v_Value)));
	}

	template<MemoryOrder Order = MemoryOrder::SEQ_CST, AtomicInteger T>
	CORIUM_FORCEINLINE T fetchNand(T* p_Memory, std::type_identity_t<T> v_Value) noexcept {
		return static_cast<T>(detail::rawFetchNand<Order>(reinterpret_cast<detail::Raw<T>*>(p_Memory), static_cast<detail::Raw<T>>(v_Value)));
	}

	// Min/max compare as T (signedness matters), so they run as a CAS loop on the typed value.
	template<MemoryOrder Order = MemoryOrder::SEQ_CST, AtomicInteger T>
	CORIUM_FORCEINLINE T fetchMax(T* p_Memory, std::type_identity_t<T> v_Value) noexcept {
		T cur = load<MemoryOrder::RELAXED>(p_Memory);
		while (cur < v_Value) {
			if (compareExchange<Order, MemoryOrder::RELAXED>(p_Memory, cur, v_Value)) break;
		}
		return cur;
	}

	template<MemoryOrder Order = MemoryOrder::SEQ_CST, AtomicInteger T>
	CORIUM_FORCEINLINE T fetchMin(T* p_Memory, std::type_identity_t<T> v_Value) noexcept {
		T cur = load<MemoryOrder::RELAXED>(p_Memory);
		while (v_Value < cur) {
			if (compareExchange<Order, MemoryOrder::RELAXED>(p_Memory, cur, v_Value)) break;
		}
		return cur;
	}

	// ---- bit helpers: return whether the bit was set BEFORE the operation ----

	template<MemoryOrder Order = MemoryOrder::SEQ_CST, AtomicInteger T>
	CORIUM_FORCEINLINE bool bitTestAndSet(T* p_Memory, unsigned v_Bit) noexcept {
		const T mask = static_cast<T>(T(1) << v_Bit);
		return (fetchOr<Order>(p_Memory, mask) & mask) != 0;
	}

	template<MemoryOrder Order = MemoryOrder::SEQ_CST, AtomicInteger T>
	CORIUM_FORCEINLINE bool bitTestAndReset(T* p_Memory, unsigned v_Bit) noexcept {
		const T mask = static_cast<T>(T(1) << v_Bit);
		return (fetchAnd<Order>(p_Memory, static_cast<T>(~mask)) & mask) != 0;
	}

	// ---- increment / decrement return the NEW value, matching _InterlockedIncrement / __atomic_add_fetch ----

	template<MemoryOrder Order = MemoryOrder::SEQ_CST, AtomicInteger T>
	CORIUM_FORCEINLINE T increment(T* p_Memory) noexcept { return static_cast<T>(fetchAdd<Order>(p_Memory, T(1)) + T(1)); }

	template<MemoryOrder Order = MemoryOrder::SEQ_CST, AtomicInteger T>
	CORIUM_FORCEINLINE T decrement(T* p_Memory) noexcept { return static_cast<T>(fetchSub<Order>(p_Memory, T(1)) - T(1)); }

	// ---- fences ----

	template<MemoryOrder Order = MemoryOrder::SEQ_CST>
	CORIUM_FORCEINLINE void fence() noexcept { detail::fence<Order>(); }

	CORIUM_FORCEINLINE void compilerFence() noexcept { detail::compilerFence(); }
	CORIUM_FORCEINLINE void pause() noexcept { detail::pause(); }
}
