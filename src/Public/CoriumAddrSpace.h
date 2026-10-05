/*
* Copyright (c) 2025 StormWeaver
*
* This file is part of the Corium Multithreading API
*
* Licensed under the MIT License. You may obtain a copy of the License at
* https://opensource.org/licenses/MIT
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND...
*/
#pragma once
#include <Corium.h>
#include <CoriumMemory.h>

// CORIUM VIRTUAL ADDRESS SPACE — 512 GiB total, 4 NUMA nodes x 128 GiB/node.
// Each node owns an independent VA reservation via VirtualAllocExNuma, so
// physical pages committed later stay node-local. Layout is identical per
// node. Every region below is documented immediately above the extern(s)
// that back it, in on-disk (low -> high address) order — the box IS the
// layout doc, there is no separate diagram to keep in sync.
// BY-DEFAULT ALL THE THREAD TLS IS MANAGED BY CORIUM.

namespace Corium::Memory::Internal {
	using namespace Corium::Memory::Literals;

	// -------------------------------------------------------------------------
	// NUMA topology constants
	// -------------------------------------------------------------------------

	constexpr uint32_t MAX_NUMA_NODES = CORIUM_MAX_NUMA;          // maximum supported NUMA nodes
	constexpr Bytes    g_TotalVA = CORIUM_VA_ALLOCATION * 1_MiB;  // 512 GiB total VA budget
	constexpr Bytes    NodeVASize = g_TotalVA / MAX_NUMA_NODES;     // 128 GiB per node

	// -------------------------------------------------------------------------
	// Guard and section sizes
	// -------------------------------------------------------------------------

	constexpr Bytes NullGuardSize = Bytes{ 2_MiB };
	constexpr Bytes SectionGuardSize = Bytes{ 2_MiB };

	// -------------------------------------------------------------------------
	// Per-node task capacity and layout constants
	// -------------------------------------------------------------------------

	constexpr size_t   MaxTasks = 2'000'000;  // maximum concurrent tasks per node
	constexpr uint32_t ParamsPerTask = 8;

	// Per-node region sizes — proportionally scaled 4x from the original 32 GiB layout
	constexpr Bytes ThreadLocalStorageSize = Bytes{ (CORIUM_SPACE_TLS_TOTAL) * 1_MiB };
	CORIUM_STATIC_ASSERT(ThreadLocalStorageSize <= 10_GiB,
		"CORIUM_SPACE_TLS_TOTAL must not exceed 10 GiB (4 GiB default + 6 GiB max growth), "
		"to leave headroom in the per-node reserved/future VA slice");
	constexpr Bytes FrameStorageSize = Bytes{ 4_GiB };
	constexpr size_t FrameRegBlobSize = CORIUM_FRAME_REG_BLOB_SIZE;
	CORIUM_STATIC_ASSERT(FrameRegBlobSize % 16 == 0, "CORIUM_FRAME_REG_BLOB_SIZE must keep the frame stack 16-byte aligned");
	constexpr Bytes RuntimeVASize = Bytes{ 24_GiB };
	constexpr Bytes TaskMetadataVASize = Bytes{ 32_GiB };
	constexpr Bytes TaskPayloadVASize = Bytes{ 56_GiB };

	// Reserved VA per node — computed as whatever is left after all named regions and guards
	constexpr Bytes NodeReservedVASize = NodeVASize
		- NullGuardSize          // upper guard
		- RuntimeVASize
		- SectionGuardSize       // runtime guard
		- TaskMetadataVASize
		- SectionGuardSize       // task metadata guard
		- TaskPayloadVASize
		- SectionGuardSize       // task payload guard
		- ThreadLocalStorageSize
		- SectionGuardSize       // TLS guard
		- NullGuardSize;         // lower guard

	// -------------------------------------------------------------------------
	// Per-node base reservations — one independent VA block per NUMA node
	// -------------------------------------------------------------------------

	extern VirtualSegment g_NodeMemory[MAX_NUMA_NODES];

	// ===========================================================================
	// Top-level VA regions — indexed by NUMA node, in on-disk order  (DO NOT TOUCH!)
	// ===========================================================================

	// +----------------------------------------------------------------+
	// | UPPER NULL GUARD (2 MiB) — unmapped, catches overflow          |
	extern VARegion g_UpperNullGuard[MAX_NUMA_NODES];
	// +----------------------------------------------------------------+

	// +======================================================================+
	//   RUNTIME / INFRA VA  (~24 GiB)
	//   ClosureFunction objects, smart pointer control blocks,
	//   schedulers, executors, pools, global allocators
	extern VARegion g_RuntimeVA[MAX_NUMA_NODES];

