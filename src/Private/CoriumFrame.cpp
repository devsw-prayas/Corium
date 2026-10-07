#include "Corium.h"
#include "CoriumMemoryHandler.h"
#include "CoriumAsm.h"
#include "CoriumCompiler.h"
#include "CoriumDiagnostics.h"
#include "FrameUtils.h"
#include "CoriumFrame.h"
#include "Atomics.h"
#include <cstring>
#include <memory>

namespace Corium::Core::Frame {
	namespace {
		thread_local FrameHandle* t_NativeContext = nullptr;
		thread_local FrameHandle* t_CurrentFrame = nullptr;
		// Set by switchTo(..., terminate = true); consumed on the landing side by afterSwitch.
		thread_local FrameHandle* t_PendingReclaim = nullptr;
		// Published SUSPENDED by afterSwitch, once its blob is fully saved.
		thread_local FrameHandle* t_PendingSuspend = nullptr;
	}

	CORIUM_NOINLINE FrameHandle& this_thread::nativeContext() {
		CORIUM_ASSERT(t_NativeContext != nullptr && "thread is not attached to Corium");
		return *t_NativeContext;
	}

	CORIUM_NOINLINE FrameHandle* this_thread::currentFrame() {
		return t_CurrentFrame;
	}

	void Internal::bindNativeContext(void* p_Storage, uint8_t v_NumaNode) noexcept {
		// Value-init zeroes every field; the blob needs no init, the first switch out writes it.
		auto* handle = ::new (p_Storage) FrameHandle{};
		handle->m_RegBlob = static_cast<std::byte*>(p_Storage) + sizeof(FrameHandle);
		handle->m_NumaNode = v_NumaNode;
		// The thread is running on it right now, and its memory belongs to the thread's TLS arena, never the frame pool.
		handle->m_State = FrameState::RUNNING;
		handle->m_Origin = Provenance::CALLEE_OWNED;
		handle->m_IsNative = true;
		t_NativeContext = handle;
		t_CurrentFrame = handle;
	}

	void Internal::unbindNativeContext() noexcept {
		t_NativeContext = nullptr;
		t_CurrentFrame = nullptr;
	}

	namespace {
		// Back to the node's frame allocator. The guard page is lifted first: the allocator threads its own bookkeeping
		// through freed memory, and whoever receives the block next would fault on it. Lifting is harmless on a block
		// that never had a guard; if it fails, leaking the block beats handing out a trap page.
		void releasePoolBlock(const FrameHandle* p_Dead) noexcept {
			auto* base = static_cast<uint8_t*>(p_Dead->m_StackPtr);
			const size_t size = p_Dead->m_StackSize;
			auto* allocator = Memory::Internal::AtomicAllocators::instance().s_FrameAllocator[p_Dead->m_NumaNode].load(Core::Atomics::MemoryOrder::ACQUIRE);

			const bool lifted = Memory::VirtualMemory::unprotectMem(Memory::createSegment({ base + ::Corium::Memory::PAGE_SIZE, ::Corium::Memory::PAGE_SIZE }));
			CORIUM_ASSERT(lifted && "Could not lift the guard page");
			if (!lifted) return;

			allocator->deallocate(base, size);
		}

		// Top of the block, 16-aligned, so the stack grows away from it and overflow reaches the header first.
		FrameEntry* entrySlot(const FrameHandle* p_Frame) noexcept {
			const uintptr_t top = reinterpret_cast<uintptr_t>(p_Frame->m_StackPtr) + p_Frame->m_StackSize;
			return std::bit_cast<FrameEntry*>((top - kFrameEntrySlotSize) & ~uintptr_t{ 15 });
		}

		// Leaves an empty closure behind, so a second destroy (terminate, then killFrame on the same block) is a no-op.
		void destroyEntry(const FrameHandle* p_Frame) noexcept {
			// Native contexts have no slot; a killed frame's blank handle has no block left.
			if (p_Frame->m_IsNative || !p_Frame->m_StackPtr) return;
			FrameEntry* slot = entrySlot(p_Frame);
			slot->~FrameEntry();
			::new (slot) FrameEntry{};
		}

