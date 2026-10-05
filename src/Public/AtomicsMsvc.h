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

// MSVC backend. Primitives operate on the unsigned raw type of the right width and never widen.
// x64 is TSO: every locked RMW is already a full barrier, plain loads are acquire, plain stores are release.
// Only the compiler needs holding back for the weaker orders, and seq_cst stores need the xchg.

#include "AtomicsCore.h"
#include "CoriumCompiler.h"
#include <intrin.h>

#if !defined(_M_X64)
#error "AtomicsMsvc.h is x64 only; an ARM64 backend needs real barriers in load/store."
#endif

namespace Corium::Atomics::detail {
	template<typename U>
	using MsvcInt = std::conditional_t<sizeof(U) == 1, char,
		std::conditional_t<sizeof(U) == 2, short,
		std::conditional_t<sizeof(U) == 4, long, __int64>>>;

	template<typename U>
	CORIUM_FORCEINLINE volatile MsvcInt<U>* msvcPtr(U* p_Memory) noexcept {
		return reinterpret_cast<volatile MsvcInt<U>*>(p_Memory);
	}

	template<MemoryOrder Order, typename U>
	CORIUM_FORCEINLINE U rawLoad(const U* p_Memory) noexcept {
		U v;
		if constexpr (sizeof(U) == 1) v = static_cast<U>(__iso_volatile_load8(reinterpret_cast<const volatile char*>(p_Memory)));
		else if constexpr (sizeof(U) == 2) v = static_cast<U>(__iso_volatile_load16(reinterpret_cast<const volatile short*>(p_Memory)));
		else if constexpr (sizeof(U) == 4) v = static_cast<U>(__iso_volatile_load32(reinterpret_cast<const volatile int*>(p_Memory)));
		else v = static_cast<U>(__iso_volatile_load64(reinterpret_cast<const volatile __int64*>(p_Memory)));
		if constexpr (Order != MemoryOrder::RELAXED) _ReadWriteBarrier();
		return v;
	}

	template<MemoryOrder Order, typename U>
	CORIUM_FORCEINLINE U rawExchange(U* p_Memory, U v_Value) noexcept {
		if constexpr (sizeof(U) == 1) return static_cast<U>(_InterlockedExchange8(msvcPtr(p_Memory), static_cast<char>(v_Value)));
		else if constexpr (sizeof(U) == 2) return static_cast<U>(_InterlockedExchange16(msvcPtr(p_Memory), static_cast<short>(v_Value)));
		else if constexpr (sizeof(U) == 4) return static_cast<U>(_InterlockedExchange(msvcPtr(p_Memory), static_cast<long>(v_Value)));
		else return static_cast<U>(_InterlockedExchange64(msvcPtr(p_Memory), static_cast<__int64>(v_Value)));
	}

	template<MemoryOrder Order, typename U>
	CORIUM_FORCEINLINE void rawStore(U* p_Memory, U v_Value) noexcept {
		if constexpr (Order == MemoryOrder::SEQ_CST) {
			(void)rawExchange<Order>(p_Memory, v_Value);
		} else {
			if constexpr (Order == MemoryOrder::RELEASE) _ReadWriteBarrier();
			if constexpr (sizeof(U) == 1) __iso_volatile_store8(reinterpret_cast<volatile char*>(p_Memory), static_cast<char>(v_Value));
			else if constexpr (sizeof(U) == 2) __iso_volatile_store16(reinterpret_cast<volatile short*>(p_Memory), static_cast<short>(v_Value));
			else if constexpr (sizeof(U) == 4) __iso_volatile_store32(reinterpret_cast<volatile int*>(p_Memory), static_cast<int>(v_Value));
			else __iso_volatile_store64(reinterpret_cast<volatile __int64*>(p_Memory), static_cast<__int64>(v_Value));
		}
	}

	// Strong CAS. On failure r_Expected receives the observed value.
	template<MemoryOrder Success, MemoryOrder Failure, typename U>
	CORIUM_FORCEINLINE bool rawCompareExchange(U* p_Memory, U& r_Expected, U v_Desired) noexcept {
		U prev;
		if constexpr (sizeof(U) == 1) prev = static_cast<U>(_InterlockedCompareExchange8(msvcPtr(p_Memory), static_cast<char>(v_Desired), static_cast<char>(r_Expected)));
		else if constexpr (sizeof(U) == 2) prev = static_cast<U>(_InterlockedCompareExchange16(msvcPtr(p_Memory), static_cast<short>(v_Desired), static_cast<short>(r_Expected)));
		else if constexpr (sizeof(U) == 4) prev = static_cast<U>(_InterlockedCompareExchange(msvcPtr(p_Memory), static_cast<long>(v_Desired), static_cast<long>(r_Expected)));
		else prev = static_cast<U>(_InterlockedCompareExchange64(msvcPtr(p_Memory), static_cast<__int64>(v_Desired), static_cast<__int64>(r_Expected)));
		const bool ok = prev == r_Expected;
		r_Expected = prev;
		return ok;
	}

