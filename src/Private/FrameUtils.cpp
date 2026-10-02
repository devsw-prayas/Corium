#include "Corium.h"
#include "FrameUtils.h"
#include "CoriumMemoryHandler.h"

namespace Corium::Core::Frame {
	namespace {
		CORIUM_FORCEINLINE Flag isFrozen(const FrameStackDesc& ro_Desc) {
			return ro_Desc.m_State == DescriptorState::FROZEN;
		}

		CORIUM_FORCEINLINE void promoteMutable(FrameStackDesc& r_Desc) {
			if (r_Desc.m_State == DescriptorState::UNINITIALIZED)
				r_Desc.m_State = DescriptorState::MUTABLE;
		}
	}

	void init(FrameStackDesc& ro_Desc) noexcept {
		ro_Desc.m_Memory = nullptr;
		ro_Desc.m_Size = kFrameStackSize;
		ro_Desc.m_Provenance = Provenance::NUMA_GLOBAL;
		ro_Desc.m_State = DescriptorState::MUTABLE;
		ro_Desc.m_NumaNode = 0;
		ro_Desc.m_IsGuardPagesEnabled = false;
	}

	void setMemoryLocation(FrameStackDesc& ro_Desc, void* p_Loc) noexcept {
		if (isFrozen(ro_Desc)) return;
		promoteMutable(ro_Desc);
		CORIUM_ASSERT(p_Loc != nullptr);
		ro_Desc.m_Memory = p_Loc;
	}

	void setStackSize(FrameStackDesc& ro_Desc, size_t v_Size) noexcept {
		if (isFrozen(ro_Desc)) return;
		promoteMutable(ro_Desc);
		CORIUM_DEBUG_ASSERT(v_Size >= kReservedHeaderSize + kFrameMinStackSize && v_Size <= kFrameMaxStackSize);
		ro_Desc.m_Size = v_Size;
	}

	void setProvenance(FrameStackDesc& ro_Desc, Provenance v_Prov) noexcept {
		if (isFrozen(ro_Desc)) return;
		promoteMutable(ro_Desc);
		ro_Desc.m_Provenance = v_Prov;
	}

	void setNumaNode(FrameStackDesc& ro_Desc, uint32_t v_Node) noexcept {
		if (isFrozen(ro_Desc)) return;
		promoteMutable(ro_Desc);
		CORIUM_ASSERT(v_Node < CORIUM_MAX_NUMA);
		ro_Desc.m_NumaNode = v_Node;
	}

	void enableGuardPages(FrameStackDesc& ro_Desc, Flag v_Permission) noexcept {
		if (isFrozen(ro_Desc)) return;
		promoteMutable(ro_Desc);
		ro_Desc.m_IsGuardPagesEnabled = v_Permission;
	}

	// Pure check: no allocation here, createFrame owns that, so one frozen desc can stamp out many frames.
	bool validate(FrameStackDesc& ro_Desc) noexcept {
		if (ro_Desc.m_State != DescriptorState::MUTABLE) return false;

		if (ro_Desc.m_Provenance == Provenance::NUMA_GLOBAL) {
			// g_FrameStorage hands out fixed kFrameStackSize blocks, so pooled frames can't pick their size.
			if (ro_Desc.m_Memory != nullptr) return false;
			if (ro_Desc.m_Size != kFrameStackSize) return false;
			if (ro_Desc.m_NumaNode >= Memory::Internal::AllocatorRegistry::s_NodeCount) return false;
		} else {
			if (ro_Desc.m_Memory == nullptr) return false;
			if ((reinterpret_cast<uintptr_t>(ro_Desc.m_Memory) & 63) != 0) return false;
			if (ro_Desc.m_Size < kReservedHeaderSize + kFrameMinStackSize) return false;
			if (ro_Desc.m_Size > kFrameMaxStackSize) return false;
			// The stack top (base + size) must keep RSP 16-byte aligned.
			if ((ro_Desc.m_Size & 15) != 0) return false;
			// Caller-owned memory has no VA headroom to carve guards out of.
			if (ro_Desc.m_IsGuardPagesEnabled) return false;
		}

		ro_Desc.m_State = DescriptorState::FROZEN;
		return true;
	}
}
