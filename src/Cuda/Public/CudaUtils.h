#pragma once

#include "CoriumCompiler.h"
#include "CoriumDiagnostics.h"

namespace Corium::Cuda::Utils {
	enum class CORIUM_RUNTIME_API CudaDeviceAttribute : uint8_t {
		COMPUTE_CAPABILITY_MAJOR,               // Major SM version (architecture generation)
		COMPUTE_CAPABILITY_MINOR,               // Minor SM version (architecture revision)

		MAX_THREADS_PER_BLOCK,                  // Maximum number of threads in a single block
		MAX_GRID_DIM_X,                         // Maximum grid dimension in X direction
		MAX_GRID_DIM_Y,                         // Maximum grid dimension in Y direction
		MAX_GRID_DIM_Z,                         // Maximum grid dimension in Z direction

		MAX_SHARED_MEMORY_PER_BLOCK,            // Maximum shared memory available per block (bytes)

		WARP_SIZE,                              // Number of threads per warp (typically 32)

		MEMORY_CLOCK_RATE,                      // Memory clock frequency (kHz)
		GLOBAL_MEMORY_BUS_WIDTH,                // Width of memory bus (bits)

		L2_CACHE_SIZE,                          // Size of L2 cache (bytes)

		UNIFIED_ADDRESSING,                     // Whether unified virtual addressing is supported (0/1)
		CONCURRENT_KERNELS,                     // Whether multiple kernels can execute concurrently (0/1)

		CAN_USE_HOST_POINTER_FOR_REGISTERED_MEM,// Whether host pointers can be directly used after registration (0/1)

		GPU_DIRECT_RDMA_SUPPORTED,              // Support for GPUDirect RDMA (0/1)
		VIRTUAL_MEMORY_MANAGEMENT_SUPPORTED,    // Support for CUDA virtual memory APIs (0/1)
		CONCURRENT_MANAGED_ACCESS               // GPU can access managed memory concurrently with CPU (0/1); false on Windows WDDM
	};

	enum class CORIUM_RUNTIME_API ContextSchedulingFlags : uint8_t {
		SCHEDULE_AUTO,
		SCHEDULE_SPIN,
		SCHEDULE_YIELD,
		SCHEDULE_BLOCKING_SYNC
	};

	enum class CORIUM_RUNTIME_API ContextCreationFlags : uint8_t {
		NONE,
		MAP_HOST,
		LMEM_RESIZE_TO_MAX
	};

	enum class CORIUM_RUNTIME_API HostAllocFlags : uint8_t {
		ALLOC_PORTABLE,
		ALLOC_DEVICE_MAP,
		ALLOC_WRITE_COMBINED
	};

	enum class CORIUM_RUNTIME_API HostRegisterFlags : uint8_t {
		REG_PORTABLE,
		REG_DEVICE_MAP,
		REG_IO_MEMORY,
		REG_READ_ONLY
	};

	enum class DeviceLocation : uint8_t {
		CPU, GPU
	};

	enum class CORIUM_RUNTIME_API AllocationType : uint8_t {
		INVALID,
		PINNED,
	};

	enum class CORIUM_RUNTIME_API AllocationHandleType : uint8_t {
		NONE,
		WIN32_HANDLE,
		FABRIC_HANDLE
	};

	enum class CORIUM_RUNTIME_API AccessFlagBits : uint8_t {
		NONE      = 0,
		READ      = 1 << 0,
		READWRITE = 1 << 1
	};

	enum class CORIUM_RUNTIME_API AllocationGranularityOption : uint8_t {
		MINIMUM,
		RECOMMENDED
	};

	enum class CORIUM_RUNTIME_API StreamFlags : uint8_t {
		DEFAULT,
		NON_BLOCKING
	};

	enum class CORIUM_RUNTIME_API EventFlags : uint8_t {
		DEFAULT        = 0,
		BLOCKING_SYNC  = 1 << 0,
		DISABLE_TIMING = 1 << 1,
		INTERPROCESS   = 1 << 2
	};

	enum class CORIUM_RUNTIME_API StreamCaptureMode : uint8_t {
		GLOBAL,
		THREAD_LOCAL,
		RELAXED
	};

	enum class CORIUM_RUNTIME_API StreamCaptureStatus : uint8_t {
		NONE,
		ACTIVE,
		INVALIDATED
	};

