#pragma once
#include "ThreadUtils.h"

// Raw thread_locals behind this_thread's accessors. Internal only: a DLL cannot export thread_local data,
// so consumers go through allocator(), currentHandle() and currentPermit().
namespace Corium::Core::this_thread {
	extern thread_local ThreadHandle                       t_Handle;
	extern thread_local ParkHandle                         t_Permit;
	extern thread_local Memory::Allocators::TlsAllocator   t_ThreadLocalAllocator;
}
