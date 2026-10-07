#include <Corium.h>
#include <ThreadUtils.h>
#include "ThreadLocals.h"

namespace Corium::Core::this_thread {
	thread_local ThreadHandle t_Handle = ThreadHandle::getInvalidThread();
	thread_local ParkHandle t_Permit{ 0u };
	thread_local Memory::Allocators::TlsAllocator t_ThreadLocalAllocator{};

	Memory::Allocators::TlsAllocator& allocator() noexcept {
		return t_ThreadLocalAllocator;
	}

	ThreadHandle& currentHandle() noexcept {
		return t_Handle;
	}

	ParkHandle& currentPermit() noexcept {
		return t_Permit;
	}
}

namespace Corium::Core {
	using namespace Corium::Memory::Literals;

	void init(ThreadAttrDesc& ro_Desc) {
		if (ro_Desc.m_State == DescriptorState::FROZEN) return;
		ro_Desc.m_State = DescriptorState::UNINITIALIZED;
		ro_Desc.m_IdealProcessor = 0;
		ro_Desc.m_IsGuardPageEnabled = false;
		ro_Desc.m_Mask = 0;
		ro_Desc.m_SupportsIdealProcessor = false;
		ro_Desc.m_ThreadPriority = ThreadPriority::NORMAL;
		ro_Desc.m_isDetached = false;
		ro_Desc.m_GroupId = 0;
	}

	void setAffinity(ThreadAttrDesc& ro_Desc, AffinityMask v_Mask) {
		if (isFrozen(ro_Desc)) return;
		promoteMutable(ro_Desc);
		ro_Desc.m_Mask = v_Mask;
	}

	void setPriority(ThreadAttrDesc& ro_Desc, ThreadPriority v_Priority) {
		if (isFrozen(ro_Desc)) return;
		promoteMutable(ro_Desc);
		ro_Desc.m_ThreadPriority = v_Priority;
	}

	void setIdealProcessor(ThreadAttrDesc& ro_Desc, ProcessorIdx v_Idx) {
		if (isFrozen(ro_Desc)) return;
		promoteMutable(ro_Desc);
		ro_Desc.m_IdealProcessor = v_Idx;
	}

	void shouldSupportIdealProcessor(ThreadAttrDesc& ro_Desc, bool v_Permission) {
		if (isFrozen(ro_Desc)) return;
		promoteMutable(ro_Desc);
		ro_Desc.m_SupportsIdealProcessor = v_Permission;
	}

	void canDetach(ThreadAttrDesc& ro_Desc, bool v_Permission) {
		if (isFrozen(ro_Desc)) return;
		promoteMutable(ro_Desc);
		ro_Desc.m_isDetached = v_Permission;
	}

	void vaGuardEnabled(ThreadAttrDesc& ro_Desc, bool v_Permission) {
		if (isFrozen(ro_Desc)) return;
		promoteMutable(ro_Desc);
		ro_Desc.m_IsGuardPageEnabled = v_Permission;
	}

	void setNumaNode(ThreadAttrDesc& ro_Desc, uint32_t v_Node) {
		if (isFrozen(ro_Desc)) return;
		promoteMutable(ro_Desc);
		CORIUM_ASSERT(v_Node < CORIUM_MAX_NUMA);
		ro_Desc.m_NumaNode = v_Node;
	}

#ifdef _WIN32																		 
	void setThreadGroup(ThreadAttrDesc& ro_Desc, Dword v_GroupId) {
		ro_Desc.m_SupportsGroup = true;
		ro_Desc.m_GroupId = v_GroupId;
	}
#endif

	bool validate(ThreadAttrDesc& ro_Desc) {
		if (isFrozen(ro_Desc)) return false;
		if (ro_Desc.m_State == DescriptorState::UNINITIALIZED) return false;
		if (!ro_Desc.m_SupportsIdealProcessor && ro_Desc.m_IdealProcessor > 0) return false;
		if (!ro_Desc.m_SupportsGroup && ro_Desc.m_GroupId > 0) return false;
		ro_Desc.m_State = DescriptorState::FROZEN;
		return true;
	}

	void init(ThreadLaunchDesc& ro_Desc) {
		ro_Desc.m_IsPreSuspended = false;
		ro_Desc.m_Name = "";
		ro_Desc.m_State = DescriptorState::UNINITIALIZED;
		ro_Desc.m_VaSize = CORIUM_SPACE_TLS_MIN_SIZE * 1_MiB;
		ro_Desc.m_StartPoint = Closure<void()>([](){}, nullptr);
	}

	void attachLaunchAddr(ThreadLaunchDesc& ro_Desc, Closure<void()> v_Closure) {
		ro_Desc.m_StartPoint = std::move(v_Closure);
	}

	void setVaSize(ThreadLaunchDesc& ro_Desc, size_t v_VaSize) {
		// Create Corium Space TLS
		CORIUM_DEBUG_ASSERT(v_VaSize >= CORIUM_SPACE_TLS_MIN_SIZE * 1_MiB &&
							v_VaSize <= CORIUM_SPACE_TLS_MAX_SIZE * 1_MiB);

		ro_Desc.m_VaSize = v_VaSize;
	}

	void setName(ThreadLaunchDesc& ro_Desc, const char* p_Name) {
		ro_Desc.m_Name = p_Name;
	}

	void isPreSuspended(ThreadLaunchDesc& ro_Desc, Flag v_Permission) {
		ro_Desc.m_IsPreSuspended = v_Permission;
	}

	bool validate(ThreadLaunchDesc& ro_Desc) {
		if (ro_Desc.m_State == DescriptorState::FROZEN) return false;
		if (!ro_Desc.m_StartPoint.isCallable()) return false;
		if (ro_Desc.m_VaSize < CORIUM_SPACE_TLS_MIN_SIZE * 1_MiB) return false;
		if (ro_Desc.m_VaSize > CORIUM_SPACE_TLS_MAX_SIZE * 1_MiB) return false;
		ro_Desc.m_State = DescriptorState::FROZEN;
		return true;
	}
} 