	enum class CORIUM_RUNTIME_API CopyMemoryType : uint8_t {
		HOST,
		DEVICE,
		ARRAY
	};

	class CORIUM_RUNTIME_API CudaHelpers final {
	public:
		static uint32_t computeAllocFlag(std::initializer_list<HostAllocFlags> flags);
		static uint32_t computeRegFlag(std::initializer_list<HostRegisterFlags> flags);
		static uint64_t computeAccessFlags(std::initializer_list<AccessFlagBits> flags);
		static uint32_t computeEventFlags(std::initializer_list<EventFlags> flags);
	};

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(4) DeviceHandle final {
		int m_HandleValue;

		bool isValid() const {
			return m_HandleValue > -1;
		}
		DeviceHandle() = default;
		~DeviceHandle() = default;

		DeviceHandle(const DeviceHandle&) = default;
		DeviceHandle& operator=(const DeviceHandle&) = default;

		DeviceHandle(DeviceHandle&&) noexcept = default;
		DeviceHandle& operator=(DeviceHandle&&) noexcept = default;

		static DeviceHandle makeCpu();
	};

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(16) DeviceUUID final {
		uint64_t m_Lo;
		uint64_t m_Hi;

		DeviceUUID() = default;
		~DeviceUUID() = default;

		DeviceUUID(const DeviceUUID&) = default;
		DeviceUUID& operator=(const DeviceUUID&) = default;

		DeviceUUID(DeviceUUID&&) noexcept = default;
		DeviceUUID& operator=(DeviceUUID&&) noexcept = default;
	};

	CORIUM_STATIC_ASSERT(sizeof(DeviceUUID) == 16, "Inavlid UUID Struct layout size, must be 16");
	CORIUM_STATIC_ASSERT(std::is_standard_layout_v<DeviceUUID>, "Invalid UUID struct layout, must maintain standard layout");
	CORIUM_STATIC_ASSERT(std::is_trivially_copyable_v<DeviceUUID>, "Invalid UUID struct members, must be trivial");
	CORIUM_STATIC_ASSERT(std::is_trivially_move_assignable_v<DeviceUUID>, "Invalid UUID struct members, must be trivial");

	using CtxPtr = void*;

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(8) CudaContext final {
		CtxPtr m_Handle;

		CudaContext() = default;
		~CudaContext() = default;

		CudaContext(const CudaContext&) = default;
		CudaContext& operator=(const CudaContext&) = default;

		CudaContext(CudaContext&&) noexcept = default;
		CudaContext& operator=(CudaContext&&) noexcept = default;
	};

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(8) PinnedAddress final {
		void* m_GpuAddr = nullptr;

		explicit PinnedAddress(void* addr) : m_GpuAddr(addr) {}
		PinnedAddress() = default;
		~PinnedAddress() = default;

		PinnedAddress(const PinnedAddress&) = default;
		PinnedAddress& operator=(const PinnedAddress&) = default;

		PinnedAddress(PinnedAddress&&) noexcept = default;
		PinnedAddress& operator=(PinnedAddress&&) noexcept = default;

		CORIUM_NODISCARD bool isValid() const {
			return m_GpuAddr != nullptr;
		}
	};

