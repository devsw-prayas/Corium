#include "Corium.h"
#include "EngineAllocators.h"
#include "CoriumMemory.h"

namespace Corium::Memory::Allocators {

	void* BumpAllocator::allocateImpl(size_t v_Bytes, size_t v_Align) noexcept {
		if (!m_Base || v_Bytes == 0) return nullptr;

		size_t old = m_Bump.load(Core::Atomics::MemoryOrder::RELAXED);

		while (true) {
			const size_t aligned = (old + v_Align - 1) & ~(v_Align - 1);
			const size_t newBump = aligned + v_Bytes;

			if (newBump > m_Size) return nullptr;

			const size_t observed = m_Bump.compareExchange(
				&old, newBump,
				Core::Atomics::MemoryOrder::ACQ_REL,
				Core::Atomics::MemoryOrder::RELAXED);

			if (observed == old) {
				// CAS succeeded — commit page if needed, then return user pointer
				if (!VirtualMemory::commitPageIfNeeded(*m_Base, newBump))
					return nullptr;

				return static_cast<uint8_t*>(m_Base->m_Memory) + aligned;
			}
			old = observed;
		}
	}

	void* BumpAllocator::allocateImpl(size_t v_Bytes) noexcept {
		return allocateImpl(v_Bytes, alignof(std::max_align_t));
	}

}