	//   +------------------------------------------------------------+
	//   | ClosureRange — 3 ClosureFunctions/task (startup/body/end)  |
	//   | x 256B each                                                |
	extern VARegion g_ClosureRange[MAX_NUMA_NODES];
	//   +------------------------------------------------------------+

	//   +------------------------------------------------------------+
	//   | Guard (2 MiB)                                               |
	extern VARegion g_ClosureGuard[MAX_NUMA_NODES];
	//   +------------------------------------------------------------+

	//   +------------------------------------------------------------+
	//   | SmartPtrControlBlocks — 8 GiB, shared_ptr/intrusive blocks |
	extern VARegion g_SmartPtrControlBlocks[MAX_NUMA_NODES];
	//   +------------------------------------------------------------+

	//   +------------------------------------------------------------+
	//   | Guard (2 MiB)                                               |
	extern VARegion g_SmartPtrGuard[MAX_NUMA_NODES];
	//   +------------------------------------------------------------+

	//   +------------------------------------------------------------+
	//   | FrameStorage — 4 GiB, pool of 2048 x 2 MiB NativeFrame     |
	//   | blocks (header + inline execution stack per frame)        |
	extern VARegion g_FrameStorage[MAX_NUMA_NODES];
	//   +------------------------------------------------------------+

	//   +------------------------------------------------------------+
	//   | Guard (2 MiB)                                               |
	extern VARegion g_FrameStorageGuard[MAX_NUMA_NODES];
	//   +------------------------------------------------------------+

	//   +------------------------------------------------------------+
	//   | RuntimeCoreObjects — remaining ~10 GiB: schedulers,        |
	//   | executors, pools, global allocators                        |
	extern VARegion g_RuntimeCoreObjects[MAX_NUMA_NODES];
	//   +------------------------------------------------------------+

	// +======================================================================+

	// +----------------------------------------------------------------+
	// | RUNTIME GUARD (2 MiB)                                          |
	extern VARegion g_RuntimeGuard[MAX_NUMA_NODES];
	// +----------------------------------------------------------------+

	// +======================================================================+
	//   TASK METADATA VA  (~32 GiB)
	extern VARegion g_TaskMetadataVA[MAX_NUMA_NODES];

	//   +------------------------------------------------------------+
	//   | TaskObjectLocations — 12 GiB                                |
	//   | TaskMemoryDesc, TaskMemoryHeader, TaskContext,              |
	//   | TaskSliceContext, GPUContext                                |
	extern VARegion g_TaskObjectLocations[MAX_NUMA_NODES];

