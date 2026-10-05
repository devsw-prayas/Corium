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

// GCC/Clang backend. Every primitive is a one-line forward to the matching __atomic builtin at the raw width.

#include "AtomicsCore.h"
#include "CoriumCompiler.h"

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

namespace Corium::Atomics::detail {
	constexpr int gnuOrder(MemoryOrder v_Order) noexcept {
		switch (v_Order) {
		case MemoryOrder::RELAXED: return __ATOMIC_RELAXED;
		case MemoryOrder::CONSUME: return __ATOMIC_CONSUME;
		case MemoryOrder::ACQUIRE: return __ATOMIC_ACQUIRE;
		case MemoryOrder::RELEASE: return __ATOMIC_RELEASE;
		case MemoryOrder::ACQ_REL: return __ATOMIC_ACQ_REL;
		case MemoryOrder::SEQ_CST: return __ATOMIC_SEQ_CST;
		}
		return __ATOMIC_SEQ_CST;
	}

	template<MemoryOrder Order, typename U>
	CORIUM_FORCEINLINE U rawLoad(const U* p_Memory) noexcept { return __atomic_load_n(p_Memory, gnuOrder(Order)); }

	template<MemoryOrder Order, typename U>
	CORIUM_FORCEINLINE void rawStore(U* p_Memory, U v_Value) noexcept { __atomic_store_n(p_Memory, v_Value, gnuOrder(Order)); }

	template<MemoryOrder Order, typename U>
	CORIUM_FORCEINLINE U rawExchange(U* p_Memory, U v_Value) noexcept { return __atomic_exchange_n(p_Memory, v_Value, gnuOrder(Order)); }

	template<MemoryOrder Success, MemoryOrder Failure, typename U>
	CORIUM_FORCEINLINE bool rawCompareExchange(U* p_Memory, U& r_Expected, U v_Desired) noexcept {
		return __atomic_compare_exchange_n(p_Memory, &r_Expected, v_Desired, false, gnuOrder(Success), gnuOrder(Failure));
	}

	template<MemoryOrder Order, typename U>
	CORIUM_FORCEINLINE U rawFetchAdd(U* p_Memory, U v_Value) noexcept { return __atomic_fetch_add(p_Memory, v_Value, gnuOrder(Order)); }

	template<MemoryOrder Order, typename U>
	CORIUM_FORCEINLINE U rawFetchAnd(U* p_Memory, U v_Value) noexcept { return __atomic_fetch_and(p_Memory, v_Value, gnuOrder(Order)); }

	template<MemoryOrder Order, typename U>
	CORIUM_FORCEINLINE U rawFetchOr(U* p_Memory, U v_Value) noexcept { return __atomic_fetch_or(p_Memory, v_Value, gnuOrder(Order)); }

	template<MemoryOrder Order, typename U>
	CORIUM_FORCEINLINE U rawFetchXor(U* p_Memory, U v_Value) noexcept { return __atomic_fetch_xor(p_Memory, v_Value, gnuOrder(Order)); }

	template<MemoryOrder Order, typename U>
	CORIUM_FORCEINLINE U rawFetchNand(U* p_Memory, U v_Value) noexcept { return __atomic_fetch_nand(p_Memory, v_Value, gnuOrder(Order)); }

	template<MemoryOrder Order>
	CORIUM_FORCEINLINE void fence() noexcept { if constexpr (Order != MemoryOrder::RELAXED) __atomic_thread_fence(gnuOrder(Order)); }

	CORIUM_FORCEINLINE void compilerFence() noexcept { __atomic_signal_fence(__ATOMIC_SEQ_CST); }

	CORIUM_FORCEINLINE void pause() noexcept {
#if defined(__x86_64__) || defined(__i386__)
		_mm_pause();
#endif
	}
}
