#pragma once
#include "CoriumAddrSpace.h"
#include "EngineAllocators.h"
#include "CoriumMemory.h"

namespace Corium::Memory::Internal {
	struct AllocatorRegistry final {
		~AllocatorRegistry() = delete;
		AllocatorRegistry(const AllocatorRegistry&) = delete;
		AllocatorRegistry(AllocatorRegistry&&) noexcept = delete;

		AllocatorRegistry& operator=(const AllocatorRegistry&) = delete;
		AllocatorRegistry& operator=(AllocatorRegistry&&) noexcept = delete;
		AllocatorRegistry() = delete;

		static bool     isRegistered;
		static uint32_t s_NodeCount;

		// ------------------------------------------------------------------------
		// Runtime / Infrastructure
		// ------------------------------------------------------------------------

		static VirtualSegment                   s_ClosureMemory[MAX_NUMA_NODES];
		static Allocators::ClosureAllocator     s_ClosureAllocator[MAX_NUMA_NODES];

		static VirtualSegment                   s_SmartPtrControlBlockMemory[MAX_NUMA_NODES];
		static Allocators::ControlBlockAllocator s_ControlBlockAllocator[MAX_NUMA_NODES];

		static VirtualSegment                   s_RuntimeCoreObjectsMemory[MAX_NUMA_NODES];
		static Allocators::GeneralAllocator     s_GeneralAllocator[MAX_NUMA_NODES];

		static VirtualSegment                   s_FrameStorageMemory[MAX_NUMA_NODES];
		static Allocators::GeneralAllocator     s_FrameAllocator[MAX_NUMA_NODES];

		// ------------------------------------------------------------------------
		// Task Metadata - Object Locations
		// ------------------------------------------------------------------------

		static VirtualSegment                    s_TaskMemoryDescMemory[MAX_NUMA_NODES];
		static Allocators::TaskMetadataAllocator s_TaskMemoryDescAllocator[MAX_NUMA_NODES];

		static VirtualSegment                    s_TaskMemoryHeaderMemory[MAX_NUMA_NODES];
		static Allocators::TaskMetadataAllocator s_TaskMemoryHeaderAllocator[MAX_NUMA_NODES];

		static VirtualSegment                    s_TaskContextMemory[MAX_NUMA_NODES];
		static Allocators::TaskMetadataAllocator s_TaskContextAllocator[MAX_NUMA_NODES];

		static VirtualSegment                    s_TaskSliceContextMemory[MAX_NUMA_NODES];
		static Allocators::TaskMetadataAllocator s_TaskSliceContextAllocator[MAX_NUMA_NODES];

		static VirtualSegment                    s_GPUContextMemory[MAX_NUMA_NODES];
		static Allocators::TaskMetadataAllocator s_GPUContextAllocator[MAX_NUMA_NODES];

		// ------------------------------------------------------------------------
		// Task Metadata - Input Layouts
		// ------------------------------------------------------------------------

		static VirtualSegment                    s_InputSizeMemory[MAX_NUMA_NODES];
		static Allocators::TaskMetadataAllocator s_InputSizeAllocator[MAX_NUMA_NODES];

		static VirtualSegment                    s_InputAlignmentMemory[MAX_NUMA_NODES];
		static Allocators::TaskMetadataAllocator s_InputAlignmentAllocator[MAX_NUMA_NODES];

		// ------------------------------------------------------------------------
		// Task Metadata - Output Layouts
		// ------------------------------------------------------------------------

		static VirtualSegment                    s_OutputSizeMemory[MAX_NUMA_NODES];
		static Allocators::TaskMetadataAllocator s_OutputSizeAllocator[MAX_NUMA_NODES];

		static VirtualSegment                    s_OutputAlignmentMemory[MAX_NUMA_NODES];
		static Allocators::TaskMetadataAllocator s_OutputAlignmentAllocator[MAX_NUMA_NODES];

		// ------------------------------------------------------------------------
		// Task Payload
		// ------------------------------------------------------------------------

		static VirtualSegment                   s_TaskPayloadMemory[MAX_NUMA_NODES];
		static Allocators::TaskPayloadAllocator s_TaskPayloadAllocator[MAX_NUMA_NODES];

		// ------------------------------------------------------------------------
		// Frame API and TLS allocators
		// ------------------------------------------------------------------------

		static VirtualSegment	           	    s_TlsMemory[MAX_NUMA_NODES];
		static Allocators::TlsAllocator s_TlsMemoryAllocator[MAX_NUMA_NODES];

