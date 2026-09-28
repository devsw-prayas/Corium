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

namespace Corium::Memory {
	struct VirtualSegment;
}

namespace Corium::Memory::Allocators {
	// track() is a deliberate no-op seam for future allocation tagging/instrumentation.
	struct AllocatorTrackingBase {
	protected:
		void track() noexcept {}
	};

	// ------------------------------------------------------------------------
	// Tier 0 - IArena: raw void*, bump/watermark only, no individual free.
	// ------------------------------------------------------------------------

	template<typename D>
	concept ArenaImpl = requires(D & d, size_t n, size_t a) {
		{ d.allocateImpl(n, a) } -> std::same_as<void*>;
		{ d.allocateImpl(n) } -> std::same_as<void*>;
		{ d.deallocateImpl() } -> std::same_as<void>;
	};

	enum class MemOrigin : std::uint8_t {
		PagedVirtual
	};

	struct CORIUM_RUNTIME_API ArenaMetadata final {
		MemOrigin m_Origin;
	};

	template<typename Arena>
	struct CORIUM_RUNTIME_API ResolveArena final {
		static constexpr ArenaMetadata metadata{ MemOrigin::PagedVirtual };
	};

	// CRTP base. `D` must satisfy `ArenaImpl`, but that cannot be checked via a
	// `requires` clause on this template header - `D` is still an incomplete type
	// at the point its own base-specifier list (`: IArena<D>`) is instantiated, so
	// none of D's member functions are visible yet. The check is deferred to first
	// construction instead, by which point D is complete.

	template<typename D>
	class IArena : public AllocatorTrackingBase {
		using derived_ = D;

	public:
		IArena() noexcept {
			CORIUM_STATIC_ASSERT(ArenaImpl<D>,
				"Arena must implement allocateImpl(bytes,align)->void*, allocateImpl(bytes)->void*, deallocateImpl()->void");
		}

		IArena(const IArena&) = default;
		IArena& operator=(const IArena&) = default;
		IArena(IArena&&) noexcept = default;
		IArena& operator=(IArena&&) noexcept = default;
		~IArena() = default;

		CORIUM_NODISCARD_MSG("Cannot discard allocated block pointer")
		void* allocate(size_t v_Bytes, size_t v_Alignment) {
			return static_cast<derived_*>(this)->allocateImpl(v_Bytes, v_Alignment);
		}

		CORIUM_NODISCARD_MSG("Cannot discard allocated block pointer")
		void* allocate(size_t v_Bytes) {
			return static_cast<derived_*>(this)->allocateImpl(v_Bytes);
		}

		// Whole-arena discard only - no per-pointer free at this tier.
		void reset() {
			static_cast<derived_*>(this)->deallocateImpl();
		}
	};

	// ------------------------------------------------------------------------
	// AllocationHeader - internal detail used by header/freelist-driven Tier 1
	// policies (e.g. GeneralAllocator). Not part of IAllocator's public surface.
	// ------------------------------------------------------------------------

	enum class AllocationTrait : std::uint8_t {
		Scratch,
		Task,
		Executor,
		Persistent,
		Unknown
	};

	// Tooling-only tag (Stratum/Profiler bucketing) - never read for control flow.
	template <typename T, typename = void> struct CORIUM_RUNTIME_API ResolveAllocation final {
		static constexpr AllocationTrait trait = AllocationTrait::Unknown;
	};

	struct alignas(64) CORIUM_RUNTIME_API AllocationHeader final {
		void* m_Block;
		size_t m_Size;
		size_t m_Alignment;
		AllocationTrait m_Trait;

		AllocationHeader(void* p_Block, size_t v_Size, size_t v_Alignment, AllocationTrait v_Trait) :
			m_Block(p_Block), m_Size(v_Size), m_Alignment(v_Alignment), m_Trait(v_Trait) {}

		~AllocationHeader() = default;

		// Deleted converting constructor from a bare AllocationTrait - not the copy
		// constructor. The real copy constructor is deleted explicitly below so the
		// deletion says what it means instead of relying on the defaulted move
		// constructor to suppress it as a side effect.
		AllocationHeader(const AllocationTrait&) = delete;
		AllocationHeader(const AllocationHeader&) = delete;
		AllocationHeader& operator=(const AllocationHeader&) = delete;

		AllocationHeader(AllocationHeader&&) noexcept = default;
		AllocationHeader& operator=(AllocationHeader&&) noexcept = default;

		template<typename T>
		T* as() const noexcept {
			return static_cast<T*>(m_Block);
		}
	};

	CORIUM_STATIC_ASSERT(std::is_standard_layout_v<AllocationHeader>,
		"AllocationHeader must maintain standard layout");
	CORIUM_STATIC_ASSERT(std::is_trivially_copyable_v<AllocationHeader>,
		"AllocationHeader must be trivially copyable");
	CORIUM_STATIC_ASSERT(std::is_trivially_move_assignable_v<AllocationHeader>,
		"AllocationHeader must be trivially move assignable");

	// ------------------------------------------------------------------------
	// Tier 1 - IAllocator<T, Arena, D>: consumes an Arena, presents contiguous
	// array behaviour over T. T = void is the raw/untyped specialization.
	// ------------------------------------------------------------------------

	template<typename D>
	concept RawAllocatorImpl = requires(D & d, void* p, size_t n) {
		{ d.deallocateImpl(p, n) } -> std::same_as<void>;
	};