		// Runs wherever a switch lands: after the context switch returns in captureFrame (resumed frame)
		// and at trampoline start (fresh frame). Only here has the outgoing frame fully left its stack.
		// Noinline: the frame may have migrated, so TLS must be resolved on the landing carrier.
		CORIUM_NOINLINE void afterSwitch() noexcept {
			if (FrameHandle* parked = t_PendingSuspend) {
				t_PendingSuspend = nullptr;
				::Corium::Atomics::store<Atomics::MemoryOrder::RELEASE>(&parked->m_State, FrameState::SUSPENDED);
			}

			FrameHandle* dead = t_PendingReclaim;
			if (!dead) return;
			t_PendingReclaim = nullptr;

			// Before TERMINATED is published: an owner seeing it may reuse the block, slot included.
			destroyEntry(dead);

			// Release store: owners polling for TERMINATED may reuse the memory once they see it.
			::Corium::Atomics::store<Atomics::MemoryOrder::RELEASE>(&dead->m_State, FrameState::TERMINATED);

			// Pool blocks are Corium's, so they go back as soon as the frame has left them. The handle is invalid after this.
			if (dead->m_Origin == Provenance::NUMA_GLOBAL) releasePoolBlock(dead);
		}

		CORIUM_NORETURN void trampolineEntry(void* p_Ctx) {
			afterSwitch();
			auto* self = static_cast<FrameHandle*>(p_Ctx);
			self->m_Entry();
			NativeFrame::yieldToNative(self, true);
			CORIUM_UNREACHABLE();
		}

		// Acquire pairs with afterSwitch's release, so a migrated frame arrives with its blob complete.
		bool claim(FrameHandle* p_Frame) noexcept {
			using ::Corium::Atomics::MemoryOrder;
			FrameState expected = ::Corium::Atomics::load<MemoryOrder::ACQUIRE>(&p_Frame->m_State);
			if (expected != FrameState::READY && expected != FrameState::SUSPENDED) return false;
			return ::Corium::Atomics::compareExchange<MemoryOrder::ACQ_REL>(&p_Frame->m_State, expected, FrameState::RUNNING);
		}
	}

	bool this_thread::tryCaptureFrame(FrameHandle* p_Incoming) {
		auto* self = this_thread::currentFrame();
		CORIUM_ASSERT(self != p_Incoming && "Cannot capture a frame already held by the thread");
		CORIUM_ASSERT((!p_Incoming->m_IsNative || p_Incoming == t_NativeContext) && "Native contexts never migrate");

		const bool claimed = claim(p_Incoming);
		if (!claimed) {
			if (t_PendingReclaim == self) t_PendingReclaim = nullptr;
			return false;
		}

		t_CurrentFrame = p_Incoming;
		// A terminating frame must never become claimable.
		if (t_PendingReclaim != self) t_PendingSuspend = self;
		Internal::CoriumFrame_ContextSwitch(self, p_Incoming);
		afterSwitch();
		return true;
	}

	void this_thread::captureFrame(FrameHandle* p_Incoming) {
		const bool captured = tryCaptureFrame(p_Incoming);
		CORIUM_ASSERT(captured && "Incoming frame must be READY or SUSPENDED and unclaimed");
	}

	namespace {
		// What a callee sees right after a call: [rsp] is the return slot, and on Win64 the 32 B of shadow space sit above it.
		// RSP at entry is therefore 8 mod 16. SysV has the return slot only.
#if defined(_WIN32)
		constexpr size_t kEntryFrameBytes = 8 + 32;
#else
		constexpr size_t kEntryFrameBytes = 8;
#endif
		// Every FP exception masked. A zero MXCSR unmasks them, and the first inexact operation crashes the frame.
		constexpr uint32_t kDefaultMxcsr = 0x1F80;
#if defined(_WIN32)
		// Windows default: 53-bit precision, round to nearest, every x87 exception masked.
		constexpr uint16_t kDefaultFpcw = 0x027F;
#endif