		static bool initRegistry(uint32_t v_NodeCount) {
			if (isRegistered)
				return false;

			s_NodeCount = v_NodeCount;

			for (uint32_t node = 0; node < v_NodeCount; ++node) {
				const uint8_t numaNode = static_cast<uint8_t>(node);

				s_ClosureMemory[node] = createSegment(g_ClosureRange[node]);
				s_ClosureMemory[node].m_NumaNode = numaNode;
				s_ClosureAllocator[node].init(&s_ClosureMemory[node]);

				s_SmartPtrControlBlockMemory[node] = createSegment(g_SmartPtrControlBlocks[node]);
				s_SmartPtrControlBlockMemory[node].m_NumaNode = numaNode;
				s_ControlBlockAllocator[node].init(&s_SmartPtrControlBlockMemory[node]);

				s_RuntimeCoreObjectsMemory[node] = createSegment(g_RuntimeCoreObjects[node]);
				s_RuntimeCoreObjectsMemory[node].m_NumaNode = numaNode;
				s_GeneralAllocator[node].init(&s_RuntimeCoreObjectsMemory[node]);

				s_FrameStorageMemory[node] = createSegment(g_FrameStorage[node]);
				s_FrameStorageMemory[node].m_NumaNode = numaNode;
				s_FrameAllocator[node].init(&s_FrameStorageMemory[node]);

				s_TaskMemoryDescMemory[node] = createSegment(g_TaskMemoryDescRange[node]);
				s_TaskMemoryDescMemory[node].m_NumaNode = numaNode;
				s_TaskMemoryDescAllocator[node].init(&s_TaskMemoryDescMemory[node]);

				s_TaskMemoryHeaderMemory[node] = createSegment(g_TaskMemoryHeaderRange[node]);
				s_TaskMemoryHeaderMemory[node].m_NumaNode = numaNode;
				s_TaskMemoryHeaderAllocator[node].init(&s_TaskMemoryHeaderMemory[node]);

				s_TaskContextMemory[node] = createSegment(g_TaskContextRange[node]);
				s_TaskContextMemory[node].m_NumaNode = numaNode;
				s_TaskContextAllocator[node].init(&s_TaskContextMemory[node]);

				s_TaskSliceContextMemory[node] = createSegment(g_TaskSliceContextRange[node]);
				s_TaskSliceContextMemory[node].m_NumaNode = numaNode;
				s_TaskSliceContextAllocator[node].init(&s_TaskSliceContextMemory[node]);

				s_GPUContextMemory[node] = createSegment(g_GPUContextRange[node]);
				s_GPUContextMemory[node].m_NumaNode = numaNode;
				s_GPUContextAllocator[node].init(&s_GPUContextMemory[node]);

				s_InputSizeMemory[node] = createSegment(g_InputSizeArrays[node]);
				s_InputSizeMemory[node].m_NumaNode = numaNode;
				s_InputSizeAllocator[node].init(&s_InputSizeMemory[node]);

				s_InputAlignmentMemory[node] = createSegment(g_InputAlignmentArrays[node]);
				s_InputAlignmentMemory[node].m_NumaNode = numaNode;
				s_InputAlignmentAllocator[node].init(&s_InputAlignmentMemory[node]);

				s_OutputSizeMemory[node] = createSegment(g_OutputSizeArrays[node]);
				s_OutputSizeMemory[node].m_NumaNode = numaNode;
				s_OutputSizeAllocator[node].init(&s_OutputSizeMemory[node]);

				s_OutputAlignmentMemory[node] = createSegment(g_OutputAlignmentArrays[node]);
				s_OutputAlignmentMemory[node].m_NumaNode = numaNode;
				s_OutputAlignmentAllocator[node].init(&s_OutputAlignmentMemory[node]);

				s_TaskPayloadMemory[node] = createSegment(g_TaskPayloadArena[node]);
				s_TaskPayloadMemory[node].m_NumaNode = numaNode;
				s_TaskPayloadAllocator[node].init(&s_TaskPayloadMemory[node]);

				s_TlsMemory[node] = createSegment(g_ThreadLocalStorage[node]);
				s_TlsMemory[node].m_NumaNode = numaNode;
				s_TlsMemoryAllocator[node].init(&s_TlsMemory[node]);
			}

			isRegistered = true;
			return true;
		}
	};

	struct AtomicAllocators final {
		AtomicAllocators(const AtomicAllocators&) = delete;
		AtomicAllocators(AtomicAllocators&&) noexcept = delete;

		AtomicAllocators& operator=(const AtomicAllocators&) = delete;
		AtomicAllocators& operator=(AtomicAllocators&&) noexcept = delete;

		// ---------------------------------------------------------------------
		// Runtime / Infrastructure
		// ---------------------------------------------------------------------

		Core::Atomic::AtomicPointer<Allocators::ClosureAllocator>      s_ClosureAllocator[MAX_NUMA_NODES];
		Core::Atomic::AtomicPointer<Allocators::ControlBlockAllocator> s_ControlBlockAllocator[MAX_NUMA_NODES];
		Core::Atomic::AtomicPointer<Allocators::GeneralAllocator>      s_GeneralAllocator[MAX_NUMA_NODES];
		Core::Atomic::AtomicPointer<Allocators::GeneralAllocator>      s_FrameAllocator[MAX_NUMA_NODES];

		// ---------------------------------------------------------------------
		// Task Metadata - Object Locations
		// ---------------------------------------------------------------------

