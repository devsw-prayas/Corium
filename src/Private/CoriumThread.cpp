#include "Corium.h"

#define ALLOW_SYSCALL
#include "CoriumSyscalls.h"
#include "CoriumCompiler.h"
#include "CoriumDiagnostics.h"
#include "CoriumThread.h"
#include "CoriumMemoryHandler.h"
#include "InternalUtils.h"
#include "CoriumFrame.h"
#include "ThreadLocals.h"

#if CORIUM_COMPILER_MSVC
#pragma comment(lib, "synchronization.lib")
#endif

namespace Corium::Core {
	using namespace Corium::Core::Atomics;
	using namespace Corium::Memory::Literals;

	// Internal constants

	constexpr size_t MAX_CORIUM_THREADS = 256;
	constexpr size_t INVALID_SLOT = static_cast<size_t>(-1);

	// LaunchContext
	//
	// Bump-allocated from GeneralAllocator[0], heap-lifetime.
	// Moved into by createThread, destroyed by WinThreadThunk after the
	// closure returns. Owns the Closure<void()> for the thread's lifetime.

	struct alignas(16) LaunchContext final {
		Closure<void()>                       m_Closure;
		Memory::Allocators::GeneralAllocator* m_Allocator;
		size_t                                m_Slot;
		size_t								  m_TlsSize;

		LaunchContext(Closure<void()>&& u_Closure,
			Memory::Allocators::GeneralAllocator* p_Allocator, size_t v_Slot, size_t v_TlsSize) noexcept
			: m_Closure(std::move(u_Closure))
			, m_Allocator(p_Allocator)
			, m_Slot(v_Slot)
			, m_TlsSize(v_TlsSize) {}
	};

	// RegistryEntry
	//
	// One cache-line per thread slot. m_TokenMask is a bitmask of outstanding
	// access tokens — allows multiple handles to a single OS thread (e.g.
	// duplicate). m_State is stored atomically to allow the thread itself to
	// mark RUNNING/SEALED from the thunk without a data race.

	struct alignas(64) RegistryEntry final {
		HANDLE                          m_OsHandle{ INVALID_HANDLE_VALUE };
		DWORD                           m_OsThreadId{ 0 };
		CORIUM_MAYBE_UNUSED uint32_t    m_Pad0{ 0 };
		uint64_t                        m_Generation{ 0 };
		Atomic::AtomicValue64<uint64_t> m_TokenMask{};
		Atomic::AtomicValue32<uint32_t> m_State{};
		uint32_t						m_NumaNode{};

		RegistryEntry() noexcept {
			m_State.store(static_cast<uint32_t>(ThreadState::REAPED), MemoryOrder::RELAXED);
		}

		size_t allocateToken() noexcept {
			for (;;) {
				uint64_t mask = m_TokenMask.load(MemoryOrder::RELAXED);
				uint64_t freeMask = ~mask;
				if (freeMask == 0u) return INVALID_SLOT;

				unsigned long bit = 0;
#if CORIUM_COMPILER_MSVC
				_BitScanForward64(&bit, freeMask);
#else
				// TODO: Linux (GCC/Clang __builtin_ctzll) implementation
#endif

				uint64_t desired = mask | (1ull << bit);
				uint64_t previous = mask;
				CORIUM_UNUSED(m_TokenMask.compareExchange(
					&mask, desired,
					MemoryOrder::ACQ_REL, MemoryOrder::ACQUIRE));
				if (mask == previous) return static_cast<size_t>(bit);
			}
		}

		void releaseToken(size_t v_Token) noexcept {
			uint64_t bit = 1ull << v_Token;
			for (;;) {
				uint64_t mask = m_TokenMask.load(MemoryOrder::RELAXED);
				uint64_t desired = mask & ~bit;
				uint64_t previous = mask;
				CORIUM_UNUSED(m_TokenMask.compareExchange(
					&mask, desired,
					MemoryOrder::ACQ_REL, MemoryOrder::ACQUIRE));
				if (mask == previous) return;
			}
		}

		bool hasOutstandingTokens() const noexcept {
			return m_TokenMask.load(MemoryOrder::ACQUIRE) != 0u;
		}

		bool hasToken(size_t v_Token) const noexcept {
			if (v_Token >= 64u) return false;
			const uint64_t bit = 1ull << v_Token;
			return (m_TokenMask.load(MemoryOrder::ACQUIRE) & bit) != 0u;
		}

