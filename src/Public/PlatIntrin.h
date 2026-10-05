/*
* Copyright (c) 2025 StormWeaver
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
#include <intrin.h>
#include "Corium.h"
#include "CoriumCompiler.h"

// This header has been changed regardless.

namespace Corium::Intrinsic {
	CORIUM_FORCEINLINE CORIUM_RUNTIME_API  void Pause() {
		_mm_pause(); // Hardware pause
	}

	// Section containing hardware fences.

	// FullFence
	//
	// Establishes a global hardware ordering point.
	//
	// Guarantees that all loads and stores issued before this point become
	// visible before any loads or stores issued after it.
	//
	// This is a heavyweight synchronization primitive intended only for
	// global phase transitions, device boundaries, or shutdown paths.

	CORIUM_FORCEINLINE CORIUM_RUNTIME_API void FullFence() {
#if CORIUM_COMPILER_MSVC
		::_mm_mfence();
#elif CORIUM_COMPILER_GCC
#if defined(__has_builtin)
#if __has_builtin(__atomic_thread_fence)
		__atomic_thread_fence(__ATOMIC_SEQ_CST);
#else
		CORIUM_STATIC_ASSERT(__has_builtin(__atomic_thread_fence), "__atomic_thread_fence not supported by this compiler");
#endif
#else
		CORIUM_STATIC_ASSERT(false, "__has_builtin not available");
#endif
#else

		CORIUM_STATIC_ASSERT(false, "Unsupported full fence in this environment");
		CORIUM_UNREACHABLE();
#endif
	}

	// LoadFence
	//
	// Establishes ordering for load operations.
	//
	// Prevents later loads from being observed before earlier loads.
	// Does not impose ordering on stores.
	//
	// Intended for explicit consumption of published data.

	CORIUM_FORCEINLINE	CORIUM_RUNTIME_API void LoadFence() {
#if CORIUM_COMPILER_MSVC
		::_mm_lfence();
#elif CORIUM_COMPILER_GCC
#if defined(__has_builtin)
#if __has_builtin(__atomic_thread_fence)
		__atomic_thread_fence(__ATOMIC_ACQUIRE);
#else
		CORIUM_STATIC_ASSERT(__has_builtin(__atomic_thread_fence), "__atomic_thread_fence not supported by this compiler");
#endif
#else
		CORIUM_STATIC_ASSERT(false, "__has_builtin not available");
#endif
#else
		CORIUM_STATIC_ASSERT(false, "Unsupported load fence in this environment");
		CORIUM_UNREACHABLE();
#endif
	}

	// StoreFence
	//
	// Establishes ordering for store operations.
	//
	// Ensures that all prior stores are committed before subsequent stores.
	// Does not impose ordering on loads.
	//
	// Commonly used when publishing data followed by a visibility flag.

	CORIUM_FORCEINLINE CORIUM_RUNTIME_API void StoreFence() {
#if CORIUM_COMPILER_MSVC
		::_mm_sfence();
#elif  CORIUM_COMPILER_GCC
#if defined(__has_builtin)
#if __has_builtin(__atomic_thread_fence)
		__atomic_thread_fence(__ATOMIC_RELEASE);
#else
		CORIUM_STATIC_ASSERT(__has_builtin(__atomic_thread_fence), "__atomic_thread_fence not supported by this compiler");
#endif
#else
		CORIUM_STATIC_ASSERT(false, "__has_builtin not available");
#endif
#else
		CORIUM_STATIC_ASSERT(false, "Unsupported store fence in this environment");
		CORIUM_UNREACHABLE();
#endif
	}

	// (Why is this even necessary?) Compiler hint fences

	// RWCompileBarrier
	//
	// Compiler-only barrier for both loads and stores.
	//
	// Prevents the compiler from reordering memory operations across
	// this point, without emitting any CPU instructions.
	//
	// Does NOT provide inter-thread synchronization.

	CORIUM_FORCEINLINE CORIUM_RUNTIME_API void RWCompileBarrier() {
#if CORIUM_COMPILER_MSVC
		_ReadWriteBarrier();
#elif CORIUM_COMPILER_GCC
#if defined(__has_builtin)
#if __has_builtin(__atomic_signal_fence)
		__atomic_signal_fence(__ATOMIC_SEQ_CST);
# else
		CORIUM_STATIC_ASSERT(__has_builtin(__atomic_signal_fence), "__atomic_signal_fence not supported by this compiler");
#endif
#else
		CORIUM_STATIC_ASSERT(false, "__has_builtin not available");
#endif
#else
		CORIUM_STATIC_ASSERT(false, "Unsupported read write barrier in this environment");
		CORIUM_UNREACHABLE();
#endif
	}

	// RCompileBarrier
	//
	// Compiler-only barrier for load operations.
	//
	// Prevents reordering of reads across this point while allowing
	// stores to move freely.
	//
	// Intended for rare, read-only ordering constraints.

	CORIUM_FORCEINLINE CORIUM_RUNTIME_API void RCompileBarrier() {
#if CORIUM_COMPILER_MSVC
		_ReadBarrier();
#elif CORIUM_COMPILER_GCC
#if defined(__has_builtin)
#if __has_builtin(__atomic_signal_fence)
		__atomic_signal_fence(__ATOMIC_ACQUIRE);
#else
		CORIUM_STATIC_ASSERT(__has_builtin(__atomic_signal_fence), "__atomic_signal_fence not supported by this compiler");
#endif
#else
		CORIUM_STATIC_ASSERT(false, "__has_builtin not available");
#endif
#else
		CORIUM_STATIC_ASSERT(false, "Unsupported read barrier in this environment");
		CORIUM_UNREACHABLE();
#endif
	}

	// WCompileBarrier
	//
	// Compiler-only barrier for store operations.
	//
	// Prevents reordering of writes across this point while allowing
	// loads to move freely.
	//
	// Commonly used before publishing shared state.

	CORIUM_FORCEINLINE CORIUM_RUNTIME_API void WCompileBarrier() {
#if CORIUM_COMPILER_MSVC
		_WriteBarrier();
#elif CORIUM_COMPILER_GCC
#if defined(__has_builtin)
#if __has_builtin(__atomic_signal_fence)
		__atomic_signal_fence(__ATOMIC_RELEASE);
#else
		CORIUM_STATIC_ASSERT(false, "__atomic_signal_fence not supported by this compiler");
#endif
#else
		CORIUM_STATIC_ASSERT(false, "__has_builtin not available");
#endif
#else
		CORIUM_STATIC_ASSERT(false, "Unsupported write barrier in this environment");
		CORIUM_UNREACHABLE();
#endif
	}

	template<typename T>
	struct ValidAtomicParameter final {
		using Type = T;
		CORIUM_STATIC_ASSERT(
			std::is_trivially_copyable_v<Type>,
			"Unsupported atomic nature: type must be trivially copyable."
		);

		CORIUM_STATIC_ASSERT(
			!std::is_const_v<Type>,
			"Unsupported atomic nature: atomic type must not be const-qualified."
		);

		CORIUM_STATIC_ASSERT(
			!std::is_volatile_v<Type>,
			"Unsupported atomic nature: atomic type must not be volatile-qualified."
		);
	};
}