	template<typename D, typename T>
	concept TypedAllocatorImpl = requires(D & d, size_t n) {
		{ d.allocateImpl(n) } -> std::same_as<T*>;
	};

	enum class MemType : std::uint8_t {
		Raw,
		Typed
	};

	struct CORIUM_RUNTIME_API AllocatorMetadata final {
		MemType m_Type;
		AllocationTrait m_Mechanism;
	};

	template<typename Allocator>
	struct CORIUM_RUNTIME_API ResolveAllocator final {
		static constexpr AllocatorMetadata metadata{ MemType::Raw, AllocationTrait::Unknown };
	};

	template<typename T, typename Arena, typename D>
	class IAllocator : public AllocatorTrackingBase {
		using derived_ = D;

	protected:
		Arena m_UnderlyingArena;

	public:
		IAllocator() noexcept {
			if constexpr (std::is_void_v<T>) {
				CORIUM_STATIC_ASSERT(RawAllocatorImpl<D>,
					"Raw allocator must implement deallocateImpl(void*,size_t)->void");
			} else {
				CORIUM_STATIC_ASSERT((TypedAllocatorImpl<D, T>),
					"Typed allocator must implement allocateImpl(count)->T*");
			}
		}

		IAllocator(const IAllocator&) = default;
		IAllocator& operator=(const IAllocator&) = default;
		IAllocator(IAllocator&&) noexcept = default;
		IAllocator& operator=(IAllocator&&) noexcept = default;
		~IAllocator() = default;

		void init(VirtualSegment* p_Segment) noexcept {
			m_UnderlyingArena.init(p_Segment);
		}

		// ---- Raw (T = void) ----
		// allocate()/emplace() dispatch through derived_::allocateImpl (CRTP), not
		// straight to the underlying Arena - this is what lets a header/freelist
		// policy (GeneralAllocator) wrap every allocation while a pure-bump policy
		// (RawBumpAllocator<Tag>) can stay a fully empty derived struct and just
		// inherit the defaults below, which forward to the Arena.

		CORIUM_NODISCARD_MSG("Cannot discard allocated block pointer")
		void* allocate(size_t v_Bytes, size_t v_Alignment) requires std::is_void_v<T> {
			return static_cast<derived_*>(this)->allocateImpl(v_Bytes, v_Alignment);
		}

		CORIUM_NODISCARD_MSG("Cannot discard allocated block pointer")
		void* allocate(size_t v_Bytes) requires std::is_void_v<T> {
			return static_cast<derived_*>(this)->allocateImpl(v_Bytes);
		}

		// Placement-new at a caller-supplied, already-allocated pointer only -
		// symmetric with the typed (T != void) tier's emplace(T*, Args&&...)
		// below; callers now allocate and construct as two separate steps.
		template<typename U, typename... Args>
		U* emplace(void* p_Ptr, Args&&... v_Args) requires std::is_void_v<T> {
			CORIUM_ASSERT(p_Ptr != nullptr);
			return ::new (p_Ptr) U(std::forward<Args>(v_Args)...);
		}

		// Policy-defined: no-op for pure-bump derivations, real freelist reclaim
		// for header/freelist-driven derivations (e.g. GeneralAllocator).
		void deallocate(void* p_Ptr, size_t v_Size) requires std::is_void_v<T> {
			static_cast<derived_*>(this)->deallocateImpl(p_Ptr, v_Size);
		}

		// Whole-arena discard. Derived types that track extra state on top of the
		// Arena (e.g. GeneralAllocator's freelist) hide this with their own reset().
		void reset() requires std::is_void_v<T> {
			m_UnderlyingArena.reset();
		}

		// Pure-bump defaults: forward straight to the Arena, no individual reclaim.
		// Derived types with real per-allocation policy (GeneralAllocator) hide
		// these with their own allocateImpl/deallocateImpl.
		void* allocateImpl(size_t v_Bytes, size_t v_Alignment) requires std::is_void_v<T> {
			return m_UnderlyingArena.allocate(v_Bytes, v_Alignment);
		}

		void* allocateImpl(size_t v_Bytes) requires std::is_void_v<T> {
			return m_UnderlyingArena.allocate(v_Bytes);
		}

		void deallocateImpl(void*, size_t) noexcept requires std::is_void_v<T> {}

		// ---- Typed (T != void) ----
		// Forward scaffolding only - not consumed by any concrete allocator yet.
		// Member templates on U so a dllexported IAllocator<void> subclass doesn't force
		// instantiating them (clang-cl ignores the requires-clause when exporting).

		template<typename U = T>
		CORIUM_NODISCARD_MSG("Cannot discard allocated block pointer")
		U* allocate(size_t v_Count = 1) requires (!std::is_void_v<U>) {
			return static_cast<U*>(m_UnderlyingArena.allocate(v_Count * sizeof(U), alignof(U)));
		}

		// Placement-new at a caller-supplied, already-allocated pointer only.
		template<typename U = T, typename... Args>
		U* emplace(U* p_Ptr, Args&&... v_Args) requires (!std::is_void_v<U>) {
			CORIUM_ASSERT(p_Ptr != nullptr);
			return ::new (p_Ptr) U(std::forward<Args>(v_Args)...);
		}

		template<typename U = T>
		void destroy(U* p_Ptr, size_t v_Count = 1) requires (!std::is_void_v<U>) {
			for (size_t i = 0; i < v_Count; ++i)
				p_Ptr[i].~U();
		}
	};
}
