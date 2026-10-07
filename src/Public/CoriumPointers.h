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
#include "AtomicVar.h"
#include "EngineAllocators.h"
#include "CoriumAtomics.h"

namespace Corium::Memory {
	using namespace Corium::Core::Atomics;

	// UniqueView - non-owning observer vended by UniquePtr

	template<typename T>
	struct alignas(16) UniqueView final {
	private:
		T* m_Ptr = nullptr;

		template<typename U, typename UAllocator>
		friend struct UniquePtr;

		explicit UniqueView(T* p_Ptr) noexcept : m_Ptr(p_Ptr) {}

	public:
		UniqueView() noexcept = default;

		T* operator->() const noexcept { return m_Ptr; }
		T& operator*()  const noexcept { return *m_Ptr; }
		T* get()        const noexcept { return m_Ptr; }

		explicit operator bool() const noexcept { return m_Ptr != nullptr; }

		operator UniqueView<const T>() const noexcept {
			return UniqueView<const T>(m_Ptr);
		}
	};

	// UniquePtr<T, TAllocator> - single-owner smart pointer.
	//
	// TAllocator defaults to GeneralAllocator but can be any allocator that
	// exposes emplace<T>(...) and deallocate(void*, size_t).
	// The allocator instance is passed at construction and stored alongside
	// the pointer so destroy() can return memory without a separate free list.

	template<typename T, typename TAllocator = Allocators::GeneralAllocator>
	struct alignas(16) UniquePtr final {
	private:
		TAllocator* m_Allocator = nullptr;
		T* m_Memory = nullptr;

		template<typename... Args>
		void alloc(Args&&... u_Params) {
			CORIUM_ASSERT(m_Allocator != nullptr);
			void* memory = m_Allocator->allocate(sizeof(T), alignof(T));
			if (memory) {
				m_Memory = m_Allocator->template emplace<T>(memory, std::forward<Args>(u_Params)...);
			} else {
				m_Memory = nullptr;
				CORIUM_ASSERT(false && "Allocation failed in UniquePtr");
			}
		}

		void destroy() noexcept {
			if (m_Memory) {
				CORIUM_ASSERT(m_Allocator != nullptr);
				m_Memory->~T();
				m_Allocator->deallocate(m_Memory, sizeof(T));
				m_Memory = nullptr;
			}
		}

	public:
		UniquePtr() noexcept = default;

		template<typename... Args>
		explicit UniquePtr(TAllocator* p_Allocator, Args&&... u_Params) noexcept {
			m_Allocator = p_Allocator;
			alloc(std::forward<Args>(u_Params)...);
		}

		UniqueView<T>       view() noexcept { return UniqueView<T>(m_Memory); }
		UniqueView<const T> view() const noexcept { return UniqueView<const T>(m_Memory); }

		UniquePtr(const UniquePtr&) = delete;
		UniquePtr& operator=(const UniquePtr&) = delete;

		UniquePtr(UniquePtr&& u_Other) noexcept
			: m_Allocator(u_Other.m_Allocator), m_Memory(u_Other.m_Memory) {
			u_Other.m_Allocator = nullptr;
			u_Other.m_Memory = nullptr;
		}

		UniquePtr& operator=(UniquePtr&& u_Other) noexcept {
			if (this == &u_Other) return *this;
			destroy();
			m_Allocator = u_Other.m_Allocator;
			m_Memory = u_Other.m_Memory;
			u_Other.m_Allocator = nullptr;
			u_Other.m_Memory = nullptr;
			return *this;
		}

		~UniquePtr() {
			destroy();
			m_Allocator = nullptr;
		}

		T* operator->() const noexcept { return m_Memory; }
		T& operator*()  const noexcept { return *m_Memory; }
		T* get()        const noexcept { return m_Memory; }

		explicit operator bool() const noexcept { return m_Memory != nullptr; }

		T* release() noexcept {
			T* result = m_Memory;
			m_Memory = nullptr;
			m_Allocator = nullptr;
			return result;
		}

		void reset() noexcept {
			destroy();
			m_Allocator = nullptr;
		}

		template<typename... Args>
		void reset(TAllocator* p_Allocator, Args&&... u_Params) {
			destroy();
			m_Allocator = p_Allocator;
			alloc(std::forward<Args>(u_Params)...);
		}
	};

	// Internal - SharedControlBlock
	//
	// Bump-allocated from ControlBlockAllocator (g_SmartPtrControlBlocks VA).
	// Never individually freed - lives for the process lifetime.
	// ControlBlockAllocator always stays fixed regardless of object allocator.

	namespace Internal {
		struct alignas(64) SharedControlBlock final {
			Core::Atomic::AtomicValue64<uint64_t> m_StrongCount;

		private:
			CORIUM_MAYBE_UNUSED const std::byte padding_[56] = {};

		public:
			explicit SharedControlBlock(uint64_t v_InitCount) noexcept
				: m_StrongCount(v_InitCount) {
			}

