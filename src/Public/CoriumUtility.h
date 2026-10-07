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
#include <Corium.h>
#include "CoriumAllocator.h"
#include "CoriumMemoryHandler.h"
#include "EngineAllocators.h"

namespace Corium::Core::Utils {
	CORIUM_FORCEINLINE constexpr uint64_t fibonacciHash(uint64_t v_Key, uint32_t v_ShiftBits = 0) noexcept {
		return (v_Key >> v_ShiftBits) * 0x9e3779b97f4a7c15ULL;
	}

	CORIUM_FORCEINLINE constexpr uint64_t murmurHash(uint64_t v_Key) noexcept {
		v_Key ^= v_Key >> 33;
		v_Key *= 0xff51afd7ed558ccdULL;
		v_Key ^= v_Key >> 33;
		v_Key *= 0xc4ceb9fe1a85ec53ULL;
		v_Key ^= v_Key >> 33;
		return v_Key;
	}

	template<typename R, typename... Args>
	struct Trampoline {
		template<typename L>
		static R invoke(void* ctx, Args... args) {
			return (*static_cast<L*>(ctx))(std::forward<Args>(args)...);
		}
	};

	template<typename A, typename S>
	struct ClosureFunction;

	template<typename A, typename R, typename... Args>
	struct ClosureFunction<A, R(Args...)> {
		using AllocatorType = A;
		using EntryType = R(*)(void*, Args...);
		using DeleterType = void(*)(void*, AllocatorType*);

		void* m_Context = nullptr;
		EntryType m_Entry = nullptr;

		AllocatorType* m_Allocator = nullptr;
		DeleterType m_Deleter = nullptr;

		ClosureFunction() = default;
		ClosureFunction(const ClosureFunction&) = delete;
		ClosureFunction& operator=(const ClosureFunction&) = delete;

		ClosureFunction(ClosureFunction&& u_Other) noexcept
			: m_Context(u_Other.m_Context),
			m_Entry(u_Other.m_Entry),
			m_Allocator(u_Other.m_Allocator),
			m_Deleter(u_Other.m_Deleter) {
			u_Other.m_Context = nullptr;
			u_Other.m_Entry = nullptr;
			u_Other.m_Allocator = nullptr;
			u_Other.m_Deleter = nullptr;
		}

		ClosureFunction& operator=(ClosureFunction&& u_Other) noexcept {
			if (this != &u_Other) {
				// Destroy current state (if any)
				if (m_Context && m_Deleter) {
					m_Deleter(m_Context, m_Allocator);
				}

				// Transfer ownership
				m_Context = u_Other.m_Context;
				m_Entry = u_Other.m_Entry;
				m_Allocator = u_Other.m_Allocator;
				m_Deleter = u_Other.m_Deleter;

				// Null out source
				u_Other.m_Context = nullptr;
				u_Other.m_Entry = nullptr;
				u_Other.m_Allocator = nullptr;
				u_Other.m_Deleter = nullptr;
			}
			return *this;
		}

		template<typename L>
			requires (!std::is_same_v<std::remove_cvref_t<L>, ClosureFunction>)
		explicit ClosureFunction(L&& lambda, AllocatorType* allocator) {
			if (!allocator) return;
			using LambdaT = std::decay_t<L>;

			void* memory = allocator->allocate(sizeof(LambdaT), alignof(LambdaT));
			LambdaT* stored = memory
				? allocator->template emplace<LambdaT>(memory, std::forward<L>(lambda))
				: nullptr;

			// Allocation failed: stay fully empty, so isCallable() is false and operator() asserts instead of jumping with a null context.
			if (!stored) return;

			m_Context = stored;
			m_Allocator = allocator;

			m_Entry = &Trampoline<R, Args...>::template invoke<LambdaT>;

			m_Deleter = [](void* ctx, AllocatorType* alloc) {
				auto* obj = static_cast<LambdaT*>(ctx);

				obj->~LambdaT();

				// Always safe: raw allocators over a pure-bump arena have a real
				// (no-op) deallocate body, so this was only ever skipping a call
				// that would do nothing anyway. Mechanism is no longer something
				// callers branch on - see AllocatorMetadata in CoriumAllocator.h.
				alloc->deallocate(obj, sizeof(LambdaT));
				};
		}

		~ClosureFunction() {
			if (m_Context && m_Deleter) {
				m_Deleter(m_Context, m_Allocator);
			}
		}

		CORIUM_NODISCARD R operator()(Args... args) const {
			CORIUM_ASSERT(m_Entry != nullptr);
			return m_Entry(m_Context, std::forward<Args>(args)...);
		}

		bool isCallable() const noexcept {
			return m_Context != nullptr && m_Entry != nullptr;
		}
	};

	template<typename>
	struct FunctionView;

	template<typename R, typename... Args>
	struct FunctionView<R(Args...)> {
		using Entry = R(*)(void*, Args...);

		void* m_Context = nullptr;
		Entry m_Entry = nullptr;

		template<typename L>
		FunctionView(L& lambda) noexcept {
			using LambdaT = std::remove_reference_t<L>;

			m_Context = &lambda;
			m_Entry = &Trampoline<R, Args...>::template invoke<LambdaT>;
		}

		// also allow view from ClosureFunction
		template<typename A>
		explicit FunctionView(const ClosureFunction<A, R(Args...)>& fn) {
			m_Context = fn.m_Context;
			m_Entry = fn.m_Entry;
		}

		// Default ctor
		FunctionView() = default;

		CORIUM_NODISCARD R operator()(Args... args) const {
			CORIUM_ASSERT(m_Entry != nullptr);
			return m_Entry(m_Context, std::forward<Args>(args)...);
		}
	};

	// Owning closure on a NUMA node's closure allocator; an out-of-range node falls back to 0.
	template<typename S, typename L>
	auto makeClosure(L&& u_Lambda, uint8_t node = 0) {
		using Alloc = Corium::Memory::Allocators::ClosureAllocator;

		const uint8_t safeNode = (node < Memory::Internal::AllocatorRegistry::s_NodeCount)
			? node
			: 0;

		auto* alloc = Corium::Memory::Internal::AtomicAllocators
			::instance()
			.s_ClosureAllocator[safeNode]
			.load();

		return Corium::Core::Utils::ClosureFunction<Alloc, S>(
			std::forward<L>(u_Lambda),
			alloc
		);
	}
}
