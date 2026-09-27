#pragma once

#include "Corium.h"
#include "ThreadUtils.h"

namespace Corium::Core::Factory {
	class CORIUM_RUNTIME_API AThreadFactory {
	public:
		AThreadFactory();
		CORIUM_NODISCARD virtual ThreadHandle createAndStart(
			Closure<void()> r_Closure, const char* p_ThreadName
		) noexcept = 0;

		AThreadFactory(AThreadFactory&&) noexcept = default;
		AThreadFactory& operator=(AThreadFactory&&) = default;

		virtual ~AThreadFactory() = default;
	protected:
		ThreadAttrDesc m_AttrDec;
		ThreadLaunchDesc m_LaunchDesc;
		static void buildAttrDesc(
			AffinityMask v_Mask, ProcessorIdx v_Idx, ThreadPriority v_Priority, uint32_t v_NumaNode,
			Flag v_IsDetached, Flag v_SupportsIdealProcessor, Flag v_IsGuardsEnabled, ThreadAttrDesc& ro_Desc
		);
		static void buildLaunchDesc(size_t v_VaSize, Flag v_IsSuspended, ThreadLaunchDesc& ro_Desc);
	};

	class CORIUM_RUNTIME_API DefaultThreadFactory final : public AThreadFactory {
	public:
		DefaultThreadFactory() = default;
		ThreadHandle createAndStart(Closure<void()> r_Closure, const char* p_ThreadName) noexcept override;
	};

	class CORIUM_RUNTIME_API AffinityFactory : public AThreadFactory {
	public:
		AffinityFactory(uint32_t v_Node, AffinityMask v_Mask);
		ThreadHandle createAndStart(Closure<void()> r_Closure, const char* p_ThreadName) noexcept override;

	protected:
		uint32_t     m_Node;
		AffinityMask m_Mask;
	};

	class CORIUM_RUNTIME_API PriorityFactory final : public AffinityFactory {
	public:
		PriorityFactory(uint32_t v_Node, AffinityMask v_Mask, ThreadPriority v_Priority);
		ThreadHandle createAndStart(Closure<void()> r_Closure, const char* p_ThreadName) noexcept override;

	private:
		ThreadPriority m_Priority;
	};
}
