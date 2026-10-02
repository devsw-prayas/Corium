#pragma once
#include "CoriumCompiler.h"
#include "CoriumMemory.h"
#include "CoriumUtility.h"
#include "ThreadUtils.h"

namespace Corium::Core::Frame {
	using namespace Corium::Memory::Literals;

	inline constexpr size_t kReservedHeaderSize = 320;
	inline constexpr size_t kFrameStackSize = (CORIUM_DEFAULT_FRAME_STACK_SIZE) * 1_MiB;
	inline constexpr size_t kFrameMaxStackSize = 8_MiB;
	inline constexpr size_t kFrameMinStackSize = 64_KiB;

	CORIUM_STATIC_ASSERT(kFrameMaxStackSize > kFrameStackSize, "Invalid Default Frame Stack size. Stack"
		"size must be less than 8MiB");

	enum class CORIUM_RUNTIME_API FrameState : uint8_t {
		READY, SUSPENDED, TERMINATED
	};

	enum class CORIUM_RUNTIME_API Provenance : uint8_t {
		NUMA_GLOBAL,
		CALLEE_OWNED
	};

	// Identity is the address: stack offsets and m_RegBlob are computed against it, so no copy/move/operator&.
	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(64) FrameHandle final {
		Utils::FunctionView<void(void*)> m_Entry;
		void*    m_StackPtr;
		size_t   m_StackSize;
		void*    m_RegBlob;	 	
		uint8_t  m_Origin;
		uint8_t  m_NumaNode;
		FrameState m_State;
		uint8_t  m_Reserved[21]; // 43B of fields above + 21B here = 64B (one cache line)

		FrameHandle() = default;

		FrameHandle(const FrameHandle&) = delete;
		FrameHandle& operator=(const FrameHandle&) = delete;

		FrameHandle(FrameHandle&&) = delete;
		FrameHandle& operator=(FrameHandle&&) = delete;

		FrameHandle* operator&() = delete;
		const FrameHandle* operator&() const = delete;

		~FrameHandle() = default;
	};
	CORIUM_STATIC_ASSERT(sizeof(FrameHandle) == 64, "FrameHandle must be exactly one cache line");
	CORIUM_STATIC_ASSERT(alignof(FrameHandle) == 64, "FrameHandle must be aligned to a full cache line");

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(32) FrameStackDesc final {
		void* m_Memory;
		size_t m_Size;
		uint32_t m_NumaNode;
		Provenance m_Provenance;
		DescriptorState m_State = DescriptorState::UNINITIALIZED;
		Flag m_IsGuardPagesEnabled;

		FrameStackDesc() = default;
		~FrameStackDesc() = default;

		FrameStackDesc(const FrameStackDesc&) = default;
		FrameStackDesc& operator=(const FrameStackDesc&) = default;

		FrameStackDesc(FrameStackDesc&&) noexcept = default;
		FrameStackDesc& operator=(FrameStackDesc&&) noexcept = default;
	};

	void CORIUM_RUNTIME_API init(FrameStackDesc& ro_Desc) noexcept;
	void CORIUM_RUNTIME_API setMemoryLocation(FrameStackDesc& ro_Desc, void* p_Loc) noexcept;
	void CORIUM_RUNTIME_API setStackSize(FrameStackDesc& ro_Desc, size_t v_Size) noexcept;
	void CORIUM_RUNTIME_API setProvenance(FrameStackDesc& ro_Desc, Provenance v_Prov) noexcept;
	void CORIUM_RUNTIME_API setNumaNode(FrameStackDesc& ro_Desc, uint32_t v_Node) noexcept;
	void CORIUM_RUNTIME_API enableGuardPages(FrameStackDesc& ro_Desc, Flag v_Permission) noexcept;
	bool CORIUM_RUNTIME_API validate(FrameStackDesc& ro_Desc) noexcept;
}