		ThreadState state() const noexcept {
			return static_cast<ThreadState>(m_State.load(MemoryOrder::ACQUIRE));
		}

		void setState(ThreadState v_State) noexcept {
			m_State.store(static_cast<uint32_t>(v_State), MemoryOrder::RELEASE);
		}
	};

	CORIUM_STATIC_ASSERT(sizeof(RegistryEntry) <= 64,
		"RegistryEntry must fit in one cache line");

	namespace {
		// Global thread registry — process-lifetime, zero-initialized.

		RegistryEntry g_Registry[MAX_CORIUM_THREADS];
		size_t findFreeSlot() noexcept {
			for (size_t i = 0; i < MAX_CORIUM_THREADS; ++i) {
				if (g_Registry[i].state() == ThreadState::REAPED &&
					!g_Registry[i].hasOutstandingTokens()) {
					return i;
				}
			}
			return INVALID_SLOT;
		}
	}

	bool NativeThread::isValidHandle(const ThreadHandle& ro_Handle) noexcept {
		const size_t slot = ro_Handle.m_ThreadId;
		if (slot >= MAX_CORIUM_THREADS) return false;
		const RegistryEntry& entry = g_Registry[slot];
		if (entry.m_Generation != ro_Handle.m_Generation) return false;
		if (entry.state() == ThreadState::REAPED) return false;
		if (!entry.hasToken(ro_Handle.m_AccessToken)) return false;
		return true;
	}

	// Internal::ThreadLaunchHelper
	//
	// Forward-declared in ThreadUtils.h, defined here. Owns the Win32 thunk
	// so that DWORD/WINAPI never appear in the public header.
	// Friended by ThreadHandle for private constructor access.

	namespace {
		// Set by attachThreadState, consumed by detachThreadState on the same thread.
		thread_local Memory::VirtualSegment* t_TlsSegment = nullptr;
		thread_local Memory::Allocators::GeneralAllocator* t_StateAllocator = nullptr;
	}

	namespace Internal {
		struct ThreadLaunchHelper final {
			static DWORD WINAPI WinThreadThunk(void* p_Raw) noexcept;

			// Per-thread state every Corium thread needs; runs on the thread itself. Shared by factory
			// threads (the thunk) and, later, adopted foreign threads (this_thread::attach).
			static void attachThreadState(size_t v_Slot, size_t v_TlsSize, Memory::Allocators::GeneralAllocator* p_Allocator) noexcept;
			static void detachThreadState(size_t v_Slot) noexcept;
		};
	}

	void Internal::ThreadLaunchHelper::attachThreadState(size_t v_Slot, size_t v_TlsSize,
		Memory::Allocators::GeneralAllocator* p_Allocator) noexcept {
		RegistryEntry& entry = g_Registry[v_Slot];
		entry.setState(ThreadState::RUNNING);

		this_thread::t_Handle = ThreadHandle(v_Slot, entry.m_Generation, 0u, ThreadState::RUNNING);
		this_thread::t_Permit = ParkHandle{ 0u };

		const uint32_t numaNode = entry.m_NumaNode;
		void* tlsMemory = Memory::Internal::AtomicAllocators::instance()
			.s_TlsAllocator[numaNode].load(MemoryOrder::ACQUIRE)->allocate(v_TlsSize);

		void* tlsSegmentMemory = p_Allocator->allocate(sizeof(Memory::VirtualSegment), alignof(Memory::VirtualSegment));
		t_TlsSegment = p_Allocator->emplace<Memory::VirtualSegment>(
			tlsSegmentMemory, tlsMemory, v_TlsSize, 0u, static_cast<uint8_t>(numaNode));
		t_StateAllocator = p_Allocator;
		this_thread::t_ThreadLocalAllocator.init(t_TlsSegment);

		// The context switch saves registers at handle + 64, so the native context needs handle + blob
		// contiguous even though it runs on the OS stack.
		void* nativeContext = this_thread::t_ThreadLocalAllocator.allocate(Frame::kReservedHeaderSize, 64);
		CORIUM_ASSERT(nativeContext != nullptr);
		Frame::Internal::bindNativeContext(nativeContext, static_cast<uint8_t>(numaNode));
	}

	void Internal::ThreadLaunchHelper::detachThreadState(size_t v_Slot) noexcept {
		// Native context storage lives in the TLS arena, so only the pointers need clearing.
		Frame::Internal::unbindNativeContext();
		g_Registry[v_Slot].setState(ThreadState::SEALED);

		t_StateAllocator->deallocate(t_TlsSegment, sizeof(Memory::VirtualSegment));
		t_TlsSegment = nullptr;
		t_StateAllocator = nullptr;
	}

