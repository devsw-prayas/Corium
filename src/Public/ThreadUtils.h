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
#include <CoriumUtility.h>

#include "AtomicVar.h"
#include "EngineAllocators.h"

namespace Corium::Core {
	// Forward declaration — implementation lives in CoriumThread.cpp.
	namespace Internal { struct ThreadLaunchHelper; }

	struct Allocator {};

	template<typename S>
	using Closure = Utils::ClosureFunction<Memory::Allocators::ClosureAllocator, S>;

	using Utils::makeClosure;

	using AffinityMask = size_t;
	using ProcessorIdx = uint32_t;
	using Dword = uint32_t;

	using Flag = bool;
	constexpr Flag allow = true;
	constexpr Flag disallow = false;

	enum class ThreadPriority {
		ZERO, LOW, BELOW_NORMAL, NORMAL, ABOVE_NORMAL, HIGH, TIME_CRITICAL
	};

	enum class ThreadState {
		CREATED, RUNNING, SEALED, REAPED
	};

	enum class DescriptorState : uint8_t { UNINITIALIZED, MUTABLE, FROZEN };

	struct CORIUM_RUNTIME_API alignas(32) ThreadAttrDesc final {
		AffinityMask m_Mask;
		ProcessorIdx m_IdealProcessor;
		ThreadPriority m_ThreadPriority;
		uint32_t m_NumaNode;
		DescriptorState m_State = DescriptorState::UNINITIALIZED;
		Flag m_isDetached;
		Flag m_SupportsIdealProcessor;
		Flag m_IsGuardPageEnabled;

#ifdef _WIN32
		Flag m_SupportsGroup;
		Dword m_GroupId;
#endif
		ThreadAttrDesc() = default;
		~ThreadAttrDesc() = default;

		ThreadAttrDesc(const ThreadAttrDesc&) = default;
		ThreadAttrDesc& operator=(const ThreadAttrDesc&) = default;
		ThreadAttrDesc(ThreadAttrDesc&&) noexcept = default;
		ThreadAttrDesc& operator=(ThreadAttrDesc&&) noexcept = default;
	};

	void CORIUM_RUNTIME_API init(ThreadAttrDesc& ro_Desc);
	void CORIUM_RUNTIME_API setAffinity(ThreadAttrDesc& ro_Desc, AffinityMask v_Mask);
	void CORIUM_RUNTIME_API setPriority(ThreadAttrDesc& ro_Desc, ThreadPriority v_Priority);
	void CORIUM_RUNTIME_API shouldSupportIdealProcessor(ThreadAttrDesc& ro_Desc, Flag v_Permission);
	void CORIUM_RUNTIME_API setIdealProcessor(ThreadAttrDesc& ro_Desc, ProcessorIdx v_Idx);
	void CORIUM_RUNTIME_API canDetach(ThreadAttrDesc& ro_Desc, Flag v_Permission);
	void CORIUM_RUNTIME_API vaGuardEnabled(ThreadAttrDesc& ro_Desc, Flag v_Permission);
	void CORIUM_RUNTIME_API setNumaNode(ThreadAttrDesc& ro_Desc, uint32_t v_Node);
#ifdef _WIN32
	void CORIUM_RUNTIME_API setThreadGroup(ThreadAttrDesc& ro_Desc, Dword v_GroupId);
#endif
	Flag CORIUM_RUNTIME_API validate(ThreadAttrDesc& ro_Desc);

	struct CORIUM_RUNTIME_API alignas(64) ThreadLaunchDesc final {
		Closure<void()> m_StartPoint;
		size_t m_VaSize;
		const char* m_Name = "";
		DescriptorState m_State;
		Flag m_IsPreSuspended;
	};

	void CORIUM_RUNTIME_API init(ThreadLaunchDesc& ro_Desc);
	void CORIUM_RUNTIME_API
		attachLaunchAddr(ThreadLaunchDesc& ro_Desc,
			Closure<void()> v_Closure);
	void CORIUM_RUNTIME_API setVaSize(ThreadLaunchDesc& ro_Desc, size_t v_VaSize);
	void CORIUM_RUNTIME_API setName(ThreadLaunchDesc& ro_Desc, const char* p_Name);
	void CORIUM_RUNTIME_API isPreSuspended(ThreadLaunchDesc& ro_Desc, Flag v_Permission);
	bool CORIUM_RUNTIME_API validate(ThreadLaunchDesc& ro_Desc);

	CORIUM_FORCEINLINE static Flag isFrozen(const ThreadAttrDesc& ro_Desc) {
		return ro_Desc.m_State == DescriptorState::FROZEN;
	}

	CORIUM_FORCEINLINE static void promoteMutable(ThreadAttrDesc& r_Desc) {
		if (r_Desc.m_State == DescriptorState::UNINITIALIZED)
			r_Desc.m_State = DescriptorState::MUTABLE;
	}

	struct CORIUM_RUNTIME_API CORIUM_ALIGNAS(64) ParkHandle final {
		Atomic::AtomicValue32<uint32_t> m_ParkingPermit{ 0 };

		explicit ParkHandle(uint32_t v_Value) : m_ParkingPermit(v_Value) {}
		ParkHandle(const ParkHandle&) = default;
		ParkHandle& operator=(const ParkHandle&) = default;
		ParkHandle(ParkHandle&&) noexcept = default;
		ParkHandle& operator=(ParkHandle&&) noexcept = default;
		~ParkHandle() = default;
	};

	struct CORIUM_ALIGNAS(32) CORIUM_RUNTIME_API ThreadHandle final {
		friend class NativeThread;
		friend struct Internal::ThreadLaunchHelper;
	private:
		size_t m_ThreadId;
		size_t m_Generation;

		size_t m_AccessToken;
		ThreadState m_State;

		constexpr ThreadHandle(size_t v_ThreadId, size_t v_Generation, size_t v_AccessToken, ThreadState v_State)
			: m_ThreadId(v_ThreadId), m_Generation(v_Generation), m_AccessToken(v_AccessToken), m_State(v_State) {}

	public:
		ThreadHandle() = default;
		ThreadState expectedState() const {
			return m_State;
		}

		static ThreadHandle getInvalidThread() {
			return { 0, 0, 0, ThreadState::REAPED };
		}
	};

	namespace this_thread {
		CORIUM_RUNTIME_API Memory::Allocators::TlsAllocator& allocator() noexcept;
		CORIUM_RUNTIME_API ThreadHandle& currentHandle() noexcept;
		CORIUM_RUNTIME_API ParkHandle& currentPermit() noexcept;

		template<typename T, typename...Args>
		CORIUM_NODISCARD T* create(Args&&...u_Args) noexcept {
			void* memory = allocator().allocate(sizeof(T), alignof(T));
			return memory ? allocator().emplace<T>(memory, std::forward<Args>(u_Args)...) : nullptr;
		}

		template<typename T>
		void destroy(T* p_Loc) noexcept {
			p_Loc->~T();
		}
		
		// Adopts a foreign thread; counted, so pair every attach with a detach. v_TlsSize 0 = TLS min.
		// The TLS slice is never reclaimed: attach once per thread lifetime, not per task.
		CORIUM_NODISCARD CORIUM_RUNTIME_API bool attach(uint32_t v_NumaNode = 0, size_t v_TlsSize = 0) noexcept;

		CORIUM_RUNTIME_API void detach() noexcept;
	}
}
