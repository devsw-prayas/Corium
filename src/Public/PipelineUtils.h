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
#include "CoriumPointers.h"
#include "EngineAllocators.h"
#include "CoriumAtomics.h"
#include "IrUtils.h"
#include "CoriumUtility.h"

namespace Corium::Execution {
	namespace IR = Corium::IntermediateRepresentation;

	// Forward declarations
	struct TaskContext;                                         // needed for TaskHandle friend
	namespace Inductor { struct TaskMemoryDesc; class TaskInductor; }
	namespace Builder { class TaskBuilder; }
	namespace Executor { class TaskExecutor; }

	// TaskID

	using TaskID = uint64_t;
	inline constexpr TaskID INVALID_TASK_ID = 0;

	// TaskPriority

	enum class TaskPriority : uint8_t {
		Low = 0,
		Normal = 1,
		High = 2,
		Critical = 3,
	};

	// TaskState

	enum class TaskState : uint8_t {
		Pending = 0,
		Running = 1,
		Completed = 2,
		Failed = 3,
		Cancelled = 4,
	};

	// TaskError
	//
	// Fixed-size, Corium-owned. Always at output buffer offset 0.
	// v_Code = 0 means no error.
	//
	// Error code ranges:
	//   0           No error
	//   1  – 999    Corium runtime
	//   1000 – 1999 Pool layer (cancellation, timeout, scheduling)
	//   2000+       User task errors from callable

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(64) TaskError final {
		uint32_t v_Code = 0;
		char     v_Message[124] = {};
	};

	CORIUM_STATIC_ASSERT(sizeof(TaskError) == 128, "TaskError must be 128 bytes");
	CORIUM_STATIC_ASSERT(std::is_standard_layout_v<TaskError>, "TaskError must maintain standard layout");
	CORIUM_STATIC_ASSERT(std::is_trivially_copyable_v<TaskError>, "TaskError must be trivially copyable");

	// TaskMemory
	//
	// VARegion views into g_TaskPayloadArena.
	// Allocated by TaskInductor::cook() via TaskPayloadAllocator (bump, never freed).

	struct CORIUM_RUNTIME_API TaskMemory final {
		TaskMemory() = default;
		~TaskMemory() = default;

		TaskMemory(const TaskMemory&) = delete;
		TaskMemory& operator=(const TaskMemory&) = delete;

		TaskMemory(TaskMemory&&) noexcept = default;
		TaskMemory& operator=(TaskMemory&&) noexcept = default;

		Memory::Internal::VARegion m_InputMemBuffer;
		Memory::Internal::VARegion m_OutputMemBuffer;
	};

	// TaskExecutionState
	//
	// SharedPtr-managed, allocated from TaskMetadataAllocator per NUMA node.
	// Pool holds the strong ref; TaskHandle copies hold WeakPtrs.
	// On task retirement the pool releases the SharedPtr — all WeakPtrs expire.

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(16) TaskExecutionState final {
		TaskExecutionState() = default;
		~TaskExecutionState() = default;

		TaskExecutionState(const TaskExecutionState&) = delete;
		TaskExecutionState& operator=(const TaskExecutionState&) = delete;
		TaskExecutionState(TaskExecutionState&&) noexcept = delete;
		TaskExecutionState& operator=(TaskExecutionState&&) noexcept = delete;

		Core::Atomic::AtomicValue32<uint32_t> m_State{ static_cast<uint32_t>(TaskState::Pending) };
		uint8_t* m_pOutputBuffer = nullptr;
	};

	CORIUM_STATIC_ASSERT(sizeof(TaskExecutionState) == 16, "TaskExecutionState must be 16 bytes");
	CORIUM_STATIC_ASSERT(std::is_standard_layout_v<TaskExecutionState>, "TaskExecutionState must maintain standard layout");

	// TaskHandle
	//
	// Copyable observer handle returned by Executor::submitObservable().
	// Multiple copies may observe the same task. All copies become stale once
	// the pool releases the TaskExecutionState (ImplicitDataFlushTask).
	// Accessing get<>() before isComplete() or on a stale handle is hard UB.
	//
	// wait() must not be called from within a task callable.

	struct CORIUM_RUNTIME_API TaskHandle final {
		TaskHandle() = default;
		~TaskHandle() = default;