		CORIUM_STATIC_ASSERT(kReservedHeaderSize <= ::Corium::Memory::PAGE_SIZE, "Header and register blob must fit in the page below the guard page");

		template<typename T>
		void storeSlot(void* p_Dst, T v_Value) noexcept {
			std::memcpy(p_Dst, &v_Value, sizeof(T));
		}

		// Volatile stores so the optimizer cannot drop the wipe. Blocks are 16-byte multiples, so word stores cover them.
		void secureZero(void* p_Dst, size_t v_Size) noexcept {
			auto* words = static_cast<volatile uint64_t*>(p_Dst);
			for (size_t i = 0; i < v_Size / sizeof(uint64_t); ++i)
				words[i] = 0;
			auto* tail = static_cast<volatile uint8_t*>(p_Dst);
			for (size_t i = v_Size & ~(sizeof(uint64_t) - 1); i < v_Size; ++i)
				tail[i] = 0;
		}
	}

	std::optional<FrameHandle*> Internal::createFrameInternal(FrameEntry&& u_Entry, const FrameStackDesc& ro_Desc) {
		// validate() freezes a desc, so an unfrozen one was never checked. One frozen desc can stamp out many frames.
		if (ro_Desc.m_State != DescriptorState::FROZEN) return std::nullopt;
		// A closure whose capture allocation failed has no context to run with.
		if (!u_Entry.isCallable()) return std::nullopt;

		auto* base = static_cast<uint8_t*>(ro_Desc.m_Memory);
		const size_t size = ro_Desc.m_Size;

		if (ro_Desc.m_Provenance == Provenance::NUMA_GLOBAL) {
			auto* allocator = ::Corium::Memory::Internal::AtomicAllocators::instance().s_FrameAllocator[ro_Desc.m_NumaNode].load(Core::Atomics::MemoryOrder::ACQUIRE);

			// The guard page needs a page-aligned block; otherwise only the handle's own 64 B alignment.
			const bool guarded = static_cast<bool>(ro_Desc.m_IsGuardPagesEnabled);
			base = static_cast<uint8_t*>(allocator->allocate(size, guarded ? ::Corium::Memory::PAGE_SIZE : alignof(FrameHandle)));
			if (!base) return std::nullopt;

			// Page 0 holds header + blob, page 1 is the guard, the stack starts above it and grows down toward it.
			if (guarded && !::Corium::Memory::Internal::lockGuard({ base + ::Corium::Memory::PAGE_SIZE, ::Corium::Memory::PAGE_SIZE })) {
				allocator->deallocate(base, size);
				return std::nullopt;
			}
		}
		CORIUM_ASSERT(base != nullptr);

		// The handle is the first 64 B of the block and the register blob follows it.
		auto* handle = ::new (base) FrameHandle{};
		auto* blob = reinterpret_cast<std::byte*>(base) + sizeof(FrameHandle);
		handle->m_StackPtr = base;
		handle->m_StackSize = size;
		handle->m_RegBlob = blob;
		handle->m_Origin = ro_Desc.m_Provenance;
		handle->m_NumaNode = static_cast<uint8_t>(ro_Desc.m_NumaNode);
		handle->m_State = FrameState::READY;

		// A frame that has never run, has nothing to restore, so the blob is built by hand and the switch loads it as usual.
		std::memset(blob, 0, ::Corium::Memory::Internal::FrameRegBlobSize);

		// Moved in last, once nothing can fail, so a failed create leaves the caller's closure intact.
		FrameEntry* slot = ::new (entrySlot(handle)) FrameEntry(std::move(u_Entry));
		handle->m_Entry = Utils::FunctionView<void()>(*slot);

		// The stack starts below the entry slot, which is already 16-aligned.
		const uintptr_t entryRsp = reinterpret_cast<uintptr_t>(slot) - kEntryFrameBytes;

		// A stray return out of the trampoline jumps to null and faults, instead of executing whatever sits above the stack.
		storeSlot<uint64_t>(std::bit_cast<void*>(entryRsp), 0);

		storeSlot<uint64_t>(blob + kOffsetRip, reinterpret_cast<uintptr_t>(&trampolineEntry));
		storeSlot<uint64_t>(blob + kOffsetRsp, entryRsp);
		storeSlot<uint32_t>(blob + kOffsetMxcsr, kDefaultMxcsr);
#if defined(_WIN32)
		// The switch loads these into the TEB, so a zeroed blob would hand the frame null stack bounds.
		const bool guardedBlock = ro_Desc.m_Provenance == Provenance::NUMA_GLOBAL && static_cast<bool>(ro_Desc.m_IsGuardPagesEnabled);
		const uintptr_t usableLow = guardedBlock
			? reinterpret_cast<uintptr_t>(base) + 2 * ::Corium::Memory::PAGE_SIZE
			: (reinterpret_cast<uintptr_t>(base) + kReservedHeaderSize + 15) & ~uintptr_t{ 15 };
		storeSlot<uint64_t>(blob + kOffsetStackBase, reinterpret_cast<uintptr_t>(slot));
		storeSlot<uint64_t>(blob + kOffsetStackLimit, usableLow);
		storeSlot<uint64_t>(blob + kOffsetDeallocStack, reinterpret_cast<uintptr_t>(base));
		storeSlot<uint16_t>(blob + kOffsetFpcw, kDefaultFpcw);
#endif

		return handle;
	}

