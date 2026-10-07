#include "Corium.h"
#include "CoriumMemoryHandler.h"
#include "CoriumUtility.h"

namespace Corium::Memory::Internal {

	bool     AllocatorRegistry::isRegistered = false;
	uint32_t AllocatorRegistry::s_NodeCount  = 0;

	// -------------------------------------------------------------------------
	// Runtime / Infrastructure
	// -------------------------------------------------------------------------

	VirtualSegment                    AllocatorRegistry::s_ClosureMemory[MAX_NUMA_NODES]{};
	Allocators::ClosureAllocator      AllocatorRegistry::s_ClosureAllocator[MAX_NUMA_NODES]{};

	VirtualSegment                    AllocatorRegistry::s_SmartPtrControlBlockMemory[MAX_NUMA_NODES]{};
	Allocators::ControlBlockAllocator AllocatorRegistry::s_ControlBlockAllocator[MAX_NUMA_NODES]{};

	VirtualSegment                    AllocatorRegistry::s_RuntimeCoreObjectsMemory[MAX_NUMA_NODES]{};
	Allocators::GeneralAllocator      AllocatorRegistry::s_GeneralAllocator[MAX_NUMA_NODES]{};

	VirtualSegment                    AllocatorRegistry::s_FrameStorageMemory[MAX_NUMA_NODES]{};
	Allocators::GeneralAllocator      AllocatorRegistry::s_FrameAllocator[MAX_NUMA_NODES]{};

	// -------------------------------------------------------------------------
	// Task Metadata - Object Locations
	// -------------------------------------------------------------------------

	VirtualSegment                    AllocatorRegistry::s_TaskMemoryDescMemory[MAX_NUMA_NODES]{};
	Allocators::TaskMetadataAllocator AllocatorRegistry::s_TaskMemoryDescAllocator[MAX_NUMA_NODES]{};

	VirtualSegment                    AllocatorRegistry::s_TaskMemoryHeaderMemory[MAX_NUMA_NODES]{};
	Allocators::TaskMetadataAllocator AllocatorRegistry::s_TaskMemoryHeaderAllocator[MAX_NUMA_NODES]{};

	VirtualSegment                    AllocatorRegistry::s_TaskContextMemory[MAX_NUMA_NODES]{};
	Allocators::TaskMetadataAllocator AllocatorRegistry::s_TaskContextAllocator[MAX_NUMA_NODES]{};

	VirtualSegment                    AllocatorRegistry::s_TaskSliceContextMemory[MAX_NUMA_NODES]{};
	Allocators::TaskMetadataAllocator AllocatorRegistry::s_TaskSliceContextAllocator[MAX_NUMA_NODES]{};

	VirtualSegment                    AllocatorRegistry::s_GPUContextMemory[MAX_NUMA_NODES]{};
	Allocators::TaskMetadataAllocator AllocatorRegistry::s_GPUContextAllocator[MAX_NUMA_NODES]{};

	// -------------------------------------------------------------------------
	// Task Metadata - Input Layouts
	// -------------------------------------------------------------------------

	VirtualSegment                    AllocatorRegistry::s_InputSizeMemory[MAX_NUMA_NODES]{};
	Allocators::TaskMetadataAllocator AllocatorRegistry::s_InputSizeAllocator[MAX_NUMA_NODES]{};

	VirtualSegment                    AllocatorRegistry::s_InputAlignmentMemory[MAX_NUMA_NODES]{};
	Allocators::TaskMetadataAllocator AllocatorRegistry::s_InputAlignmentAllocator[MAX_NUMA_NODES]{};

	// -------------------------------------------------------------------------
	// Task Metadata - Output Layouts
	// -------------------------------------------------------------------------

	VirtualSegment                    AllocatorRegistry::s_OutputSizeMemory[MAX_NUMA_NODES]{};
	Allocators::TaskMetadataAllocator AllocatorRegistry::s_OutputSizeAllocator[MAX_NUMA_NODES]{};

	VirtualSegment                    AllocatorRegistry::s_OutputAlignmentMemory[MAX_NUMA_NODES]{};
	Allocators::TaskMetadataAllocator AllocatorRegistry::s_OutputAlignmentAllocator[MAX_NUMA_NODES]{};

	// -------------------------------------------------------------------------
	// Task Payload
	// -------------------------------------------------------------------------

	VirtualSegment                   AllocatorRegistry::s_TaskPayloadMemory[MAX_NUMA_NODES]{};
	Allocators::TaskPayloadAllocator AllocatorRegistry::s_TaskPayloadAllocator[MAX_NUMA_NODES]{};

	// ------------------------------------------------------------------------
	// Frame API and TLS allocators
	// ------------------------------------------------------------------------

	VirtualSegment	           	     AllocatorRegistry::s_TlsMemory[MAX_NUMA_NODES]{};
	Allocators::TlsAllocator         AllocatorRegistry::s_TlsMemoryAllocator[MAX_NUMA_NODES]{};

	AtomicAllocators& AtomicAllocators::instance() {
		static AtomicAllocators inst;
		return inst;
	}
}

namespace Corium::Core::Utils {
	Memory::Allocators::ClosureAllocator* closureAllocator(uint8_t v_Node) noexcept {
		using namespace Memory::Internal;
		const uint8_t node = v_Node < AllocatorRegistry::s_NodeCount ? v_Node : 0;
		return AtomicAllocators::instance().s_ClosureAllocator[node].load();
	}
}
