#pragma once

#include "AtomicVar.h"
#include "CoriumAllocator.h"
#include "CoriumMemory.h"
#include "PlatIntrin.h"

namespace Corium::Memory::Allocators {
	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(64) BumpAllocator : IArena<BumpAllocator> {
		Core::Atomic::AtomicValue64<size_t> m_Bump{ 0 };
		VirtualSegment* m_Base = nullptr;
		size_t m_Size = 0;

		BumpAllocator() = default;
		BumpAllocator(const BumpAllocator&) = delete;
		BumpAllocator& operator=(const BumpAllocator&) = delete;
		BumpAllocator(BumpAllocator&&) noexcept = delete;
		BumpAllocator& operator=(BumpAllocator&&) noexcept = delete;
		~BumpAllocator() = default;

		void init(VirtualSegment * ro_Base) noexcept {
			m_Base = ro_Base;
			m_Size = m_Base->m_TotalSize;
			m_Bump.store(0, Core::Atomics::MemoryOrder::RELAXED);
		}

		CORIUM_NODISCARD_MSG("Cannot discard allocated block pointer")
			void* allocateImpl(size_t v_Bytes, size_t v_Align) noexcept;

		CORIUM_NODISCARD_MSG("Cannot discard allocated block pointer")
			void* allocateImpl(size_t v_Bytes) noexcept;

		// Whole-arena reset - the real Tier 0 reclaim, not a no-op stub.
		void deallocateImpl() noexcept {
			m_Bump.store(0, Core::Atomics::MemoryOrder::RELAXED);
		}
	};
	
	template<typename Tag>
	struct RawBumpAllocator final : IAllocator<void, BumpAllocator, RawBumpAllocator<Tag>> {};

	using TaskMetadataAllocator = RawBumpAllocator<struct TaskMetadataTag>;
	using TaskPayloadAllocator = RawBumpAllocator<struct TaskPayloadTag>;
	using ControlBlockAllocator = RawBumpAllocator<struct ControlBlockTag>;
	using ClosureAllocator = RawBumpAllocator<struct ClosureTag>;
	using TlsAllocator = RawBumpAllocator<struct TlsTag>;
	using FrameAllocator = RawBumpAllocator<struct FrameTag>;

	template<>
	struct CORIUM_RUNTIME_API ResolveAllocation<TaskMetadataAllocator> final {
		static constexpr AllocationTrait trait = AllocationTrait::Persistent;
	};

	template<>
	struct CORIUM_RUNTIME_API ResolveAllocation<TaskPayloadAllocator> final {
		static constexpr AllocationTrait trait = AllocationTrait::Persistent;
	};

	template<>
	struct CORIUM_RUNTIME_API ResolveAllocation<ControlBlockAllocator> final {
		static constexpr AllocationTrait trait = AllocationTrait::Persistent;
	};

	template<>
	struct CORIUM_RUNTIME_API ResolveAllocation<ClosureAllocator> final {
		static constexpr AllocationTrait trait = AllocationTrait::Persistent;
	};

	struct alignas(64) CORIUM_RUNTIME_API GeneralAllocator final : IAllocator<void, BumpAllocator, GeneralAllocator>{
	private:
		struct alignas(16) BlockHeader final {
			size_t m_SizeAndFlags;   // lower bit = free flag
			BlockHeader* m_NextFree;
			BlockHeader* m_PrevFree;

			static constexpr size_t FREE_BIT = 1;

			size_t size() const {
				return m_SizeAndFlags & ~FREE_BIT;
			}

			bool isFree() const {
				return (m_SizeAndFlags & FREE_BIT) != 0;
			}

			void set(size_t size, bool free) {
				m_SizeAndFlags = size | (free ? FREE_BIT : 0);
			}
		};

		struct BlockFooter final {
			size_t m_Size;
		};

	private:
		BlockHeader* m_FreeList = nullptr;
		Core::Atomic::AtomicValue32<uint32_t> m_Lock{ 0 };