	DWORD WINAPI Internal::ThreadLaunchHelper::WinThreadThunk(void* p_Raw) noexcept {
#ifdef _WIN32
		auto* context = static_cast<LaunchContext*>(p_Raw);

		attachThreadState(context->m_Slot, context->m_TlsSize, context->m_Allocator);
		context->m_Closure();
		detachThreadState(context->m_Slot);

		auto* allocator = context->m_Allocator;
		context->~LaunchContext();
		allocator->deallocate(context, sizeof(LaunchContext));
		return 0;
#else
		// TODO: Linux implementation
		return 0;
#endif
	}

	// NativeThread — method implementations

	ThreadHandle NativeThread::createThread(ThreadLaunchDesc&& u_LaunchDesc, const ThreadAttrDesc& ro_ExecDesc) noexcept {
		CORIUM_ASSERT(u_LaunchDesc.m_State == DescriptorState::FROZEN
			&& "ThreadLaunchDesc must be validated (call validate()) before createThread");
		CORIUM_ASSERT(ro_ExecDesc.m_State == DescriptorState::FROZEN
			&& "ThreadAttrDesc must be validated (call validate()) before createThread");

		const size_t slot = findFreeSlot();
		if (slot == INVALID_SLOT) return ThreadHandle::getInvalidThread();

		RegistryEntry& entry = g_Registry[slot];

		const size_t token = entry.allocateToken();
		if (token == INVALID_SLOT) return ThreadHandle::getInvalidThread();

		auto* allocator = &Memory::Internal::AllocatorRegistry::s_GeneralAllocator[0];

		void* contextMemory = allocator->allocate(sizeof(LaunchContext), alignof(LaunchContext));
		auto* context = contextMemory ? allocator->emplace<LaunchContext>(
			contextMemory,
			std::move(u_LaunchDesc.m_StartPoint),
			allocator,
			slot,
			u_LaunchDesc.m_VaSize) : nullptr;

		if (!context) {
			entry.releaseToken(token);
			return ThreadHandle::getInvalidThread();
		}

#ifdef _WIN32
		const DWORD createFlags = u_LaunchDesc.m_IsPreSuspended ? CREATE_SUSPENDED : 0u;

		const uint32_t attrCount = (ro_ExecDesc.m_Mask != 0 ? 1u : 0u)
			+ (ro_ExecDesc.m_SupportsIdealProcessor ? 1u : 0u);

		LPPROC_THREAD_ATTRIBUTE_LIST attrList = nullptr;
		LPVOID attrListBuf = nullptr;

		if (attrCount > 0) {
			SIZE_T attrListSize = 0;
			InitializeProcThreadAttributeList(nullptr, attrCount, 0, &attrListSize);
			attrListBuf = HeapAlloc(GetProcessHeap(), 0, attrListSize);
			if (!attrListBuf) {
				context->~LaunchContext();
				allocator->deallocate(context, sizeof(LaunchContext));
				entry.releaseToken(token);
				return ThreadHandle::getInvalidThread();
			}
			attrList = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrListBuf);
			InitializeProcThreadAttributeList(attrList, attrCount, 0, &attrListSize);

			if (ro_ExecDesc.m_Mask != 0) {
				GROUP_AFFINITY groupAffinity{};
				groupAffinity.Mask = ro_ExecDesc.m_Mask;
				groupAffinity.Group = static_cast<WORD>(ro_ExecDesc.m_GroupId);
				UpdateProcThreadAttribute(attrList, 0, PROC_THREAD_ATTRIBUTE_GROUP_AFFINITY,
					&groupAffinity, sizeof(groupAffinity), nullptr, nullptr);
			}
			if (ro_ExecDesc.m_SupportsIdealProcessor) {
				PROCESSOR_NUMBER procNum{};
				procNum.Group = static_cast<WORD>(ro_ExecDesc.m_GroupId);
				procNum.Number = static_cast<BYTE>(ro_ExecDesc.m_IdealProcessor);
				UpdateProcThreadAttribute(attrList, 0, PROC_THREAD_ATTRIBUTE_IDEAL_PROCESSOR,
					&procNum, sizeof(procNum), nullptr, nullptr);
			}
		}