	CORIUM_STATIC_ASSERT(std::is_standard_layout_v<PinnedAddress>, "GpuAddress must maintain standard layout");
	CORIUM_STATIC_ASSERT(std::is_trivially_copyable_v<PinnedAddress>, "GpuAddress must be trivially copyable");
	CORIUM_STATIC_ASSERT(std::is_trivially_move_assignable_v<PinnedAddress>, "GpuAddress must be trivially move assignable");
	CORIUM_STATIC_ASSERT(sizeof(PinnedAddress) == 8, "Invalid GpuAddress size, must be 64bit");

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(16) GpuMemory final {
		size_t m_TotalMemory = 0;
		size_t m_AvailableMemory = 0;

		GpuMemory() = default;
		~GpuMemory() = default;

		GpuMemory(const GpuMemory&) = default;
		GpuMemory& operator=(const GpuMemory&) = default;

		GpuMemory(GpuMemory&&) noexcept = default;
		GpuMemory& operator=(GpuMemory&&) noexcept = default;
	};

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(8) GpuAddress final {
		uint64_t m_GpuAddr = 0;

		explicit GpuAddress(uint64_t addr) : m_GpuAddr(addr) {}
		GpuAddress() = default;
		~GpuAddress() = default;

		GpuAddress(const GpuAddress&) = default;
		GpuAddress& operator=(const GpuAddress&) = default;

		GpuAddress(GpuAddress&&) noexcept = default;
		GpuAddress& operator=(GpuAddress&&) noexcept = default;

		CORIUM_NODISCARD bool isValid() const {
			return m_GpuAddr != 0;
		}
	};

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(8) AllocHandle final {
		uint64_t m_Handle;

		bool isValid() const {
			return m_Handle ? 1 : 0;
		}

		AllocHandle() = default;
		~AllocHandle() = default;

		AllocHandle(const AllocHandle&) = default;
		AllocHandle& operator=(const AllocHandle&) = default;

		AllocHandle(AllocHandle&&) noexcept = default;
		AllocHandle& operator=(AllocHandle&&) noexcept = default;
	};

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(8) Location final {
		DeviceLocation m_Location;
		DeviceHandle   m_Handle;

		Location() = default;
		~Location() = default;

		Location(const Location&) = default;
		Location& operator=(const Location&) = default;

		Location(Location&&) noexcept = default;
		Location& operator=(Location&&) noexcept = default;
	};

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(16) AllocDesc final {
		void*                m_win32meta;
		Location             m_Loc;
		AllocationType       m_Type;
		AllocationHandleType m_HandleType;

		AllocDesc() = default;
		~AllocDesc() = default;

		AllocDesc(const AllocDesc&) = default;
		AllocDesc& operator=(const AllocDesc&) = default;

		AllocDesc(AllocDesc&&) noexcept = default;
		AllocDesc& operator=(AllocDesc&&) noexcept = default;
	};

	CORIUM_RUNTIME_API void initAllocDesc(AllocDesc& ro_Desc);
	CORIUM_RUNTIME_API void setAllocationType(AllocDesc& ro_Desc, AllocationType v_Type);
	CORIUM_RUNTIME_API void setAllocationHandleType(AllocDesc& ro_Desc, AllocationHandleType v_Type);
	CORIUM_RUNTIME_API void setLocation(AllocDesc& ro_Desc, DeviceHandle& ro_Handle);

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(16) AccessDesc final {
		Location m_Loc;
		uint64_t flags;

		AccessDesc() = default;
		~AccessDesc() = default;

		AccessDesc(const AccessDesc&) = default;
		AccessDesc& operator=(const AccessDesc&) = default;

		AccessDesc(AccessDesc&&) noexcept = default;
		AccessDesc& operator=(AccessDesc&&) noexcept = default;
	};

	CORIUM_RUNTIME_API void initAccessDesc(AccessDesc& ro_Desc);
	CORIUM_RUNTIME_API void setAccessLocation(AccessDesc& ro_Desc, DeviceHandle& ro_Handle);
	CORIUM_RUNTIME_API void setAccessFlags(AccessDesc& ro_Desc, uint64_t v_Flags);

	// -----------------------------------------------------------------------
	// CudaArray handle — opaque wrapper required by MemCpy3DDesc
	// -----------------------------------------------------------------------

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(8) CudaArray final {
		void* m_Array = nullptr;

		CudaArray() = default;
		~CudaArray() = default;

		CudaArray(const CudaArray&) = default;
		CudaArray& operator=(const CudaArray&) = default;

		CudaArray(CudaArray&&) noexcept = default;
		CudaArray& operator=(CudaArray&&) noexcept = default;

		CORIUM_NODISCARD bool isValid() const {
			return m_Array != nullptr;
		}
	};

	CORIUM_STATIC_ASSERT(sizeof(CudaArray) == 8, "Invalid CudaArray size, must be 64bit");
	CORIUM_STATIC_ASSERT(std::is_standard_layout_v<CudaArray>, "CudaArray must maintain standard layout");
	CORIUM_STATIC_ASSERT(std::is_trivially_copyable_v<CudaArray>, "CudaArray must be trivially copyable");
	CORIUM_STATIC_ASSERT(std::is_trivially_move_assignable_v<CudaArray>, "CudaArray must be trivially move assignable");