			SharedControlBlock(const SharedControlBlock&) = delete;
			SharedControlBlock& operator=(const SharedControlBlock&) = delete;
			SharedControlBlock(SharedControlBlock&&) = delete;
			SharedControlBlock& operator=(SharedControlBlock&&) = delete;

			void addRef() noexcept {
				CORIUM_UNUSED(m_StrongCount.increment(MemoryOrder::RELAXED));
			}

			bool release() noexcept {
				uint64_t newCount = m_StrongCount.decrement(MemoryOrder::ACQ_REL);
				return newCount == 0u;
			}

			bool tryAddRef() noexcept {
				uint64_t cur = m_StrongCount.load(MemoryOrder::ACQUIRE);
				while (cur != 0u) {
					const uint64_t observed = m_StrongCount.compareExchange(
						&cur, cur + 1u,
						MemoryOrder::ACQ_REL,
						MemoryOrder::ACQUIRE);
					if (observed == cur) return true;
					cur = observed;
				}
				return false;
			}

			uint32_t useCount() const noexcept {
				return static_cast<uint32_t>(m_StrongCount.load(MemoryOrder::ACQUIRE));
			}
		};

		CORIUM_STATIC_ASSERT(sizeof(SharedControlBlock) <= 64,
							 "SharedControlBlock must fit in one cache line");
	} // namespace Internal

	// Forward declarations

	template<typename T, typename TAllocator = Allocators::GeneralAllocator> struct SharedPtr;
	template<typename T, typename TAllocator = Allocators::GeneralAllocator> struct WeakPtr;

	// SharedPtr<T, TAllocator> - shared-ownership smart pointer.
	//
	// TAllocator defaults to GeneralAllocator but can be any allocator that
	// exposes emplace<T>(...) and deallocate(void*, size_t).
	// ControlBlockAllocator always stays fixed - control blocks always go to
	// g_SmartPtrControlBlocks regardless of object allocator.
	//
	// Construction:
	//   SharedPtr<T, TAllocator>::adopt(p_Object, p_ObjAlloc, p_CtrlAlloc)
	//   SharedPtr<T, TAllocator>::make(p_ObjAlloc, p_CtrlAlloc, args...)

	template<typename T, typename TAllocator>
	struct alignas(16) SharedPtr final {
	private:
		TAllocator* m_Allocator = nullptr;
		T* m_Ptr = nullptr;
		Internal::SharedControlBlock* m_Control = nullptr;

		SharedPtr(T* p_Ptr,
				  Internal::SharedControlBlock* p_Control,
				  TAllocator* p_Allocator) noexcept
			: m_Allocator(p_Allocator), m_Ptr(p_Ptr), m_Control(p_Control) {
		}

		void releaseRef() noexcept {
			if (m_Control && m_Control->release()) {
				CORIUM_ASSERT(m_Allocator != nullptr);
				m_Ptr->~T();
				m_Allocator->deallocate(m_Ptr, sizeof(T));
			}
			m_Allocator = nullptr;
			m_Ptr = nullptr;
			m_Control = nullptr;
		}

		template<typename U, typename UAllocator> friend struct SharedPtr;
		template<typename U, typename UAllocator> friend struct WeakPtr;

	public:
		SharedPtr() noexcept = default;

		static SharedPtr adopt(T* p_Object,
							   TAllocator* p_ObjAlloc,
							   Allocators::ControlBlockAllocator* p_CtrlAlloc) noexcept {
			CORIUM_ASSERT(p_Object && "Cannot adopt null object");
			CORIUM_ASSERT(p_ObjAlloc && "Allocator is null");
			CORIUM_ASSERT(p_CtrlAlloc && "ControlBlockAllocator is null");

			void* controlMemory = p_CtrlAlloc->allocate(sizeof(Internal::SharedControlBlock), alignof(Internal::SharedControlBlock));
			auto* control = controlMemory
				? p_CtrlAlloc->emplace<Internal::SharedControlBlock>(controlMemory, 1u)
				: nullptr;
			CORIUM_ASSERT(control && "ControlBlockAllocator out of capacity");

			return SharedPtr(p_Object, control, p_ObjAlloc);
		}

		template<typename... Args>
		static SharedPtr make(TAllocator* p_ObjAlloc,
							  Allocators::ControlBlockAllocator* p_CtrlAlloc,
							  Args&&... u_Args) noexcept {
			CORIUM_ASSERT(p_ObjAlloc && "Allocator is null");
			CORIUM_ASSERT(p_CtrlAlloc && "ControlBlockAllocator is null");

			void* memory = p_ObjAlloc->allocate(sizeof(T), alignof(T));
			T* object = memory
				? p_ObjAlloc->template emplace<T>(memory, std::forward<Args>(u_Args)...)
				: nullptr;
			if (!object) return SharedPtr{};

			return adopt(object, p_ObjAlloc, p_CtrlAlloc);
		}

		SharedPtr(const SharedPtr& r_Other) noexcept
			: m_Allocator(r_Other.m_Allocator)
			, m_Ptr(r_Other.m_Ptr)
			, m_Control(r_Other.m_Control) {
			if (m_Control) m_Control->addRef();
		}