	void NativeFrame::switchTo(FrameHandle* p_Self, FrameHandle* p_Target, bool v_Terminate) {
		const bool switched = trySwitchTo(p_Self, p_Target, v_Terminate);
		CORIUM_ASSERT(switched && "Target frame must be READY or SUSPENDED and unclaimed");
	}

	bool NativeFrame::trySwitchTo(FrameHandle* p_Self, FrameHandle* p_Target, bool v_Terminate) {
		CORIUM_ASSERT(p_Self == this_thread::currentFrame() && "switchTo must be called by the frame that is running");
		// Self's memory is untouchable until execution has left it, so the landing side (afterSwitch) finishes the job.
		if (v_Terminate) t_PendingReclaim = p_Self;
		return this_thread::tryCaptureFrame(p_Target);
	}

	void NativeFrame::yieldToNative(FrameHandle* p_Self, bool v_Terminate) {
		// FrameHandle deletes operator&, so the native context's address comes from addressof.
		switchTo(p_Self, std::addressof(this_thread::nativeContext()), v_Terminate);
	}

	void NativeFrame::killFrame(const FrameHandle* p_Target) {
		CORIUM_ASSERT(p_Target != nullptr);
		CORIUM_ASSERT(p_Target != this_thread::currentFrame() && "Cannot kill the frame that is running");
		CORIUM_ASSERT(!p_Target->m_IsNative && "Cannot kill a thread's native context");
		CORIUM_ASSERT(p_Target->m_State != FrameState::RUNNING && "Cannot kill a running frame");

		// Runs the closure's destructor while its slot is intact; the wipe below would skip it.
		destroyEntry(p_Target);

		auto* base = static_cast<uint8_t*>(p_Target->m_StackPtr);
		const size_t size = p_Target->m_StackSize;

		// A pool block's guard page faults on any touch, so lift it before wiping (harmless when the block has none).
		if (p_Target->m_Origin == Provenance::NUMA_GLOBAL) {
			const bool lifted = ::Corium::Memory::VirtualMemory::unprotectMem(::Corium::Memory::createSegment({ base + ::Corium::Memory::PAGE_SIZE, ::Corium::Memory::PAGE_SIZE }));
			CORIUM_ASSERT(lifted && "Could not lift the guard page");
			if (!lifted) return;
		}

		// Nothing of the old occupant survives, then a blank handle stands at the same address (assignment is deleted).
		secureZero(base, size);
		auto* blank = ::new (base) FrameHandle{};

		// A blank READY handle with no entry would crash whoever dispatched it, so it reads as finished instead.
		::Corium::Atomics::store<Atomics::MemoryOrder::RELEASE>(&blank->m_State, FrameState::TERMINATED);
	}
}