	// -----------------------------------------------------------------------
	// 3D memory copy descriptor
	// -----------------------------------------------------------------------

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(16) MemCpy3DDesc final {
		// source
		CopyMemoryType  m_SrcType           = CopyMemoryType::HOST;
		PinnedAddress   m_SrcHost           = {};
		GpuAddress      m_SrcDevice         = {};
		CudaArray       m_SrcArray          = {};
		size_t          m_SrcPitch          = 0;
		size_t          m_SrcHeight         = 0;
		size_t          m_SrcXOffsetBytes   = 0;
		size_t          m_SrcYOffset        = 0;
		size_t          m_SrcZOffset        = 0;

		// destination
		CopyMemoryType  m_DstType           = CopyMemoryType::ARRAY;
		PinnedAddress   m_DstHost           = {};
		GpuAddress      m_DstDevice         = {};
		CudaArray       m_DstArray          = {};
		size_t          m_DstPitch          = 0;
		size_t          m_DstHeight         = 0;
		size_t          m_DstXOffsetBytes   = 0;
		size_t          m_DstYOffset        = 0;
		size_t          m_DstZOffset        = 0;

		// copy dimensions
		size_t          m_WidthInBytes      = 0;
		size_t          m_Height            = 0;
		size_t          m_Depth             = 0;

		MemCpy3DDesc() = default;
		~MemCpy3DDesc() = default;

		MemCpy3DDesc(const MemCpy3DDesc&) = default;
		MemCpy3DDesc& operator=(const MemCpy3DDesc&) = default;

		MemCpy3DDesc(MemCpy3DDesc&&) noexcept = default;
		MemCpy3DDesc& operator=(MemCpy3DDesc&&) noexcept = default;
	};

	CORIUM_RUNTIME_API void initMemCpy3DDesc(MemCpy3DDesc& ro_Desc);
	CORIUM_RUNTIME_API void setMemCpy3DSrcHost(MemCpy3DDesc& ro_Desc, PinnedAddress v_Src);
	CORIUM_RUNTIME_API void setMemCpy3DSrcDevice(MemCpy3DDesc& ro_Desc, GpuAddress v_Src);
	CORIUM_RUNTIME_API void setMemCpy3DSrcArray(MemCpy3DDesc& ro_Desc, CudaArray v_Src);
	CORIUM_RUNTIME_API void setMemCpy3DDstHost(MemCpy3DDesc& ro_Desc, PinnedAddress v_Dst);
	CORIUM_RUNTIME_API void setMemCpy3DDstDevice(MemCpy3DDesc& ro_Desc, GpuAddress v_Dst);
	CORIUM_RUNTIME_API void setMemCpy3DDstArray(MemCpy3DDesc& ro_Desc, CudaArray v_Dst);
	CORIUM_RUNTIME_API void setMemCpy3DDimensions(MemCpy3DDesc& ro_Desc, size_t v_WidthInBytes, size_t v_Height, size_t v_Depth);
	CORIUM_RUNTIME_API void setMemCpy3DSrcPitch(MemCpy3DDesc& ro_Desc, size_t v_Pitch, size_t v_Height);
	CORIUM_RUNTIME_API void setMemCpy3DDstPitch(MemCpy3DDesc& ro_Desc, size_t v_Pitch, size_t v_Height);
	CORIUM_RUNTIME_API void setMemCpy3DSrcOffsets(MemCpy3DDesc& ro_Desc, size_t v_XOffsetBytes, size_t v_YOffset, size_t v_ZOffset);
	CORIUM_RUNTIME_API void setMemCpy3DDstOffsets(MemCpy3DDesc& ro_Desc, size_t v_XOffsetBytes, size_t v_YOffset, size_t v_ZOffset);

	// -----------------------------------------------------------------------
	// Stream / Event / Graph handles
	// -----------------------------------------------------------------------

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(8) GpuStream final {
		void* m_StreamHandle = nullptr;

		GpuStream() = default;
		~GpuStream() = default;

		GpuStream(const GpuStream&) = default;
		GpuStream& operator=(const GpuStream&) = default;

		GpuStream(GpuStream&&) noexcept = default;
		GpuStream& operator=(GpuStream&&) noexcept = default;

		CORIUM_NODISCARD bool isValid() const {
			return m_StreamHandle != nullptr;
		}
	};