		SharedPtr& operator=(const SharedPtr& r_Other) noexcept {
			if (this == &r_Other) return *this;
			releaseRef();
			m_Allocator = r_Other.m_Allocator;
			m_Ptr = r_Other.m_Ptr;
			m_Control = r_Other.m_Control;
			if (m_Control) m_Control->addRef();
			return *this;
		}

		SharedPtr(SharedPtr&& u_Other) noexcept
			: m_Allocator(u_Other.m_Allocator)
			, m_Ptr(u_Other.m_Ptr)
			, m_Control(u_Other.m_Control) {
			u_Other.m_Allocator = nullptr;
			u_Other.m_Ptr = nullptr;
			u_Other.m_Control = nullptr;
		}

		SharedPtr& operator=(SharedPtr&& u_Other) noexcept {
			if (this == &u_Other) return *this;
			releaseRef();
			m_Allocator = u_Other.m_Allocator;
			m_Ptr = u_Other.m_Ptr;
			m_Control = u_Other.m_Control;
			u_Other.m_Allocator = nullptr;
			u_Other.m_Ptr = nullptr;
			u_Other.m_Control = nullptr;
			return *this;
		}

		~SharedPtr() { releaseRef(); }

		T* get()        const noexcept { return m_Ptr; }
		T& operator*()  const noexcept { CORIUM_ASSERT(m_Ptr); return *m_Ptr; }
		T* operator->() const noexcept { CORIUM_ASSERT(m_Ptr); return m_Ptr; }

		explicit operator bool() const noexcept { return m_Ptr != nullptr; }

		uint32_t useCount() const noexcept {
			return m_Control ? m_Control->useCount() : 0u;
		}

		WeakPtr<T, TAllocator> weak() const noexcept;

		void reset() noexcept { releaseRef(); }
	};

	// WeakPtr<T, TAllocator> - non-owning observer for SharedPtr-managed objects.
	//
	// Carries the allocator ptr from the originating SharedPtr so that
	// lock() can reconstruct a full SharedPtr<T> if the object is still alive.
	// No ref counting in the destructor - control block lives for process lifetime.

	template<typename T, typename TAllocator>
	struct alignas(16) WeakPtr final {
	private:
		TAllocator* m_Allocator = nullptr;
		T* m_Ptr = nullptr;
		Internal::SharedControlBlock* m_Control = nullptr;

		template<typename U, typename UAllocator> friend struct SharedPtr;
		template<typename U, typename UAllocator> friend struct WeakPtr;

	public:
		WeakPtr() noexcept = default;

		explicit WeakPtr(const SharedPtr<T, TAllocator>& r_Shared) noexcept
			: m_Allocator(r_Shared.m_Allocator)
			, m_Ptr(r_Shared.m_Ptr)
			, m_Control(r_Shared.m_Control) {
		}

		WeakPtr(const WeakPtr& r_Other) noexcept
			: m_Allocator(r_Other.m_Allocator)
			, m_Ptr(r_Other.m_Ptr)
			, m_Control(r_Other.m_Control) {
		}

		WeakPtr& operator=(const WeakPtr& r_Other) noexcept {
			if (this == &r_Other) return *this;
			m_Allocator = r_Other.m_Allocator;
			m_Ptr = r_Other.m_Ptr;
			m_Control = r_Other.m_Control;
			return *this;
		}

		WeakPtr(WeakPtr&& u_Other) noexcept
			: m_Allocator(u_Other.m_Allocator)
			, m_Ptr(u_Other.m_Ptr)
			, m_Control(u_Other.m_Control) {
			u_Other.m_Allocator = nullptr;
			u_Other.m_Ptr = nullptr;
			u_Other.m_Control = nullptr;
		}

		WeakPtr& operator=(WeakPtr&& u_Other) noexcept {
			if (this == &u_Other) return *this;
			m_Allocator = u_Other.m_Allocator;
			m_Ptr = u_Other.m_Ptr;
			m_Control = u_Other.m_Control;
			u_Other.m_Allocator = nullptr;
			u_Other.m_Ptr = nullptr;
			u_Other.m_Control = nullptr;
			return *this;
		}

		~WeakPtr() = default;

		SharedPtr<T, TAllocator> lock() const noexcept {
			if (!m_Control || !m_Control->tryAddRef()) return SharedPtr<T, TAllocator>{};
			return SharedPtr<T, TAllocator>(m_Ptr, m_Control, m_Allocator);
		}

		bool expired() const noexcept {
			return !m_Control || m_Control->useCount() == 0u;
		}

		explicit operator bool() const noexcept { return !expired(); }

		void reset() noexcept {
			m_Allocator = nullptr;
			m_Ptr = nullptr;
			m_Control = nullptr;
		}
	};

	// Deferred - defined after WeakPtr is complete.
	template<typename T, typename TAllocator>
	WeakPtr<T, TAllocator> SharedPtr<T, TAllocator>::weak() const noexcept {
		return WeakPtr<T, TAllocator>(*this);
	}
} 