		TaskHandle(const TaskHandle&) = default;
		TaskHandle& operator=(const TaskHandle&) = default;

		TaskHandle(TaskHandle&&) noexcept = default;
		TaskHandle& operator=(TaskHandle&&) noexcept = default;

		CORIUM_NODISCARD TaskID    taskID()  const noexcept { return m_TaskID; }
		CORIUM_NODISCARD bool      isValid() const noexcept { return !m_wpState.expired(); }

		CORIUM_NODISCARD TaskState state() const noexcept {
			auto locked = m_wpState.lock();
			if (!locked) return TaskState::Cancelled;
			return static_cast<TaskState>(
				locked->m_State.load(Core::Atomics::MemoryOrder::ACQUIRE));
		}

		CORIUM_NODISCARD bool isComplete()  const noexcept { return state() == TaskState::Completed; }
		CORIUM_NODISCARD bool isFailed()    const noexcept { return state() == TaskState::Failed; }
		CORIUM_NODISCARD bool isCancelled() const noexcept { return state() == TaskState::Cancelled; }
		CORIUM_NODISCARD bool isPending()   const noexcept {
			const TaskState currentState = state();
			return currentState == TaskState::Pending || currentState == TaskState::Running;
		}

		// Blocks until Completed, Failed, or Cancelled.
		// Must not be called from within a task callable.
		void wait() const noexcept;

		// MPH lookup into output buffer. Hard UB before completion or on miss.
		template<typename TParam, size_t N>
		CORIUM_NODISCARD const typename TParam::DataType& get() const noexcept {
			auto locked = m_wpState.lock();
			const uint64_t hash = IR::mixPositionHash(TParam::TypeHash, N);
			const size_t   offset = m_pOutputMPH->lookup(hash);
			return *reinterpret_cast<const typename TParam::DataType*>(
				locked->m_pOutputBuffer + offset);
		}

		template<typename TParam, size_t N>
		CORIUM_NODISCARD typename TParam::DataType& get() noexcept {
			auto locked = m_wpState.lock();
			const uint64_t hash = IR::mixPositionHash(TParam::TypeHash, N);
			const size_t   offset = m_pOutputMPH->lookup(hash);
			return *reinterpret_cast<typename TParam::DataType*>(
				locked->m_pOutputBuffer + offset);
		}

		// Reads TaskError from output buffer offset 0. Meaningful only after isFailed().
		CORIUM_NODISCARD TaskError error() const noexcept {
			auto locked = m_wpState.lock();
			return *reinterpret_cast<const TaskError*>(locked->m_pOutputBuffer);
		}

	private:
		Memory::WeakPtr<TaskExecutionState, Memory::Allocators::TaskMetadataAllocator> m_wpState;
		const IR::MphTable* m_pOutputMPH = nullptr;
		TaskID              m_TaskID = INVALID_TASK_ID;

		friend struct TaskContext;
		friend class Executor::TaskExecutor;
	};

	// TaskContext
	//
	// Execution-time view of a running task supplied to void(TaskContext&)
	// callables. Constructed on the worker thread's stack immediately before
	// invoking the closure, destroyed immediately after return.
	//
	// Non-copyable, non-movable. Only valid during callable execution.

	struct CORIUM_RUNTIME_API TaskContext {
		TaskContext() = delete;
		~TaskContext() = default;

		TaskContext(const TaskContext&) = delete;
		TaskContext& operator=(const TaskContext&) = delete;
		TaskContext(TaskContext&&) = delete;
		TaskContext& operator=(TaskContext&&) = delete;

		// Read input parameter at pack position N. Hard UB on miss.
		template<typename TParam, size_t N>
		CORIUM_NODISCARD const typename TParam::DataType& get() const noexcept {
			return *reinterpret_cast<const typename TParam::DataType*>(
				m_pInputBuffer + m_pInputMPH->lookup(IR::mixPositionHash(TParam::TypeHash, N)));
		}

		// Mutable input access. Hard UB on miss.
		template<typename TParam, size_t N>
		CORIUM_NODISCARD typename TParam::DataType& get() noexcept {
			return *reinterpret_cast<typename TParam::DataType*>(
				m_pInputBuffer + m_pInputMPH->lookup(IR::mixPositionHash(TParam::TypeHash, N)));
		}

