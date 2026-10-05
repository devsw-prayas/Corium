#include "Corium.h"
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
    }

    FrameHandle& this_thread::nativeContext() {
        CORIUM_ASSERT(t_NativeContext != nullptr && "thread is not attached to Corium");
        return *t_NativeContext;
    }

    FrameHandle* this_thread::currentFrame() {
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
        void releasePoolBlock(FrameHandle* p_Dead) noexcept {
            auto* base = static_cast<uint8_t*>(p_Dead->m_StackPtr);
            const size_t size = p_Dead->m_StackSize;
            auto* allocator = ::Corium::Memory::Internal::AtomicAllocators::instance()
                .s_FrameAllocator[p_Dead->m_NumaNode].load(Core::Atomics::MemoryOrder::ACQUIRE);

            const bool lifted = ::Corium::Memory::VirtualMemory::unprotectMem(
                ::Corium::Memory::createSegment({ base + ::Corium::Memory::PAGE_SIZE, ::Corium::Memory::PAGE_SIZE }));
            CORIUM_ASSERT(lifted && "Could not lift the guard page");
            if (!lifted) return;

            allocator->deallocate(base, size);
        }

        // Runs wherever a switch lands: after the context switch returns in captureFrame (resumed frame)
        // and at trampoline start (fresh frame). Only here has the terminated frame fully left its stack.
        void afterSwitch() noexcept {
            FrameHandle* dead = t_PendingReclaim;
            if (!dead) return;
            t_PendingReclaim = nullptr;

            // Release store: owners polling for TERMINATED may reuse the memory once they see it.
            ::Corium::Atomics::store<::Corium::Atomics::MemoryOrder::RELEASE>(&dead->m_State, FrameState::TERMINATED);

            // Pool blocks are Corium's, so they go back as soon as the frame has left them. The handle is invalid after this.
            if (dead->m_Origin == Provenance::NUMA_GLOBAL) releasePoolBlock(dead);
        }

        void trampolineEntry(void* p_Ctx){
            afterSwitch();
            auto* self = static_cast<FrameHandle*>(p_Ctx);
            self->m_Entry();
            NativeFrame::yieldToNative(self, true);
            CORIUM_UNREACHABLE();
        }
    }

    void this_thread::captureFrame(FrameHandle *p_Incoming){
        auto* self = this_thread::currentFrame();
        CORIUM_ASSERT(self != p_Incoming && "Cannot capture a frame already held by the thread");
        CORIUM_ASSERT(p_Incoming->m_State != FrameState::TERMINATED && "Incoming frame cannot be terminated");
        CORIUM_ASSERT(p_Incoming->m_State != FrameState::RUNNING && "Incoming frame cannot be running");
        t_CurrentFrame = p_Incoming;
        t_CurrentFrame->m_State = FrameState::RUNNING;
        self->m_State = FrameState::SUSPENDED;
        Internal::CoriumFrame_ContextSwitch(self, p_Incoming);
        afterSwitch();
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

        CORIUM_STATIC_ASSERT(kReservedHeaderSize <= ::Corium::Memory::PAGE_SIZE, "Header and register blob must fit in the page below the guard page");

        template<typename T>
        void storeSlot(void* p_Dst, T v_Value) noexcept {
            std::memcpy(p_Dst, &v_Value, sizeof(T));
        }

        // Volatile stores so the optimizer cannot drop the wipe. Blocks are 16-byte multiples, so word stores cover them.
        void secureZero(void* p_Dst, size_t v_Size) noexcept {
            auto* words = static_cast<volatile uint64_t*>(p_Dst);
            for (size_t i = 0; i < v_Size / sizeof(uint64_t); ++i) words[i] = 0;
            auto* tail = static_cast<volatile uint8_t*>(p_Dst);
            for (size_t i = v_Size & ~(sizeof(uint64_t) - 1); i < v_Size; ++i) tail[i] = 0;
        }
    }

    std::optional<FrameHandle*> Internal::createFrameInternal(Utils::FunctionView<void ()> v_View, const FrameStackDesc& ro_Desc) {
        // validate() freezes a desc, so an unfrozen one was never checked. One frozen desc can stamp out many frames.
        if (ro_Desc.m_State != DescriptorState::FROZEN) return std::nullopt;

        auto* base = static_cast<uint8_t*>(ro_Desc.m_Memory);
        const size_t size = ro_Desc.m_Size;

        if (ro_Desc.m_Provenance == Provenance::NUMA_GLOBAL) {
            auto* allocator = ::Corium::Memory::Internal::AtomicAllocators::instance()
                .s_FrameAllocator[ro_Desc.m_NumaNode].load(Core::Atomics::MemoryOrder::ACQUIRE);

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
        handle->m_Entry = v_View;
        handle->m_StackPtr = base;
        handle->m_StackSize = size;
        handle->m_RegBlob = blob;
        handle->m_Origin = ro_Desc.m_Provenance;
        handle->m_NumaNode = static_cast<uint8_t>(ro_Desc.m_NumaNode);
        handle->m_State = FrameState::READY;

        // A frame that has never run has nothing to restore, so the blob is built by hand and the switch loads it as usual.
        std::memset(blob, 0, ::Corium::Memory::Internal::FrameRegBlobSize);

        const uintptr_t top = reinterpret_cast<uintptr_t>(base) + size;
        const uintptr_t entryRsp = (top & ~uintptr_t{ 15 }) - kEntryFrameBytes;

        // A stray return out of the trampoline jumps to null and faults, instead of executing whatever sits above the stack.
        storeSlot<uint64_t>(reinterpret_cast<void*>(entryRsp), 0);

        storeSlot<uint64_t>(blob + Internal::kOffsetRip, reinterpret_cast<uintptr_t>(&trampolineEntry));
        storeSlot<uint64_t>(blob + Internal::kOffsetRsp, entryRsp);
        storeSlot<uint32_t>(blob + Internal::kOffsetMxcsr, kDefaultMxcsr);

        return handle;
    }

    void NativeFrame::switchTo(FrameHandle* p_Self, FrameHandle* p_Target, bool v_Terminate) {
        CORIUM_ASSERT(p_Self == this_thread::currentFrame() && "switchTo must be called by the frame that is running");
        // Self's memory is untouchable until execution has left it, so the landing side (afterSwitch) finishes the job.
        if (v_Terminate) t_PendingReclaim = p_Self;
        this_thread::captureFrame(p_Target);
    }

    void NativeFrame::yieldToNative(FrameHandle* p_Self, bool v_Terminate) {
        // FrameHandle deletes operator&, so the native context's address comes from addressof.
        switchTo(p_Self, std::addressof(this_thread::nativeContext()), v_Terminate);
    }

    void NativeFrame::killFrame(FrameHandle* p_Target) {
        CORIUM_ASSERT(p_Target != nullptr);
        CORIUM_ASSERT(p_Target != this_thread::currentFrame() && "Cannot kill the frame that is running");
        CORIUM_ASSERT(p_Target->m_State != FrameState::RUNNING && "Cannot kill a running frame");

        auto* base = static_cast<uint8_t*>(p_Target->m_StackPtr);
        const size_t size = p_Target->m_StackSize;

        // A pool block's guard page faults on any touch, so lift it before wiping (harmless when the block has none).
        if (p_Target->m_Origin == Provenance::NUMA_GLOBAL) {
            const bool lifted = ::Corium::Memory::VirtualMemory::unprotectMem(
                ::Corium::Memory::createSegment({ base + ::Corium::Memory::PAGE_SIZE, ::Corium::Memory::PAGE_SIZE }));
            CORIUM_ASSERT(lifted && "Could not lift the guard page");
            if (!lifted) return;
        }

        // Nothing of the old occupant survives, then a blank handle stands at the same address (assignment is deleted).
        secureZero(base, size);
        auto* blank = ::new (base) FrameHandle{};

        // A blank READY handle with no entry would crash whoever dispatched it, so it reads as finished instead.
        ::Corium::Atomics::store<::Corium::Atomics::MemoryOrder::RELEASE>(&blank->m_State, FrameState::TERMINATED);
    }
}