		Core::Atomic::AtomicPointer<Allocators::TaskMetadataAllocator> s_TaskMemoryDescAllocator[MAX_NUMA_NODES];
		Core::Atomic::AtomicPointer<Allocators::TaskMetadataAllocator> s_TaskMemoryHeaderAllocator[MAX_NUMA_NODES];
		Core::Atomic::AtomicPointer<Allocators::TaskMetadataAllocator> s_TaskContextAllocator[MAX_NUMA_NODES];
		Core::Atomic::AtomicPointer<Allocators::TaskMetadataAllocator> s_TaskSliceContextAllocator[MAX_NUMA_NODES];
		Core::Atomic::AtomicPointer<Allocators::TaskMetadataAllocator> s_GPUContextAllocator[MAX_NUMA_NODES];

		// ---------------------------------------------------------------------
		// Task Metadata - Input Layouts
		// ---------------------------------------------------------------------

		Core::Atomic::AtomicPointer<Allocators::TaskMetadataAllocator> s_InputSizeAllocator[MAX_NUMA_NODES];
		Core::Atomic::AtomicPointer<Allocators::TaskMetadataAllocator> s_InputAlignmentAllocator[MAX_NUMA_NODES];

		// ---------------------------------------------------------------------
		// Task Metadata - Output Layouts
		// ---------------------------------------------------------------------

		Core::Atomic::AtomicPointer<Allocators::TaskMetadataAllocator> s_OutputSizeAllocator[MAX_NUMA_NODES];
		Core::Atomic::AtomicPointer<Allocators::TaskMetadataAllocator> s_OutputAlignmentAllocator[MAX_NUMA_NODES];

		// ---------------------------------------------------------------------
		// Task Payload
		// ---------------------------------------------------------------------

		Core::Atomic::AtomicPointer<Allocators::TaskPayloadAllocator>  s_TaskPayloadAllocator[MAX_NUMA_NODES];

		// ---------------------------------------------------------------------
		// Frame API and TLS
		// ---------------------------------------------------------------------

		Core::Atomic::AtomicPointer<Allocators::TlsAllocator>          s_TlsAllocator[MAX_NUMA_NODES];

	private:
		AtomicAllocators() {
			for (uint32_t node = 0; node < AllocatorRegistry::s_NodeCount; ++node) {
				s_ClosureAllocator[node].store(
					&AllocatorRegistry::s_ClosureAllocator[node], Core::Atomics::MemoryOrder::RELAXED);
				s_ControlBlockAllocator[node].store(
					&AllocatorRegistry::s_ControlBlockAllocator[node], Core::Atomics::MemoryOrder::RELAXED);
				s_GeneralAllocator[node].store(
					&AllocatorRegistry::s_GeneralAllocator[node], Core::Atomics::MemoryOrder::RELAXED);
				s_FrameAllocator[node].store(
					&AllocatorRegistry::s_FrameAllocator[node], Core::Atomics::MemoryOrder::RELAXED);

				s_TaskMemoryDescAllocator[node].store(
					&AllocatorRegistry::s_TaskMemoryDescAllocator[node], Core::Atomics::MemoryOrder::RELAXED);
				s_TaskMemoryHeaderAllocator[node].store(
					&AllocatorRegistry::s_TaskMemoryHeaderAllocator[node], Core::Atomics::MemoryOrder::RELAXED);
				s_TaskContextAllocator[node].store(
					&AllocatorRegistry::s_TaskContextAllocator[node], Core::Atomics::MemoryOrder::RELAXED);
				s_TaskSliceContextAllocator[node].store(
					&AllocatorRegistry::s_TaskSliceContextAllocator[node], Core::Atomics::MemoryOrder::RELAXED);
				s_GPUContextAllocator[node].store(
					&AllocatorRegistry::s_GPUContextAllocator[node], Core::Atomics::MemoryOrder::RELAXED);

				s_InputSizeAllocator[node].store(
					&AllocatorRegistry::s_InputSizeAllocator[node], Core::Atomics::MemoryOrder::RELAXED);
				s_InputAlignmentAllocator[node].store(
					&AllocatorRegistry::s_InputAlignmentAllocator[node], Core::Atomics::MemoryOrder::RELAXED);

				s_OutputSizeAllocator[node].store(
					&AllocatorRegistry::s_OutputSizeAllocator[node], Core::Atomics::MemoryOrder::RELAXED);
				s_OutputAlignmentAllocator[node].store(
					&AllocatorRegistry::s_OutputAlignmentAllocator[node], Core::Atomics::MemoryOrder::RELAXED);

				s_TaskPayloadAllocator[node].store(
					&AllocatorRegistry::s_TaskPayloadAllocator[node], Core::Atomics::MemoryOrder::RELAXED);

				s_TlsAllocator[node].store(
					&AllocatorRegistry::s_TlsMemoryAllocator[node], Core::Atomics::MemoryOrder::RELAXED);
			}
		}

	public:
		static AtomicAllocators& instance();
	};
}