	template<MemoryOrder Order, typename U>
	CORIUM_FORCEINLINE U rawFetchAdd(U* p_Memory, U v_Value) noexcept {
		if constexpr (sizeof(U) == 1) return static_cast<U>(_InterlockedExchangeAdd8(msvcPtr(p_Memory), static_cast<char>(v_Value)));
		else if constexpr (sizeof(U) == 2) return static_cast<U>(_InterlockedExchangeAdd16(msvcPtr(p_Memory), static_cast<short>(v_Value)));
		else if constexpr (sizeof(U) == 4) return static_cast<U>(_InterlockedExchangeAdd(msvcPtr(p_Memory), static_cast<long>(v_Value)));
		else return static_cast<U>(_InterlockedExchangeAdd64(msvcPtr(p_Memory), static_cast<__int64>(v_Value)));
	}

	template<MemoryOrder Order, typename U>
	CORIUM_FORCEINLINE U rawFetchAnd(U* p_Memory, U v_Value) noexcept {
		if constexpr (sizeof(U) == 1) return static_cast<U>(_InterlockedAnd8(msvcPtr(p_Memory), static_cast<char>(v_Value)));
		else if constexpr (sizeof(U) == 2) return static_cast<U>(_InterlockedAnd16(msvcPtr(p_Memory), static_cast<short>(v_Value)));
		else if constexpr (sizeof(U) == 4) return static_cast<U>(_InterlockedAnd(msvcPtr(p_Memory), static_cast<long>(v_Value)));
		else return static_cast<U>(_InterlockedAnd64(msvcPtr(p_Memory), static_cast<__int64>(v_Value)));
	}

	template<MemoryOrder Order, typename U>
	CORIUM_FORCEINLINE U rawFetchOr(U* p_Memory, U v_Value) noexcept {
		if constexpr (sizeof(U) == 1) return static_cast<U>(_InterlockedOr8(msvcPtr(p_Memory), static_cast<char>(v_Value)));
		else if constexpr (sizeof(U) == 2) return static_cast<U>(_InterlockedOr16(msvcPtr(p_Memory), static_cast<short>(v_Value)));
		else if constexpr (sizeof(U) == 4) return static_cast<U>(_InterlockedOr(msvcPtr(p_Memory), static_cast<long>(v_Value)));
		else return static_cast<U>(_InterlockedOr64(msvcPtr(p_Memory), static_cast<__int64>(v_Value)));
	}

	template<MemoryOrder Order, typename U>
	CORIUM_FORCEINLINE U rawFetchXor(U* p_Memory, U v_Value) noexcept {
		if constexpr (sizeof(U) == 1) return static_cast<U>(_InterlockedXor8(msvcPtr(p_Memory), static_cast<char>(v_Value)));
		else if constexpr (sizeof(U) == 2) return static_cast<U>(_InterlockedXor16(msvcPtr(p_Memory), static_cast<short>(v_Value)));
		else if constexpr (sizeof(U) == 4) return static_cast<U>(_InterlockedXor(msvcPtr(p_Memory), static_cast<long>(v_Value)));
		else return static_cast<U>(_InterlockedXor64(msvcPtr(p_Memory), static_cast<__int64>(v_Value)));
	}

	// No native NAND on x86: CAS loop at the real width.
	template<MemoryOrder Order, typename U>
	CORIUM_FORCEINLINE U rawFetchNand(U* p_Memory, U v_Value) noexcept {
		U cur = rawLoad<MemoryOrder::RELAXED>(p_Memory);
		while (!rawCompareExchange<Order, MemoryOrder::RELAXED>(p_Memory, cur, static_cast<U>(~(cur & v_Value)))) {}
		return cur;
	}

	template<MemoryOrder Order>
	CORIUM_FORCEINLINE void fence() noexcept {
		if constexpr (Order == MemoryOrder::SEQ_CST) _mm_mfence();
		else if constexpr (Order != MemoryOrder::RELAXED) _ReadWriteBarrier();
	}

	CORIUM_FORCEINLINE void compilerFence() noexcept { _ReadWriteBarrier(); }
	CORIUM_FORCEINLINE void pause() noexcept { _mm_pause(); }
}