		DWORD  osThreadId = 0;
		HANDLE osHandle = CreateRemoteThreadEx(
			GetCurrentProcess(),
			nullptr,
			CORIUM_DEFAULT_OS_TLS_SIZE * 1_MiB,
			Internal::ThreadLaunchHelper::WinThreadThunk,
			context,
			createFlags,
			attrList,
			&osThreadId);

		if (attrList) {
			DeleteProcThreadAttributeList(attrList);
			HeapFree(GetProcessHeap(), 0, attrListBuf);
		}

		if (!osHandle || osHandle == INVALID_HANDLE_VALUE) {
			context->~LaunchContext();
			allocator->deallocate(context, sizeof(LaunchContext));
			entry.releaseToken(token);
			return ThreadHandle::getInvalidThread();
		}

		// Thread priority.
		int winPriority = Corium::Internal::toWin32Priority(ro_ExecDesc.m_ThreadPriority);
		SetThreadPriority(osHandle, winPriority);

		// Thread name (debugger-visible).
		if (u_LaunchDesc.m_Name && u_LaunchDesc.m_Name[0] != '\0') {
			wchar_t wideName[256] = {};
			for (size_t i = 0; i < 255 && u_LaunchDesc.m_Name[i]; ++i)
				wideName[i] = static_cast<wchar_t>(u_LaunchDesc.m_Name[i]);
			SetThreadDescription(osHandle, wideName);
		}

		// Commit registry entry.
		entry.m_OsHandle = osHandle;
		entry.m_OsThreadId = osThreadId;
		entry.m_NumaNode = ro_ExecDesc.m_NumaNode;
		++entry.m_Generation;
		entry.setState(ThreadState::CREATED);

		// Perfrom cleanup

#else
		// TODO: Linux (pthreads) implementation
		context->~LaunchContext();
		allocator->deallocate(context, sizeof(LaunchContext));
		entry.releaseToken(token);
		return ThreadHandle::getInvalidThread();
#endif

