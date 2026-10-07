#include "Corium.h"

#include <atomic>

#include <CoriumMemory.h>
#define ALLOW_SYSCALL
#include <CoriumSyscalls.h>
#include <InternalUtils.h>

namespace {
	struct VmInfo {
		Corium::Memory::Bytes m_PageSize             = 0;
		Corium::Memory::Bytes m_AllocationGranularity = 0;
	};
	VmInfo g_VmInfo{};
	bool   g_IsVmInitialized = false;
}

namespace Corium::Memory {
	void VirtualMemory::init() {
		if (g_IsVmInitialized) return;
#ifdef _WIN32
		SYSTEM_INFO si{};
		GetSystemInfo(&si);
		g_VmInfo.m_PageSize             = static_cast<Bytes>(si.dwPageSize);
		g_VmInfo.m_AllocationGranularity = static_cast<Bytes>(si.dwAllocationGranularity);
#elif defined(__linux__)
		long ps = sysconf(_SC_PAGESIZE);
		g_VmInfo.m_PageSize             = ps > 0 ? static_cast<Bytes>(ps) : 4096u;
		g_VmInfo.m_AllocationGranularity = g_VmInfo.m_PageSize;
#endif
		g_IsVmInitialized = true;
	}

	Bytes VirtualMemory::alignToGranularity(Bytes v_Bytes) {
		CORIUM_ASSERT(g_IsVmInitialized);
		return (v_Bytes + g_VmInfo.m_AllocationGranularity - 1) & ~(g_VmInfo.m_AllocationGranularity - 1);
	}

	Bytes VirtualMemory::getAllocationGranularity() {
		CORIUM_ASSERT(g_IsVmInitialized);
		return g_VmInfo.m_AllocationGranularity;
	}