		// Write output parameter at pack position N. Hard UB on miss.
		template<typename TParam, size_t N>
		void put(typename TParam::DataType v_Value) noexcept {
			*reinterpret_cast<typename TParam::DataType*>(
				m_pOutputBuffer + m_pOutputMPH->lookup(IR::mixPositionHash(TParam::TypeHash, N)))
				= std::move(v_Value);
		}

		// Writes TaskError to output buffer offset 0. Sets state to FAILED.
		// No-op after first call.
		void fail(uint32_t v_Code, const char* p_Message) noexcept;

		CORIUM_NODISCARD const TaskHandle& handle() const noexcept { return m_Handle; }

	protected:
		explicit TaskContext(
			uint8_t* p_InputBuffer,
			uint8_t* p_OutputBuffer,
			const IR::MphTable* p_InputMPH,
			const IR::MphTable* p_OutputMPH,
			TaskHandle          v_Handle
		) noexcept
			: m_pInputBuffer(p_InputBuffer)
			, m_pOutputBuffer(p_OutputBuffer)
			, m_pInputMPH(p_InputMPH)
			, m_pOutputMPH(p_OutputMPH)
			, m_Handle(std::move(v_Handle)) {}

		uint8_t* m_pInputBuffer;
		uint8_t* m_pOutputBuffer;
		const IR::MphTable* m_pInputMPH;
		const IR::MphTable* m_pOutputMPH;
		TaskHandle          m_Handle;

		friend class Executor::TaskExecutor;
	};

	// TaskFrame
	//
	// Lightweight, move-only handle produced by TaskInductor::cook().
	// Binds a frozen TaskMemoryDesc to payload buffers in g_TaskPayloadArena.
	//
	// After cook(), TaskBuilder::build() allocates a ClosureFunction into
	// g_ClosureRange and writes the closure pointers below. TaskFrame holds
	// raw pointers into VA — it owns nothing.
	//
	// isValid()  — canonical null check for cook() failure.
	// isBuilt()  — true once a closure has been bound via build().
	//
	// m_pfnSliceInvoke — reserved for WorkStealableTask / TaskSliceContext path
	//                    (not implemented; set to nullptr until that layer lands).

	struct CORIUM_RUNTIME_API TaskFrame final {
		TaskFrame() = default;
		~TaskFrame() = default;

		TaskFrame(const TaskFrame&) = delete;
		TaskFrame& operator=(const TaskFrame&) = delete;

		TaskFrame(TaskFrame&&) noexcept = default;
		TaskFrame& operator=(TaskFrame&&) noexcept = default;

		CORIUM_NODISCARD bool isValid() const noexcept {
			return !m_MemDesc.expired() && m_InputBuffer.isValid();
		}

		CORIUM_NODISCARD bool isBuilt() const noexcept {
			return m_pClosure != nullptr;
		}

	private:
		explicit TaskFrame(
			Memory::WeakPtr<Inductor::TaskMemoryDesc, Memory::Allocators::TaskMetadataAllocator> sw_Desc,
			Memory::Internal::VARegion v_InputBuf,
			Memory::Internal::VARegion v_OutputBuf
		) noexcept
			: m_MemDesc(std::move(sw_Desc))
			, m_InputBuffer(v_InputBuf)
			, m_OutputBuffer(v_OutputBuf) {}

		Memory::WeakPtr<Inductor::TaskMemoryDesc, Memory::Allocators::TaskMetadataAllocator> m_MemDesc;
		Memory::Internal::VARegion m_InputBuffer;
		Memory::Internal::VARegion m_OutputBuffer;

		// Closure section — all null until build() is called.
		// m_pClosure and m_pfnDestroy always set together.
		// Exactly one invoke pointer is non-null at runtime.
		void* m_pClosure = nullptr;  // ClosureFunction<...>* in g_ClosureRange
		void(*m_pfnDestroy)(void*) = nullptr;
		void(*m_pfnCpuInvoke)(void*, TaskContext&) = nullptr;
		void* m_pfnSliceInvoke = nullptr;  // reserved — TaskSliceContext path

		friend class Inductor::TaskInductor;
		friend class Builder::TaskBuilder;
		friend class Executor::TaskExecutor;
	};
}
