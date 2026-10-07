#include "CoriumRuntime.h"
#include "CoriumMemoryHandler.h"
#include "CoriumAddrSpace.h"
#include "CoriumEnvironment.h"

namespace Corium {
	namespace {
		bool s_IsInit = false;
	}

	void CoriumRuntime::initRuntime() {
		if (s_IsInit) return;

		// Probe CPU topology first -node count drives allocator init.
		Environment::EnvironmentProbe::init();

		const uint32_t detectedNodes = Environment::EnvironmentProbe::getCpuInfo().m_NumaNodeCount;
		const uint32_t rawNodes      = detectedNodes > 0 ? detectedNodes : 1u;
		const uint32_t nodeCount     = rawNodes < Memory::Internal::MAX_NUMA_NODES
		                             ? rawNodes
		                             : Memory::Internal::MAX_NUMA_NODES;

		// Reserve VA regions for the real, detected NUMA nodes only.
		Memory::Internal::init(nodeCount);

		// Wire allocators to their node VA regions.
		Memory::Internal::AllocatorRegistry::initRegistry(nodeCount);

		s_IsInit = true;
	}

	bool CoriumRuntime::isRuntimeInit() {
		return s_IsInit;
	}
}