	//     +----------------------------------------------------------+
	//     | TaskMemoryDescRange — MaxTasks x sizeof(TaskMemoryDescHeader) |
	extern VARegion g_TaskMemoryDescRange[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | Guard (2 MiB)                                             |
	extern VARegion g_TaskMemoryDescGuard[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | TaskMemoryHeaderRange — MaxTasks x sizeof(TaskMemoryHeader) |
	extern VARegion g_TaskMemoryHeaderRange[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | Guard (2 MiB)                                             |
	extern VARegion g_TaskMemoryHeaderGuard[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | TaskContextRange — MaxTasks x sizeof(TaskContextHeader)  |
	extern VARegion g_TaskContextRange[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | Guard (2 MiB)                                             |
	extern VARegion g_TaskContextGuard[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | TaskSliceContextRange — MaxTasks x sizeof(TaskSliceContextHeader) |
	extern VARegion g_TaskSliceContextRange[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | Guard (2 MiB)                                             |
	extern VARegion g_TaskSliceContextGuard[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | GPUContextRange — MaxTasks x sizeof(GPUContextHeader)    |
	extern VARegion g_GPUContextRange[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | Guard (2 MiB)                                             |
	extern VARegion g_GPUContextGuard[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | ObjectLocationSpare — remainder of TaskObjectLocations   |
	extern VARegion g_ObjectLocationSpare[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//   +------------------------------------------------------------+

	//   +------------------------------------------------------------+
	//   | Guard (2 MiB)                                               |
	extern VARegion g_ObjectLocationsGuard[MAX_NUMA_NODES];
	//   +------------------------------------------------------------+

	//   +------------------------------------------------------------+
	//   | TaskInputLayouts — 12 GiB                                   |
	//   | Input size / alignment arrays                               |
	extern VARegion g_TaskInputLayouts[MAX_NUMA_NODES];

	//     +----------------------------------------------------------+
	//     | InputSizeArrays — MaxTasks x ParamsPerTask x sizeof(size_t) |
	extern VARegion g_InputSizeArrays[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | Guard (2 MiB)                                             |
	extern VARegion g_InputSizeGuard[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | InputAlignmentArrays — MaxTasks x ParamsPerTask x sizeof(size_t) |
	extern VARegion g_InputAlignmentArrays[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | Guard (2 MiB)                                             |
	extern VARegion g_InputAlignmentGuard[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | InputLayoutSpare — remainder of TaskInputLayouts         |
	extern VARegion g_InputLayoutSpare[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//   +------------------------------------------------------------+

	//   +------------------------------------------------------------+
	//   | Guard (2 MiB)                                               |
	extern VARegion g_InputLayoutsGuard[MAX_NUMA_NODES];
	//   +------------------------------------------------------------+

	//   +------------------------------------------------------------+
	//   | TaskOutputLayouts — remaining ~8 GiB                        |
	//   | Output size / alignment arrays                              |
	extern VARegion g_TaskOutputLayouts[MAX_NUMA_NODES];

	//     +----------------------------------------------------------+
	//     | OutputSizeArrays — MaxTasks x ParamsPerTask x sizeof(size_t) |
	extern VARegion g_OutputSizeArrays[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | Guard (2 MiB)                                             |
	extern VARegion g_OutputSizeGuard[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | OutputAlignmentArrays — MaxTasks x ParamsPerTask x sizeof(size_t) |
	extern VARegion g_OutputAlignmentArrays[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | Guard (2 MiB)                                             |
	extern VARegion g_OutputAlignmentGuard[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//     +----------------------------------------------------------+
	//     | OutputLayoutSpare — remainder of TaskOutputLayouts       |
	extern VARegion g_OutputLayoutSpare[MAX_NUMA_NODES];
	//     +----------------------------------------------------------+

	//   +------------------------------------------------------------+

	// +======================================================================+

	// +----------------------------------------------------------------+
	// | TASK METADATA GUARD (2 MiB)                                    |
	extern VARegion g_TaskMetadataGuard[MAX_NUMA_NODES];
	// +----------------------------------------------------------------+

	// +======================================================================+
	//   TASK PAYLOAD VA  (~56 GiB)
	extern VARegion g_TaskPayloadVA[MAX_NUMA_NODES];

	//   +------------------------------------------------------------+
	//   | TaskPayloadArena — entire TaskPayloadVA                    |
	//   | task payload buffers, input/output data, reductions,       |
	//   | GPU-visible payload                                         |
	extern VARegion g_TaskPayloadArena[MAX_NUMA_NODES];
	//   +------------------------------------------------------------+

	// +======================================================================+

	// +----------------------------------------------------------------+
	// | TASK PAYLOAD GUARD (2 MiB)                                     |
	extern VARegion g_TaskPayloadGuard[MAX_NUMA_NODES];
	// +----------------------------------------------------------------+

	// +----------------------------------------------------------------+
	// | CORIUM THREAD LOCAL STORAGE (4 GiB)                            |
	// | Corium-managed TLS, allocated per worker thread                |
	extern VARegion g_ThreadLocalStorage[MAX_NUMA_NODES];
	// +----------------------------------------------------------------+

	// +----------------------------------------------------------------+
	// | TLS GUARD (2 MiB)                                              |
	extern VARegion g_TLSGuard[MAX_NUMA_NODES];
	// +----------------------------------------------------------------+

	// +----------------------------------------------------------------+
	// | RESERVED / FUTURE VA (~12 GiB)                                 |
	// | GPU staging/DMA, NUMA-local staging, sanitizer memory, RDMA    |
	extern VARegion g_ReservedVA[MAX_NUMA_NODES];
	// +----------------------------------------------------------------+

	// +----------------------------------------------------------------+
	// | LOWER NULL GUARD (2 MiB) — unmapped, catches underflow         |
	extern VARegion g_LowerNullGuard[MAX_NUMA_NODES];
	// +----------------------------------------------------------------+

	// -------------------------------------------------------------------------
	// Typed-range sizing stubs — cache-line aligned, used only for sizeof()
	// -------------------------------------------------------------------------

	struct alignas(64) TaskMemoryHeader       final {};
	struct alignas(64) TaskMemoryDescHeader   final {};
	struct alignas(64) TaskContextHeader      final {};
	struct alignas(64) TaskSliceContextHeader final {};
	struct alignas(64) GPUContextHeader       final {};

	// Call this before anything else. Every subsystem depends on it.
	// v_NodeCount must be <= MAX_NUMA_NODES - VA is only reserved for real,
	// detected NUMA nodes, not every compile-time slot.
	CORIUM_RUNTIME_API bool init(uint32_t v_NodeCount);
}
