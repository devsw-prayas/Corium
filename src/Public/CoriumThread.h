#pragma once
#include "Corium.h"
#include "ThreadUtils.h"
#include "ThreadSupport.h"
#include "CoriumChrono.h"

namespace Corium::Core {

	class CORIUM_RUNTIME_API NativeThread {
	private:
		static bool isValidHandle(const ThreadHandle& ro_Handle) noexcept;

	public:
		static ThreadHandle createThread(
			ThreadLaunchDesc&& u_LaunchDesc, const ThreadAttrDesc& ro_ExecDesc) noexcept;
		static bool         detachThread(const ThreadHandle& ro_Handle) noexcept;
		static bool         closeHandle(const ThreadHandle& ro_Handle) noexcept;
		static ThreadHandle duplicateHandle(const ThreadHandle& ro_Handle) noexcept;

		static bool         isAlive(const ThreadHandle& ro_Handle) noexcept;
		static ProcessorIdx getThreadID(const ThreadHandle& ro_Handle) noexcept;

		static bool suspendThread(const ThreadHandle& ro_Handle) noexcept;
		static bool resumeThread(const ThreadHandle& ro_Handle) noexcept;
		static bool terminateThread(const ThreadHandle& ro_Handle) noexcept;
		static bool joinThread(const ThreadHandle& ro_Handle) noexcept;

		static uint32_t getNumaNode(const ThreadHandle& ro_Handle) noexcept;

		static void waitOnAddress(ParkingSupport& ro_Support, uint32_t expected = 0u) noexcept;
		static void wakeOnAddress(ParkingSupport& ro_Support) noexcept;
		static void wakeAllOnAddress(ParkingSupport& ro_Support) noexcept;

		// Parks while the word equals v_Expected; pass the value the caller last saw, not a constant.
		static void waitOnAddressFor(ParkingSupport& ro_Support, Chrono::Instant v_Deadline, uint32_t v_Expected = 0u) noexcept;
	};
}
