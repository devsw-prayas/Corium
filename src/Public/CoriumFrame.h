#pragma once
#include "Corium.h"

namespace Corium::Core::Frame {
	class NativeFrame final {
	public:
		NativeFrame() = delete;
		~NativeFrame() = delete;

		NativeFrame(const NativeFrame&) = delete;
		NativeFrame& operator=(const NativeFrame&) = delete;
		
		NativeFrame(NativeFrame&&) noexcept = delete;
		NativeFrame& operator=(NativeFrame&&) noexcept = delete;
	};
}