		// findFree/insertFree/split/coalesce touch m_FreeList and block headers with plain
		// pointers, so this bookkeeping needs real mutual exclusion.
		// TODO: whole-function lock is coarse (shows up as jitter under contention) - revisit
		// with finer-grained locking or a lock-free free-list if it ever becomes a hot path.
		struct LockGuard final {
			Core::Atomic::AtomicValue32<uint32_t>& m_Lock;
			explicit LockGuard(Core::Atomic::AtomicValue32<uint32_t>& r_Lock) : m_Lock(r_Lock) {
				uint32_t expected = 0;
				while (!m_Lock.compareExchange(&expected, 1u,
					Core::Atomics::MemoryOrder::ACQUIRE, Core::Atomics::MemoryOrder::RELAXED)) {
					expected = 0;
					Corium::Intrinsic::Pause();
				}
			}
			~LockGuard() { m_Lock.store(0u, Core::Atomics::MemoryOrder::RELEASE); }
		};

	private:
		static CORIUM_FORCEINLINE uintptr_t alignUp(uintptr_t v, size_t a) {
			return (v + a - 1) & ~(a - 1);
		}

		// Byte-level view into the composed BumpAllocator's own state, since
		// GeneralAllocator no longer keeps its own duplicate base/cursor bookkeeping.
		CORIUM_FORCEINLINE uint8_t* arenaBase() const {
			return static_cast<uint8_t*>(m_UnderlyingArena.m_Base->m_Memory);
		}

		CORIUM_FORCEINLINE uint8_t* arenaCursor() const {
			return arenaBase() + m_UnderlyingArena.m_Bump.load(Core::Atomics::MemoryOrder::RELAXED);
		}

		static CORIUM_FORCEINLINE BlockFooter* footer(BlockHeader* h) {
			return reinterpret_cast<BlockFooter*>(
				reinterpret_cast<uint8_t*>(h) + h->size() - sizeof(BlockFooter));
		}

		static CORIUM_FORCEINLINE BlockHeader* next(BlockHeader* h) {
			return reinterpret_cast<BlockHeader*>(
				reinterpret_cast<uint8_t*>(h) + h->size());
		}

		static CORIUM_FORCEINLINE BlockHeader* prev(BlockHeader* h) {
			auto* f = reinterpret_cast<BlockFooter*>(
				reinterpret_cast<uint8_t*>(h) - sizeof(BlockFooter));
			return reinterpret_cast<BlockHeader*>(
				reinterpret_cast<uint8_t*>(h) - f->m_Size);
		}

		void insertFree(BlockHeader* b) {
			b->m_NextFree = m_FreeList;
			b->m_PrevFree = nullptr;

			if (m_FreeList)
				m_FreeList->m_PrevFree = b;

			m_FreeList = b;
		}

		void removeFree(BlockHeader* b) {
			if (b->m_PrevFree)
				b->m_PrevFree->m_NextFree = b->m_NextFree;
			else
				m_FreeList = b->m_NextFree;

			if (b->m_NextFree)
				b->m_NextFree->m_PrevFree = b->m_PrevFree;
		}

		BlockHeader* findFree(size_t size) {
			BlockHeader* cur = m_FreeList;
			while (cur) {
				if (cur->size() >= size)
					return cur;
				cur = cur->m_NextFree;
			}
			return nullptr;
		}

		void split(BlockHeader* b, size_t needed) {
			size_t remaining = b->size() - needed;

			constexpr size_t MIN_SPLIT = sizeof(BlockHeader) + sizeof(BlockFooter) + 32;

			if (remaining < MIN_SPLIT)
				return;

			b->set(needed, false);

			auto* newBlock = reinterpret_cast<BlockHeader*>(
				reinterpret_cast<uint8_t*>(b) + needed);

			newBlock->set(remaining, true);

			footer(b)->m_Size = b->size();
			footer(newBlock)->m_Size = newBlock->size();

			insertFree(newBlock);
		}

		BlockHeader* coalesce(BlockHeader* b) {
			// next
			auto* n = next(b);
			if (reinterpret_cast<uint8_t*>(n) < arenaCursor() && n->isFree()) {
				removeFree(n);
				b->set(b->size() + n->size(), true);
			}

			// prev
			if (reinterpret_cast<uint8_t*>(b) > arenaBase()) {
				auto* p = prev(b);
				if (p->isFree()) {
					removeFree(p);
					p->set(p->size() + b->size(), true);
					b = p;
				}
			}

			footer(b)->m_Size = b->size();
			return b;
		}

