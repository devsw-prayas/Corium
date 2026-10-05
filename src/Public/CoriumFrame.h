#pragma once
#include "Corium.h"
#include "CoriumUtility.h"
#include "FrameUtils.h"
#include <optional>

namespace Corium::Core::Frame {

	namespace this_thread {
		CORIUM_RUNTIME_API void captureFrame(FrameHandle* p_Incoming);
		CORIUM_RUNTIME_API FrameHandle& nativeContext();
		// Frame currently executing on this thread (the native context when no frame is running).
		// Closures take no arguments, so this is how one finds itself, like t_Handle for NativeThread.
		CORIUM_RUNTIME_API FrameHandle* currentFrame();
	}

	namespace Internal {
		// Called by thread attach/detach; p_Storage is kReservedHeaderSize bytes, 64-aligned.
		void bindNativeContext(void* p_Storage, uint8_t v_NumaNode) noexcept;
		void unbindNativeContext() noexcept;
		// Needs a FROZEN desc (see validate). nullopt means the desc was not frozen or the pool is exhausted.
		CORIUM_RUNTIME_API std::optional<FrameHandle*> createFrameInternal(Utils::FunctionView<void()> v_View, const FrameStackDesc& ro_Desc);
	}

	class CORIUM_RUNTIME_API NativeFrame final {
	public:
		NativeFrame() = delete;
		~NativeFrame() = delete;

		NativeFrame(const NativeFrame&) = delete;
		NativeFrame& operator=(const NativeFrame&) = delete;
		
		NativeFrame(NativeFrame&&) noexcept = delete;
		NativeFrame& operator=(NativeFrame&&) noexcept = delete;

		template<typename A>
		static std::optional<FrameHandle*> createFrame(
			const Utils::ClosureFunction<A, void()>& ro_Closure, const FrameStackDesc& ro_Desc){
				return Internal::createFrameInternal(Utils::FunctionView<void()>(ro_Closure), ro_Desc);
			}

		static void switchTo(FrameHandle* p_Self, FrameHandle* p_Target, bool v_Terminate);
		static void yieldToNative(FrameHandle* p_Self, bool v_Terminate);
		// Wipes a suspended frame's whole block and leaves a blank TERMINATED handle. Never resumes it, never frees it.
		static void killFrame(FrameHandle* p_Target);
	};
}