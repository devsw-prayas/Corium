#pragma once
#include "Corium.h"
#include "CoriumUtility.h"
#include "FrameUtils.h"
#include <optional>

namespace Corium::Core::Frame {
	using FrameEntry = Utils::ClosureFunction<Memory::Allocators::ClosureAllocator, void()>;

	namespace this_thread {
		CORIUM_RUNTIME_API void captureFrame(FrameHandle* p_Incoming);
		CORIUM_RUNTIME_API FrameHandle& nativeContext();
		// Frame currently executing on this thread (the native context when no frame is running).
		// Closures take no arguments, so this is how one finds itself, like t_Handle for NativeThread.
		CORIUM_RUNTIME_API FrameHandle* currentFrame();
		// False when the frame is RUNNING or TERMINATED; a pending terminate is dropped and the caller keeps running.
		CORIUM_NODISCARD CORIUM_RUNTIME_API bool tryCaptureFrame(FrameHandle* p_Incoming);
	}

	namespace Internal {
		// Called by thread attach/detach; p_Storage is kReservedHeaderSize bytes, 64-aligned.
		void bindNativeContext(void* p_Storage, uint8_t v_NumaNode) noexcept;
		void unbindNativeContext() noexcept;
		// Needs a FROZEN desc (see validate). nullopt means the desc was not frozen, the closure is not callable
		// or the pool is exhausted. The closure is only moved from on success.
		CORIUM_RUNTIME_API std::optional<FrameHandle*> createFrameInternal(FrameEntry&& u_Entry, const FrameStackDesc& ro_Desc);
	}

	class CORIUM_RUNTIME_API NativeFrame final {
	public:
		NativeFrame() = delete;
		~NativeFrame() = delete;

		NativeFrame(const NativeFrame&) = delete;
		NativeFrame& operator=(const NativeFrame&) = delete;
		
		NativeFrame(NativeFrame&&) noexcept = delete;
		NativeFrame& operator=(NativeFrame&&) noexcept = delete;

		// Takes ownership of the closure, so temporaries are fine; it is destroyed when the frame terminates or is killed.
		static std::optional<FrameHandle*> createFrame(FrameEntry&& u_Entry, const FrameStackDesc& ro_Desc) {
			return Internal::createFrameInternal(std::move(u_Entry), ro_Desc);
		}

		static void switchTo(FrameHandle* p_Self, FrameHandle* p_Target, bool v_Terminate);
		// Same as switchTo, but false (and no terminate) when the target cannot be claimed.
		CORIUM_NODISCARD static bool trySwitchTo(FrameHandle* p_Self, FrameHandle* p_Target, bool v_Terminate);
		static void yieldToNative(FrameHandle* p_Self, bool v_Terminate);
		// Wipes a suspended frame's whole block and leaves a blank TERMINATED handle. Never resumes it, never frees it.
		static void killFrame(const FrameHandle* p_Target);
	};
}