	CORIUM_STATIC_ASSERT(sizeof(GpuStream) == 8, "Invalid GpuStream size, must be 64bit");
	CORIUM_STATIC_ASSERT(std::is_standard_layout_v<GpuStream>, "GpuStream must maintain standard layout");
	CORIUM_STATIC_ASSERT(std::is_trivially_copyable_v<GpuStream>, "GpuStream must be trivially copyable");
	CORIUM_STATIC_ASSERT(std::is_trivially_move_assignable_v<GpuStream>, "GpuStream must be trivially move assignable");

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(8) GpuEvent final {
		void* m_EventHandle = nullptr;

		GpuEvent() = default;
		~GpuEvent() = default;

		GpuEvent(const GpuEvent&) = default;
		GpuEvent& operator=(const GpuEvent&) = default;

		GpuEvent(GpuEvent&&) noexcept = default;
		GpuEvent& operator=(GpuEvent&&) noexcept = default;

		CORIUM_NODISCARD bool isValid() const {
			return m_EventHandle != nullptr;
		}
	};

	CORIUM_STATIC_ASSERT(sizeof(GpuEvent) == 8, "Invalid GpuEvent size, must be 64bit");
	CORIUM_STATIC_ASSERT(std::is_standard_layout_v<GpuEvent>, "GpuEvent must maintain standard layout");
	CORIUM_STATIC_ASSERT(std::is_trivially_copyable_v<GpuEvent>, "GpuEvent must be trivially copyable");
	CORIUM_STATIC_ASSERT(std::is_trivially_move_assignable_v<GpuEvent>, "GpuEvent must be trivially move assignable");

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(8) GpuGraph final {
		void* m_GraphHandle = nullptr;

		GpuGraph() = default;
		~GpuGraph() = default;

		GpuGraph(const GpuGraph&) = default;
		GpuGraph& operator=(const GpuGraph&) = default;

		GpuGraph(GpuGraph&&) noexcept = default;
		GpuGraph& operator=(GpuGraph&&) noexcept = default;

		CORIUM_NODISCARD bool isValid() const {
			return m_GraphHandle != nullptr;
		}
	};

	CORIUM_STATIC_ASSERT(sizeof(GpuGraph) == 8, "Invalid GpuGraph size, must be 64bit");
	CORIUM_STATIC_ASSERT(std::is_standard_layout_v<GpuGraph>, "GpuGraph must maintain standard layout");
	CORIUM_STATIC_ASSERT(std::is_trivially_copyable_v<GpuGraph>, "GpuGraph must be trivially copyable");
	CORIUM_STATIC_ASSERT(std::is_trivially_move_assignable_v<GpuGraph>, "GpuGraph must be trivially move assignable");

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(8) GpuIpcEventHandle final {
		char m_Reserved[64];

		GpuIpcEventHandle() = default;
		~GpuIpcEventHandle() = default;

		GpuIpcEventHandle(const GpuIpcEventHandle&) = default;
		GpuIpcEventHandle& operator=(const GpuIpcEventHandle&) = default;

		GpuIpcEventHandle(GpuIpcEventHandle&&) noexcept = default;
		GpuIpcEventHandle& operator=(GpuIpcEventHandle&&) noexcept = default;
	};

	CORIUM_STATIC_ASSERT(sizeof(GpuIpcEventHandle) == 64, "Invalid GpuIpcEventHandle size, must be 64 bytes");
	CORIUM_STATIC_ASSERT(std::is_standard_layout_v<GpuIpcEventHandle>, "GpuIpcEventHandle must maintain standard layout");
	CORIUM_STATIC_ASSERT(std::is_trivially_copyable_v<GpuIpcEventHandle>, "GpuIpcEventHandle must be trivially copyable");
	CORIUM_STATIC_ASSERT(std::is_trivially_move_assignable_v<GpuIpcEventHandle>, "GpuIpcEventHandle must be trivially move assignable");

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(8) GpuGraphExec final {
		void* m_ExecHandle = nullptr;

		GpuGraphExec() = default;
		~GpuGraphExec() = default;

		GpuGraphExec(const GpuGraphExec&) = default;
		GpuGraphExec& operator=(const GpuGraphExec&) = default;

		GpuGraphExec(GpuGraphExec&&) noexcept = default;
		GpuGraphExec& operator=(GpuGraphExec&&) noexcept = default;

		CORIUM_NODISCARD bool isValid() const {
			return m_ExecHandle != nullptr;
		}
	};