	public:
		// Own init: forwards to the base (which inits the composed BumpAllocator),
		// then resets the freelist - the one piece of state the base doesn't know
		// about.
		void init(VirtualSegment* seg) {
			IAllocator<void, BumpAllocator, GeneralAllocator>::init(seg);
			m_FreeList = nullptr;
		}

		// Fast path pulls a fresh block from the composed BumpAllocator (over-sized
		// by worst-case alignment slack so a correctly-aligned user pointer can
		// always be carved out after the header); falls back to the freelist when
		// the arena is exhausted.
		void* allocateImpl(size_t size, size_t alignment) {
			LockGuard guard(m_Lock);
			if (!m_UnderlyingArena.m_Base) return nullptr;

			const size_t slack = alignment > alignof(BlockHeader) ? alignment - alignof(BlockHeader) : 0;
			// BACK_PTR_SLOT: header always sits at the arena's own start for this block (raw,
			// or a free-list block's own address) - never shifted for alignment, or the
			// shifted-over bytes become an untracked gap next()/prev() can walk into as a fake
			// neighbor. Any slack needed for an over-aligned userPtr lands *after* the header
			// instead, inside this block's own already-accounted span, with the real header's
			// address stashed in the pointer-sized slot immediately before userPtr so
			// deallocateImpl can recover it regardless of how far alignment pushed userPtr.
			constexpr size_t BACK_PTR_SLOT = sizeof(BlockHeader*);
			const size_t total = alignUp(sizeof(BlockHeader) + BACK_PTR_SLOT + slack + size + sizeof(BlockFooter), alignof(BlockHeader));

			if (uint8_t* raw = static_cast<uint8_t*>(m_UnderlyingArena.allocate(total, alignof(BlockHeader)))) {
				auto* header = reinterpret_cast<BlockHeader*>(raw);
				uint8_t* userPtr = reinterpret_cast<uint8_t*>(alignUp(
					reinterpret_cast<uintptr_t>(raw) + sizeof(BlockHeader) + BACK_PTR_SLOT, alignment));
				*reinterpret_cast<BlockHeader**>(userPtr - BACK_PTR_SLOT) = header;

				header->set(total, false);
				footer(header)->m_Size = total;
				// no stale free-list linkage carried forward into this block's new life
				header->m_NextFree = nullptr;
				header->m_PrevFree = nullptr;

				return userPtr;
			}

			auto* b = findFree(total);
			if (!b) return nullptr;

			removeFree(b);
			split(b, total);

			b->set(b->size(), false);
			b->m_NextFree = nullptr;
			b->m_PrevFree = nullptr;

			uint8_t* userPtr = reinterpret_cast<uint8_t*>(alignUp(
				reinterpret_cast<uintptr_t>(b) + sizeof(BlockHeader) + BACK_PTR_SLOT, alignment));
			*reinterpret_cast<BlockHeader**>(userPtr - BACK_PTR_SLOT) = b;
			return userPtr;
		}

		void* allocateImpl(size_t size) {
			return allocateImpl(size, alignof(std::max_align_t));
		}

		// Real coalesce-and-cursor-rewind reclaim - unlike the pure-bump
		// allocators, GeneralAllocator actually reclaims individual blocks.
		void deallocateImpl(void* ptr, size_t) {
			LockGuard guard(m_Lock);
			if (!ptr) return;

			auto* b = *reinterpret_cast<BlockHeader**>(static_cast<uint8_t*>(ptr) - sizeof(BlockHeader*));
			b->set(b->size(), true);
			b = coalesce(b);

			uint8_t* end = reinterpret_cast<uint8_t*>(b) + b->size();

			// Cursor rewind: if this block was the most recent bump allocation,
			// hand it straight back to the arena instead of parking it on the
			// freelist.
			if (end == arenaCursor()) {
				m_UnderlyingArena.m_Bump.store(
					static_cast<size_t>(reinterpret_cast<uint8_t*>(b) - arenaBase()),
					Core::Atomics::MemoryOrder::RELAXED);
				return;
			}

			insertFree(b);
		}

		// Own reset: the base only knows how to reset the composed BumpAllocator's
		// bump pointer - the freelist is GeneralAllocator's own state on top of it.
		void reset() {
			LockGuard guard(m_Lock);
			m_FreeList = nullptr;
			IAllocator<void, BumpAllocator, GeneralAllocator>::reset();
		}
	};
}
