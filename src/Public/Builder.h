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
#include "CoriumMemory.h"
#include "Inductor.h"
#include "PipelineUtils.h"

namespace Corium::Execution::Builder {

	class CORIUM_RUNTIME_API TaskBuilder final {
	public:

		// Lowers a void(TaskContext&) callable into a ClosureFunction allocated
		// in g_ClosureRange. Writes type-erased closure pointers into ro_Frame.
		// Returns false if the frame is invalid, already built, or allocation fails.
		template<typename Callable>
			requires std::is_invocable_v<std::decay_t<Callable>, TaskContext&>
		CORIUM_NODISCARD static bool build(TaskFrame& ro_Frame, const Inductor::TaskDesc& ro_Desc, Callable&& u_Fn) {
			if (!ro_Frame.isValid()) return false;
			if (ro_Frame.isBuilt())  return false;

			using CF    = Core::Utils::ClosureFunction<Memory::Allocators::ClosureAllocator, void(TaskContext&)>;
			auto* allocator = Core::Utils::closureAllocator(static_cast<uint8_t>(ro_Desc.m_NumaNode));

			void* memory = allocator->allocate(sizeof(CF), alignof(CF));
			CF* function = memory
				? allocator->template emplace<CF>(memory, std::forward<Callable>(u_Fn), allocator)
				: nullptr;
			if (!function || !function->isCallable()) return false;

			ro_Frame.m_pClosure     = function;
			ro_Frame.m_pfnDestroy   = [](void* p) { static_cast<CF*>(p)->~CF(); };
			ro_Frame.m_pfnCpuInvoke = [](void* p, TaskContext& ctx) { (*static_cast<CF*>(p))(ctx); };

			return true;
		}

		// Destroys the bound closure and resets all closure fields.
		// Frame must not be submitted. Safe to call on an unbuilt frame.
		static void destroy(TaskFrame& ro_Frame) noexcept;

		// Replaces bound closure. Equivalent to destroy() + build().
		// Frame must not be submitted.
		template<typename Callable>
			requires std::is_invocable_v<std::decay_t<Callable>, TaskContext&>
		CORIUM_NODISCARD static bool rebind(TaskFrame& ro_Frame, const Inductor::TaskDesc& ro_Desc, Callable&& u_Fn) {
			destroy(ro_Frame);
			return build(ro_Frame, ro_Desc, std::forward<Callable>(u_Fn));
		}
	};

}
