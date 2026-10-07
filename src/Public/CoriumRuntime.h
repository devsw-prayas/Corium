#pragma once

#include "Corium.h"

namespace Corium {
	class CORIUM_RUNTIME_API CoriumRuntime final {
	public:
		static void initRuntime();
		static bool isRuntimeInit();
	};
}