	CORIUM_STATIC_ASSERT(sizeof(GpuGraphExec) == 8, "Invalid GpuGraphExec size, must be 64bit");
	CORIUM_STATIC_ASSERT(std::is_standard_layout_v<GpuGraphExec>, "GpuGraphExec must maintain standard layout");
	CORIUM_STATIC_ASSERT(std::is_trivially_copyable_v<GpuGraphExec>, "GpuGraphExec must be trivially copyable");
	CORIUM_STATIC_ASSERT(std::is_trivially_move_assignable_v<GpuGraphExec>, "GpuGraphExec must be trivially move assignable");

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(8) GpuGraphNode final {
		void* m_NodeHandle = nullptr;

		GpuGraphNode() = default;
		~GpuGraphNode() = default;

		GpuGraphNode(const GpuGraphNode&) = default;
		GpuGraphNode& operator=(const GpuGraphNode&) = default;

		GpuGraphNode(GpuGraphNode&&) noexcept = default;
		GpuGraphNode& operator=(GpuGraphNode&&) noexcept = default;

		CORIUM_NODISCARD bool isValid() const {
			return m_NodeHandle != nullptr;
		}
	};

	CORIUM_STATIC_ASSERT(sizeof(GpuGraphNode) == 8, "Invalid GpuGraphNode size, must be 64bit");
	CORIUM_STATIC_ASSERT(std::is_standard_layout_v<GpuGraphNode>, "GpuGraphNode must maintain standard layout");
	CORIUM_STATIC_ASSERT(std::is_trivially_copyable_v<GpuGraphNode>, "GpuGraphNode must be trivially copyable");
	CORIUM_STATIC_ASSERT(std::is_trivially_move_assignable_v<GpuGraphNode>, "GpuGraphNode must be trivially move assignable");

	enum class CORIUM_RUNTIME_API ExecAffinityType : uint8_t {
		SM_COUNT = 0
	};

	struct CORIUM_RUNTIME_API ExecAffinitySmCount final {
		uint32_t m_Val = 0;
	};

	struct CORIUM_RUNTIME_API ExecAffinityParam final {
		ExecAffinityType    m_Type;
		ExecAffinitySmCount m_SmCount;
	};

	// CIG (CUDA in Graphics) params are opaque at this abstraction level.
	// Cast to CUctxCigParam* at the call site when needed.
	struct CORIUM_RUNTIME_API CtxCreateParams final {
		ExecAffinityParam* m_ExecAffinityParams    = nullptr;
		int                m_NumExecAffinityParams = 0;
		void*              m_CigParams             = nullptr;
	};

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(8) GpuGraphEdgeData final {
		unsigned char m_FromPort = 0;
		unsigned char m_ToPort   = 0;
		unsigned char m_Type     = 0;

		GpuGraphEdgeData() = default;
		~GpuGraphEdgeData() = default;

		GpuGraphEdgeData(const GpuGraphEdgeData&) = default;
		GpuGraphEdgeData& operator=(const GpuGraphEdgeData&) = default;

		GpuGraphEdgeData(GpuGraphEdgeData&&) noexcept = default;
		GpuGraphEdgeData& operator=(GpuGraphEdgeData&&) noexcept = default;
	};

	// Params for cuGraphAddKernelNode. m_Function must be a valid CUfunction handle from a loaded module.
	struct CORIUM_RUNTIME_API KernelNodeParams final {
		void*    m_Function       = nullptr; // CUfunction
		uint32_t m_GridDimX       = 1;
		uint32_t m_GridDimY       = 1;
		uint32_t m_GridDimZ       = 1;
		uint32_t m_BlockDimX      = 1;
		uint32_t m_BlockDimY      = 1;
		uint32_t m_BlockDimZ      = 1;
		uint32_t m_SharedMemBytes = 0;
		void**   m_KernelParams   = nullptr; // void*[] of kernel arguments
		void**   m_Extra          = nullptr; // null in standard usage
	};

	// Params for cuGraphAddMemsetNode. m_ElementSize must be 1, 2, or 4.
	struct CORIUM_RUNTIME_API MemsetNodeParams final {
		uint64_t m_Dst         = 0; // CUdeviceptr
		size_t   m_Pitch       = 0;
		uint32_t m_Value       = 0;
		uint32_t m_ElementSize = 1; // bytes: 1, 2, or 4
		size_t   m_Width       = 0;
		size_t   m_Height      = 1;
	};
}