	VirtualSegment VirtualMemory::virtualAlloc(VirtualSegment& segment, Bytes v_Size, MemoryOperation v_Operation) {
#ifdef CORIUM_DEBUG
		CORIUM_ASSERT(v_Size > 0);
#endif
#ifdef _WIN32
		switch (v_Operation) {
		case MemoryOperation::Reserve:
			{
				if (segment.isValid()) return INVALID_SEGMENT;
				v_Size = alignToGranularity(v_Size);
				void* p = nullptr;
				// Route through VirtualAllocExNuma when a node is set so physical pages
				// are preferentially allocated on the correct NUMA node at commit time.
				if (segment.m_NumaNode != VirtualSegment::INVALID_NUMA_NODE) {
					p = VirtualAllocExNuma(GetCurrentProcess(), nullptr, v_Size,
					                       MEM_RESERVE, PAGE_NOACCESS, segment.m_NumaNode);
				} else {
					p = VirtualAlloc(nullptr, v_Size, MEM_RESERVE, PAGE_NOACCESS);
				}
				return p ? VirtualSegment{ p, v_Size, 0, segment.m_NumaNode } : INVALID_SEGMENT;
			}
		case MemoryOperation::Commit:
			{
				if (!segment.isValid())return INVALID_SEGMENT;
				v_Size = alignToPage(v_Size);

				// atomic_ref: m_CommittedSize can't be a real atomic (VirtualSegment is
				// copied/moved by value), but concurrent commits into the same arena race on it.
				std::atomic_ref<Bytes> committed(segment.m_CommittedSize);
				Bytes old = committed.load(std::memory_order_acquire);
				Bytes newCommitted;
				do {
					newCommitted = old + v_Size;
					if (newCommitted > segment.m_TotalSize) return INVALID_SEGMENT;
				} while (!committed.compare_exchange_weak(
					old, newCommitted, std::memory_order_acq_rel, std::memory_order_relaxed));

				void* p = VirtualAlloc(static_cast<std::byte*>(segment.m_Memory) + old, v_Size, MEM_COMMIT, PAGE_READWRITE);
				if (!p)return INVALID_SEGMENT;
				return segment;
			}
		case MemoryOperation::Decommit:
		case MemoryOperation::Free: return INVALID_SEGMENT;
		}
		CORIUM_UNREACHABLE();
#elif defined(__linux__)

		switch (v_Operation) {
		case MemoryOperation::Reserve:
			{
				// Linux: mbind/numa_alloc_onnode not implemented; node tag is carried
				// through for bookkeeping but physical affinity is not enforced.
				if (segment.isValid()) return INVALID_SEGMENT;
				v_Size = alignToGranularity(v_Size);
				void* p = mmap(nullptr, v_Size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
				if (p == MAP_FAILED) return INVALID_SEGMENT;
				return VirtualSegment{ p, v_Size, 0, segment.m_NumaNode };
			}

		case MemoryOperation::Commit:
			{
				if (!segment.isValid())return INVALID_SEGMENT;
				v_Size = alignToPage(v_Size);

				std::atomic_ref<Bytes> committed(segment.m_CommittedSize);
				Bytes old = committed.load(std::memory_order_acquire);
				Bytes newCommitted;
				do {
					newCommitted = old + v_Size;
					if (newCommitted > segment.m_TotalSize) return INVALID_SEGMENT;
				} while (!committed.compare_exchange_weak(
					old, newCommitted, std::memory_order_acq_rel, std::memory_order_relaxed));

				void* p = static_cast<std::byte*>(segment.m_Memory) + old;
				if (mprotect(p, v_Size, PROT_READ | PROT_WRITE) == -1)return INVALID_SEGMENT;
				return segment;
			}
		case MemoryOperation::Decommit:
		case MemoryOperation::Free: return INVALID_SEGMENT;
		}
		CORIUM_UNREACHABLE();
#else
		CORIUM_UNREACHABLE();
#endif
	}

	bool VirtualMemory::virtualFree(
		VirtualSegment& segment,
		Bytes v_Size,
		MemoryOperation v_Operation
	) {
		if (!segment.isValid()) return false;
#ifdef CORIUM_DEBUG
		CORIUM_ASSERT(v_Size > 0);
#endif
		v_Size = alignToPage(v_Size);

#ifdef _WIN32

		switch (v_Operation) {
		case MemoryOperation::Decommit:
			{
				if (v_Size > segment.m_CommittedSize)
					return false;

				void* p = static_cast<std::byte*>(segment.m_Memory)
					+ (segment.m_CommittedSize - v_Size);

				if (!VirtualFree(p, v_Size, MEM_DECOMMIT))
					return false;

				segment.m_CommittedSize -= v_Size;
				return true;
			}

		case MemoryOperation::Free:
			{
				bool ok = VirtualFree(segment.m_Memory, 0, MEM_RELEASE);
				segment = INVALID_SEGMENT;
				return ok;
			}

		case MemoryOperation::Commit:
		case MemoryOperation::Reserve: return false;
		}
		CORIUM_UNREACHABLE();

#elif defined(__linux__)

		switch (v_Operation) {
		case MemoryOperation::Decommit:
			{
				if (v_Size > segment.m_CommittedSize)
					return false;

				void* p = static_cast<std::byte*>(segment.m_Memory)
					+ (segment.m_CommittedSize - v_Size);

				if (mprotect(p, v_Size, PROT_NONE) == -1)
					return false;

				madvise(p, v_Size, MADV_DONTNEED);

				segment.m_CommittedSize -= v_Size;
				return true;
			}

		case MemoryOperation::Free:
			{
				bool ok = munmap(segment.m_Memory, segment.m_TotalSize) != -1;
				segment = INVALID_SEGMENT;
				return ok;
			}

		case MemoryOperation::Commit:
		case MemoryOperation::Reserve: return false;
		}
		CORIUM_UNREACHABLE();
#else
		CORIUM_UNREACHABLE();
#endif
	}

	bool VirtualMemory::protectMem(const VirtualSegment& segment) {
		if (!segment.isValid() || segment.m_CommittedSize != 0)
			return false;

#ifdef _WIN32
		DWORD old;
		return VirtualProtect(segment.m_Memory, segment.m_TotalSize, PAGE_NOACCESS, &old) != 0;
#elif defined(__linux__)
		return mprotect(segment.m_Memory, segment.m_TotalSize, PROT_NONE) == 0;
#else
		CORIUM_UNREACHABLE();
#endif
	}

	bool VirtualMemory::unprotectMem(const VirtualSegment& segment) {
#ifdef _WIN32
		DWORD old;
		return VirtualProtect(segment.m_Memory, segment.m_TotalSize, PAGE_READWRITE, &old) != 0;
#elif defined(__linux__)
		return mprotect(segment.m_Memory, segment.m_TotalSize, PROT_READ | PROT_WRITE) == 0;
#else
		return false;
#endif
	}

	bool VirtualMemory::lockMem(const VirtualSegment& segment) {
#ifdef _WIN32
		return VirtualLock(segment.m_Memory, segment.m_CommittedSize) != 0;
#elif defined(__linux__)
		return mlock(segment.m_Memory, segment.m_CommittedSize) == 0;
#else
		return false;
#endif
	}

	bool VirtualMemory::unlockMem(const VirtualSegment& segment) {
		if (!segment.isValid() || segment.m_CommittedSize == 0)
			return false;

#ifdef _WIN32
		return VirtualUnlock(segment.m_Memory, segment.m_CommittedSize);

#elif defined(__linux__)
		return munlock(segment.m_Memory, segment.m_CommittedSize) == 0;

#else
		CORIUM_UNREACHABLE();
#endif
	}

	MemState VirtualMemory::queryPage(const VirtualSegment& segment, Bytes v_Offset) {
#ifdef CORIUM_DEBUG
		CORIUM_ASSERT(v_Offset < segment.m_TotalSize);
#endif
#ifdef _WIN32
		MEMORY_BASIC_INFORMATION memInfo;
		void* offsetMem = static_cast<std::byte*>(segment.m_Memory) + v_Offset;
		VirtualQuery(offsetMem, &memInfo, sizeof(MEMORY_BASIC_INFORMATION));
		return Corium::Internal::fromWin32MemState(memInfo.State);
#elif defined(__linux__)
#if defined(CORIUM_DEBUG)
		CORIUM_UNREACHABLE(); // TODO: /proc/self/maps + mincore
#else
		return MemState::Freed;
#endif
#else
		CORIUM_UNREACHABLE();
#endif
	}
	bool VirtualMemory::commitPageIfNeeded(VirtualSegment& ro_Segment, size_t v_Offset) {
		if (!ro_Segment.isValid())	return false;
		switch (queryPage(ro_Segment, v_Offset)) {
		case MemState::Committed: return true;
		case MemState::Reserved:
			{
				// re-check: a racing committer may have already passed v_Offset since queryPage()
				const Bytes committed = std::atomic_ref<Bytes>(ro_Segment.m_CommittedSize)
					.load(std::memory_order_acquire);
				if (v_Offset <= committed) return true;
				return virtualAlloc(ro_Segment, v_Offset - committed, MemoryOperation::Commit).isValid();
			}
		case MemState::Freed: return false;
		}
		CORIUM_UNREACHABLE();
	}
}