		// TODO Kill me for this TLS upgrade
		return { slot, entry.m_Generation, token, ThreadState::CREATED };
	}

	bool NativeThread::detachThread(const ThreadHandle& ro_Handle) noexcept {
		if (!isValidHandle(ro_Handle)) return false;
		RegistryEntry& entry = g_Registry[ro_Handle.m_ThreadId];

		entry.releaseToken(ro_Handle.m_AccessToken);

		// If no other handles reference this slot, close the OS handle.
		if (!entry.hasOutstandingTokens()) {
#ifdef _WIN32
			CloseHandle(entry.m_OsHandle);
			entry.m_OsHandle = INVALID_HANDLE_VALUE;
			entry.m_OsThreadId = 0;
			// State stays as-is — the thread may still be running.
#else
			// TODO: Linux implementation
#endif
		}
		return true;
	}

	bool NativeThread::closeHandle(const ThreadHandle& ro_Handle) noexcept {
		if (!isValidHandle(ro_Handle)) return false;
		RegistryEntry& entry = g_Registry[ro_Handle.m_ThreadId];

		entry.releaseToken(ro_Handle.m_AccessToken);

		if (!entry.hasOutstandingTokens()) {
#ifdef _WIN32
			if (entry.m_OsHandle != INVALID_HANDLE_VALUE) {
				CloseHandle(entry.m_OsHandle);
				entry.m_OsHandle = INVALID_HANDLE_VALUE;
				entry.m_OsThreadId = 0;
			}
#else
			// TODO: Linux implementation
#endif
			entry.setState(ThreadState::REAPED);
		}
		return true;
	}

	ThreadHandle NativeThread::duplicateHandle(const ThreadHandle& ro_Handle) noexcept {
		if (!isValidHandle(ro_Handle)) return ThreadHandle::getInvalidThread();
		RegistryEntry& entry = g_Registry[ro_Handle.m_ThreadId];

		const size_t newToken = entry.allocateToken();
		if (newToken == INVALID_SLOT) return ThreadHandle::getInvalidThread();

		return { ro_Handle.m_ThreadId, entry.m_Generation,newToken, entry.state() };
	}

	bool NativeThread::isAlive(const ThreadHandle& ro_Handle) noexcept {
		if (!isValidHandle(ro_Handle)) return false;
#ifdef _WIN32
		const RegistryEntry& entry = g_Registry[ro_Handle.m_ThreadId];
		if (entry.m_OsHandle == INVALID_HANDLE_VALUE) return false;

		DWORD exitCode = 0;
		if (!GetExitCodeThread(entry.m_OsHandle, &exitCode)) return false;
		return exitCode == STILL_ACTIVE;
#else
		// TODO: Linux implementation
		return false;
#endif
	}

	ProcessorIdx NativeThread::getThreadID(const ThreadHandle& ro_Handle) noexcept {
		if (!isValidHandle(ro_Handle)) return 0;
#ifdef _WIN32
		return static_cast<ProcessorIdx>(g_Registry[ro_Handle.m_ThreadId].m_OsThreadId);
#else
		// TODO: Linux implementation
		return 0;
#endif
	}

	bool NativeThread::suspendThread(const ThreadHandle& ro_Handle) noexcept {
		if (!isValidHandle(ro_Handle)) return false;
#ifdef _WIN32
		const HANDLE osHandle = g_Registry[ro_Handle.m_ThreadId].m_OsHandle;
		if (osHandle == INVALID_HANDLE_VALUE) return false;
		return ::SuspendThread(osHandle) != static_cast<DWORD>(-1);
#else
		// TODO: Linux implementation
		return false;
#endif
	}

	bool NativeThread::resumeThread(const ThreadHandle& ro_Handle) noexcept {
		if (!isValidHandle(ro_Handle)) return false;
#ifdef _WIN32
		const HANDLE osHandle = g_Registry[ro_Handle.m_ThreadId].m_OsHandle;
		if (osHandle == INVALID_HANDLE_VALUE) return false;
		return ::ResumeThread(osHandle) != static_cast<DWORD>(-1);
#else
		// TODO: Linux implementation
		return false;
#endif
	}

	bool NativeThread::terminateThread(const ThreadHandle& ro_Handle) noexcept {
		if (!isValidHandle(ro_Handle)) return false;
#ifdef _WIN32
		RegistryEntry& entry = g_Registry[ro_Handle.m_ThreadId];
		if (entry.m_OsHandle == INVALID_HANDLE_VALUE) return false;

		const bool ok = TerminateThread(entry.m_OsHandle, 0) != FALSE;
		if (ok) entry.setState(ThreadState::SEALED);
		return ok;
#else
		// TODO: Linux implementation
		return false;
#endif
	}

	bool NativeThread::joinThread(const ThreadHandle& ro_Handle) noexcept {
		if (!isValidHandle(ro_Handle)) return false;
#ifdef _WIN32
		const HANDLE osHandle = g_Registry[ro_Handle.m_ThreadId].m_OsHandle;
		if (osHandle == INVALID_HANDLE_VALUE) return false;
		return WaitForSingleObject(osHandle, INFINITE) == WAIT_OBJECT_0;
#else
		// TODO: Linux implementation
		return false;
#endif
	}

	uint32_t NativeThread::getNumaNode(const ThreadHandle& ro_Handle) noexcept {
		if (!isValidHandle(ro_Handle)) return UINT32_MAX;
		return g_Registry[ro_Handle.m_ThreadId].m_NumaNode;
	}

	void NativeThread::waitOnAddress(ParkingSupport& ro_Support, uint32_t expected) noexcept {
#ifdef _WIN32
		WaitOnAddress(
			ro_Support.data(),
			&expected,
			sizeof(uint32_t),
			INFINITE
		);
#else
		// futex(..., FUTEX_WAIT, expected, ...)
#endif
	}

	void NativeThread::wakeOnAddress(ParkingSupport& ro_Support) noexcept {
#ifdef _WIN32
		// No value to store here - caller already mutated the real state; this is a pure notify.
		WakeByAddressSingle(ro_Support.data());
#else
		// TODO: Linux (futex) implementation
#endif
	}

	void NativeThread::wakeAllOnAddress(ParkingSupport& ro_Support) noexcept {
#ifdef _WIN32
		WakeByAddressAll(ro_Support.data());
#else
		// TODO: Linux (futex) implementation
#endif
	}

	void NativeThread::waitOnAddressFor(ParkingSupport& ro_Support, Chrono::Instant v_Deadline) noexcept {
#ifdef _WIN32
		const int64_t milliseconds = v_Deadline.remainingMilliseconds();
		const DWORD timeout = (milliseconds <= 0) ? 0u
			: static_cast<DWORD>(milliseconds < 0xFFFFFFFELL ? milliseconds : 0xFFFFFFFEu);
		uint32_t expected = 0u;
		WaitOnAddress(ro_Support.data(), &expected, sizeof(uint32_t), timeout);
#else
		// TODO: Linux futex with timeout
		(void) ro_Support;
		(void) v_Deadline;
#endif
	}
}