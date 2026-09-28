#pragma once
#include "PlatIntrin.h"

namespace Corium::Core::Atomics {
	enum class MemoryOrder : std::uint8_t {
		RELAXED = CORIUM_MEMORY_ORDER_RELAXED,
		CONSUME = CORIUM_MEMORY_ORDER_CONSUME,
		ACQUIRE = CORIUM_MEMORY_ORDER_ACQUIRE,
		RELEASE = CORIUM_MEMORY_ORDER_RELEASE,
		ACQ_REL = CORIUM_MEMORY_ORDER_ACQ_REL,
		SEQ_CST = CORIUM_MEMORY_ORDER_SEQ_CST
	};

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atonic load") CORIUM_FORCEINLINE
		Valid atomicLoad(Valid* p_Memory, MemoryOrder v_Ordering) {
		switch (v_Ordering) {
		case MemoryOrder::RELAXED:
			return AtomicLoad_Relaxed(p_Memory);
		case MemoryOrder::CONSUME:
			return AtomicLoad_Consume(p_Memory);
		case MemoryOrder::ACQUIRE:
			return AtomicLoad_Acquire(p_Memory);
		case MemoryOrder::SEQ_CST:
			return AtomicLoad_SeqCst(p_Memory);
		case MemoryOrder::RELEASE: ;
		case MemoryOrder::ACQ_REL: return AtomicLoad_SeqCst(p_Memory);
		}
		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_FORCEINLINE
		void atomicStore(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		switch (v_Ordering) {
		case MemoryOrder::RELAXED:
			AtomicStore_Relaxed(p_Memory, v_Value);
			return;
		case MemoryOrder::SEQ_CST:
			AtomicStore_SeqCst(p_Memory, v_Value);
			return;
		case MemoryOrder::RELEASE:
			AtomicStore_Release(p_Memory, v_Value);
			return;
		case MemoryOrder::ACQ_REL:
		case MemoryOrder::ACQUIRE:
		case MemoryOrder::CONSUME:
			CORIUM_ASSERT(false && "invalid memory order for atomic store");
			CORIUM_UNREACHABLE();
		}
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic exchange") CORIUM_FORCEINLINE
		Valid atomicExchange32(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		switch (v_Ordering) {
		case MemoryOrder::ACQ_REL:
			return AtomicExchange32_AcqRel(p_Memory, v_Value);
		case MemoryOrder::RELAXED:
			return AtomicExchange32_Relaxed(p_Memory, v_Value);
		case MemoryOrder::SEQ_CST:
			return AtomicExchange32_SeqCst(p_Memory, v_Value);
		case MemoryOrder::RELEASE:
		case MemoryOrder::CONSUME:
		case MemoryOrder::ACQUIRE:
			CORIUM_ASSERT(false && "Invalid memory order for atomic exchange");
			CORIUM_UNREACHABLE();
		}
		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic exchange") CORIUM_FORCEINLINE
		Valid atomicExchange64(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		switch (v_Ordering) {
		case MemoryOrder::ACQ_REL:
			return AtomicExchange64_AcqRel(p_Memory, v_Value);
		case MemoryOrder::RELAXED:
			return AtomicExchange64_Relaxed(p_Memory, v_Value);
		case MemoryOrder::SEQ_CST:
			return AtomicExchange64_SeqCst(p_Memory, v_Value);
		case MemoryOrder::RELEASE: 
		case MemoryOrder::CONSUME: 
		case MemoryOrder::ACQUIRE:
			CORIUM_ASSERT(false && "Invalid memory order for atomic exchange");
			CORIUM_UNREACHABLE();
		}
		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic compare exchange") CORIUM_FORCEINLINE
		Valid atomicCompareExchange32(
			Valid* p_Memory,
			T* v_Expected,
			T v_Desired,
			MemoryOrder v_OrderingSuccess,
			MemoryOrder v_OrderingFailure) {
		switch (v_OrderingSuccess) {
		case MemoryOrder::RELAXED:
			switch (v_OrderingFailure) {
			case MemoryOrder::RELAXED:
				return AtomicCompareExchange32_Relaxed_Relaxed(reinterpret_cast<volatile long*>(p_Memory), v_Expected, v_Desired, false);

			case MemoryOrder::ACQUIRE:
				return AtomicCompareExchange32_Relaxed_Acquire(reinterpret_cast<volatile long*>(p_Memory), v_Expected, v_Desired, false);

			case MemoryOrder::SEQ_CST:
				return AtomicCompareExchange32_Relaxed_SeqCst(reinterpret_cast<volatile long*>(p_Memory), v_Expected, v_Desired, false);

			case MemoryOrder::RELEASE:
			case MemoryOrder::ACQ_REL:
			case MemoryOrder::CONSUME:
				break;
			}
			CORIUM_ASSERT(false && "invalid failure memory order for atomic compare exchange");
			CORIUM_UNREACHABLE();

		case MemoryOrder::ACQUIRE:
			switch (v_OrderingFailure) {
			case MemoryOrder::RELAXED: return AtomicCompareExchange32_Acquire_Relaxed(reinterpret_cast<volatile long*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::ACQUIRE: return AtomicCompareExchange32_Acquire_Acquire(reinterpret_cast<volatile long*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::SEQ_CST: return AtomicCompareExchange32_Acquire_SeqCst(reinterpret_cast<volatile long*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::RELEASE:
			case MemoryOrder::ACQ_REL:
			case MemoryOrder::CONSUME:
				break;
			}
			CORIUM_ASSERT(false && "invalid failure memory order for atomic compare exchange");
			CORIUM_UNREACHABLE();

		case MemoryOrder::RELEASE:
			switch (v_OrderingFailure) {
			case MemoryOrder::RELAXED:
				return AtomicCompareExchange32_Release_Relaxed(reinterpret_cast<volatile long*>(p_Memory), v_Expected, v_Desired, false);

			case MemoryOrder::ACQUIRE:
				return AtomicCompareExchange32_Release_Acquire(reinterpret_cast<volatile long*>(p_Memory), v_Expected, v_Desired, false);

			case MemoryOrder::SEQ_CST:
				return AtomicCompareExchange32_Release_SeqCst(reinterpret_cast<volatile long*>(p_Memory), v_Expected, v_Desired, false);

			case MemoryOrder::RELEASE:
			case MemoryOrder::ACQ_REL:
			case MemoryOrder::CONSUME:
				break;
			}
			CORIUM_ASSERT(false && "invalid failure memory order for atomic compare exchange");
			CORIUM_UNREACHABLE();

		case MemoryOrder::ACQ_REL:
			switch (v_OrderingFailure) {
			case MemoryOrder::RELAXED: return AtomicCompareExchange32_AcqRel_Relaxed(reinterpret_cast<volatile long*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::ACQUIRE: return AtomicCompareExchange32_AcqRel_Acquire(reinterpret_cast<volatile long*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::SEQ_CST: return AtomicCompareExchange32_AcqRel_SeqCst(reinterpret_cast<volatile long*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::RELEASE:
			case MemoryOrder::ACQ_REL:
			case MemoryOrder::CONSUME:
				break;
			}
			CORIUM_ASSERT(false && "invalid failure memory order for atomic compare exchange");
			CORIUM_UNREACHABLE();

		case MemoryOrder::SEQ_CST:
			switch (v_OrderingFailure) {
			case MemoryOrder::RELAXED: return AtomicCompareExchange32_SeqCst_Relaxed(reinterpret_cast<volatile long*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::ACQUIRE: return AtomicCompareExchange32_SeqCst_Acquire(reinterpret_cast<volatile long*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::SEQ_CST: return AtomicCompareExchange32_SeqCst_SeqCst(reinterpret_cast<volatile long*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::RELEASE:
			case MemoryOrder::ACQ_REL:
			case MemoryOrder::CONSUME:
				break;
			}
			CORIUM_ASSERT(false && "invalid failure memory order for atomic compare exchange");
			CORIUM_UNREACHABLE();

		case MemoryOrder::CONSUME:
			CORIUM_ASSERT(false && "invalid success memory order for atomic compare exchange");
			CORIUM_UNREACHABLE();
		}

		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic compare exchange") CORIUM_FORCEINLINE
		Valid atomicCompareExchange64(
			Valid* p_Memory,
			T* v_Expected,
			T v_Desired,
			MemoryOrder v_OrderingSuccess,
			MemoryOrder v_OrderingFailure) {
		switch (v_OrderingSuccess) {
			// ---------------- RELAXED ----------------
		case MemoryOrder::RELAXED:
			switch (v_OrderingFailure) {
			case MemoryOrder::RELAXED: return AtomicCompareExchange64_Relaxed_Relaxed(reinterpret_cast<volatile __int64*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::ACQUIRE: return AtomicCompareExchange64_Relaxed_Acquire(reinterpret_cast<volatile __int64*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::SEQ_CST: return AtomicCompareExchange64_Relaxed_SeqCst(reinterpret_cast<volatile __int64*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::RELEASE:
			case MemoryOrder::ACQ_REL:
			case MemoryOrder::CONSUME:
				break;
			}
			CORIUM_ASSERT(false && "invalid failure memory order for atomic compare exchange");
			CORIUM_UNREACHABLE();

			// ---------------- ACQUIRE ----------------
		case MemoryOrder::ACQUIRE:
			switch (v_OrderingFailure) {
			case MemoryOrder::RELAXED: return AtomicCompareExchange64_Acquire_Relaxed(reinterpret_cast<volatile __int64*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::ACQUIRE: return AtomicCompareExchange64_Acquire_Acquire(reinterpret_cast<volatile __int64*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::SEQ_CST: return AtomicCompareExchange64_Acquire_SeqCst(reinterpret_cast<volatile __int64*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::RELEASE:
			case MemoryOrder::ACQ_REL:
			case MemoryOrder::CONSUME:
				break;
			}
			CORIUM_ASSERT(false && "invalid failure memory order for atomic compare exchange");
			CORIUM_UNREACHABLE();

			// ---------------- RELEASE ----------------
		case MemoryOrder::RELEASE:
			switch (v_OrderingFailure) {
			case MemoryOrder::RELAXED: return AtomicCompareExchange64_Release_Relaxed(reinterpret_cast<volatile __int64*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::ACQUIRE: return AtomicCompareExchange64_Release_Acquire(reinterpret_cast<volatile __int64*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::SEQ_CST: return AtomicCompareExchange64_Release_SeqCst(reinterpret_cast<volatile __int64*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::RELEASE:
			case MemoryOrder::ACQ_REL:
			case MemoryOrder::CONSUME:
				break;
			}
			CORIUM_ASSERT(false && "invalid failure memory order for atomic compare exchange");
			CORIUM_UNREACHABLE();

			// ---------------- ACQ_REL ----------------
		case MemoryOrder::ACQ_REL:
			switch (v_OrderingFailure) {
			case MemoryOrder::RELAXED: return AtomicCompareExchange64_AcqRel_Relaxed(reinterpret_cast<volatile __int64*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::ACQUIRE: return AtomicCompareExchange64_AcqRel_Acquire(reinterpret_cast<volatile __int64*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::SEQ_CST: return AtomicCompareExchange64_AcqRel_SeqCst(reinterpret_cast<volatile __int64*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::RELEASE:
			case MemoryOrder::ACQ_REL:
			case MemoryOrder::CONSUME:
				break;
			}
			CORIUM_ASSERT(false && "invalid failure memory order for atomic compare exchange");
			CORIUM_UNREACHABLE();

			// ---------------- SEQ_CST ----------------
		case MemoryOrder::SEQ_CST:
			switch (v_OrderingFailure) {
			case MemoryOrder::RELAXED:return AtomicCompareExchange64_SeqCst_Relaxed(reinterpret_cast<volatile __int64*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::ACQUIRE: return AtomicCompareExchange64_SeqCst_Acquire(reinterpret_cast<volatile __int64*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::SEQ_CST: return AtomicCompareExchange64_SeqCst_SeqCst(reinterpret_cast<volatile __int64*>(p_Memory), v_Expected, v_Desired, false);
			case MemoryOrder::RELEASE:
			case MemoryOrder::ACQ_REL:
			case MemoryOrder::CONSUME:
				break;
			}
			CORIUM_ASSERT(false && "invalid failure memory order for atomic compare exchange");
			CORIUM_UNREACHABLE();

		case MemoryOrder::CONSUME:
			CORIUM_ASSERT(false && "invalid success memory order for atomic compare exchange");
			CORIUM_UNREACHABLE();
		}

		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch add") CORIUM_FORCEINLINE
		Valid atomicFetchAdd32(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		auto* raw = reinterpret_cast<volatile long*>(p_Memory);
		switch (v_Ordering) {
		case MemoryOrder::RELAXED: return static_cast<Valid>(AtomicFetchAdd32_Relaxed(raw, v_Value));
		case MemoryOrder::ACQUIRE: return static_cast<Valid>(AtomicFetchAdd32_Acquire(raw, v_Value));
		case MemoryOrder::RELEASE: return static_cast<Valid>(AtomicFetchAdd32_Release(raw, v_Value));
		case MemoryOrder::ACQ_REL: return static_cast<Valid>(AtomicFetchAdd32_AcqRel(raw, v_Value));
		case MemoryOrder::SEQ_CST: return static_cast<Valid>(AtomicFetchAdd32_SeqCst(raw, v_Value));

		case MemoryOrder::CONSUME: CORIUM_UNREACHABLE();
		}
		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch") CORIUM_FORCEINLINE
		Valid atomicFetchAdd64(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		auto* raw = reinterpret_cast<volatile __int64*>(p_Memory);
		switch (v_Ordering) {
		case MemoryOrder::RELAXED:return static_cast<Valid>(AtomicFetchAdd64_Relaxed(raw, v_Value));
		case MemoryOrder::ACQUIRE:return static_cast<Valid>(AtomicFetchAdd64_Acquire(raw, v_Value));
		case MemoryOrder::RELEASE:return static_cast<Valid>(AtomicFetchAdd64_Release(raw, v_Value));
		case MemoryOrder::ACQ_REL:return static_cast<Valid>(AtomicFetchAdd64_AcqRel(raw, v_Value));
		case MemoryOrder::SEQ_CST:return static_cast<Valid>(AtomicFetchAdd64_SeqCst(raw, v_Value));
		case MemoryOrder::CONSUME: CORIUM_UNREACHABLE();
		}

		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic increment") CORIUM_FORCEINLINE
		Valid atomicIncrement32(Valid* p_Memory, MemoryOrder v_Ordering) {
		auto* raw = reinterpret_cast<volatile long*>(p_Memory);
		switch (v_Ordering) {
		case MemoryOrder::RELAXED:return static_cast<Valid>(AtomicIncrement32_Relaxed(raw));
		case MemoryOrder::ACQUIRE:return static_cast<Valid>(AtomicIncrement32_Acquire(raw));
		case MemoryOrder::RELEASE:return static_cast<Valid>(AtomicIncrement32_Release(raw));
		case MemoryOrder::ACQ_REL:return static_cast<Valid>(AtomicIncrement32_AcqRel(raw));
		case MemoryOrder::SEQ_CST:return static_cast<Valid>(AtomicIncrement32_SeqCst(raw));
		case MemoryOrder::CONSUME: CORIUM_UNREACHABLE();
		}

		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic increment") CORIUM_FORCEINLINE
		Valid atomicIncrement64(Valid* p_Memory, MemoryOrder v_Ordering) {
		auto* raw = reinterpret_cast<volatile __int64*>(p_Memory);
		switch (v_Ordering) {
		case MemoryOrder::RELAXED:return static_cast<Valid>(AtomicIncrement64_Relaxed(raw));
		case MemoryOrder::ACQUIRE:return static_cast<Valid>(AtomicIncrement64_Acquire(raw));
		case MemoryOrder::RELEASE:return static_cast<Valid>(AtomicIncrement64_Release(raw));
		case MemoryOrder::ACQ_REL:return static_cast<Valid>(AtomicIncrement64_AcqRel(raw));
		case MemoryOrder::SEQ_CST:return static_cast<Valid>(AtomicIncrement64_SeqCst(raw));
		case MemoryOrder::CONSUME: CORIUM_UNREACHABLE();
		}

		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic decrement") CORIUM_FORCEINLINE
		Valid atomicDecrement32(Valid* p_Memory, MemoryOrder v_Ordering) {
		auto* raw = reinterpret_cast<volatile long*>(p_Memory);
		switch (v_Ordering) {
		case MemoryOrder::RELAXED:return static_cast<Valid>(AtomicDecrement32_Relaxed(raw));
		case MemoryOrder::ACQUIRE:return static_cast<Valid>(AtomicDecrement32_Acquire(raw));
		case MemoryOrder::RELEASE:return static_cast<Valid>(AtomicDecrement32_Release(raw));
		case MemoryOrder::ACQ_REL:return static_cast<Valid>(AtomicDecrement32_AcqRel(raw));
		case MemoryOrder::SEQ_CST:return static_cast<Valid>(AtomicDecrement32_SeqCst(raw));
		case MemoryOrder::CONSUME: CORIUM_UNREACHABLE();
		}

		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic decrement") CORIUM_FORCEINLINE
		Valid atomicDecrement64(Valid* p_Memory, MemoryOrder v_Ordering) {
		auto* raw = reinterpret_cast<volatile __int64*>(p_Memory);
		switch (v_Ordering) {
		case MemoryOrder::RELAXED:return static_cast<Valid>(AtomicDecrement64_Relaxed(raw));
		case MemoryOrder::ACQUIRE:return static_cast<Valid>(AtomicDecrement64_Acquire(raw));
		case MemoryOrder::RELEASE:return static_cast<Valid>(AtomicDecrement64_Release(raw));
		case MemoryOrder::ACQ_REL:return static_cast<Valid>(AtomicDecrement64_AcqRel(raw));
		case MemoryOrder::SEQ_CST:return static_cast<Valid>(AtomicDecrement64_SeqCst(raw));
		case MemoryOrder::CONSUME: CORIUM_UNREACHABLE();
		}

		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch AND") CORIUM_FORCEINLINE
		Valid atomicFetchAnd(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		switch (v_Ordering) {
		case MemoryOrder::RELAXED:
			return AtomicFetchAnd_Relaxed(p_Memory, v_Value);

		case MemoryOrder::ACQUIRE:
			return AtomicFetchAnd_Acquire(p_Memory, v_Value);

		case MemoryOrder::RELEASE:
			return AtomicFetchAnd_Release(p_Memory, v_Value);

		case MemoryOrder::ACQ_REL:
			return AtomicFetchAnd_AcqRel(p_Memory, v_Value);

		case MemoryOrder::SEQ_CST:
			return AtomicFetchAnd_SeqCst(p_Memory, v_Value);

		case MemoryOrder::CONSUME:
			CORIUM_UNREACHABLE();
		}

		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch OR") CORIUM_FORCEINLINE
		Valid atomicFetchOr(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		switch (v_Ordering) {
		case MemoryOrder::RELAXED:
			return AtomicFetchOr_Relaxed(p_Memory, v_Value);

		case MemoryOrder::ACQUIRE:
			return AtomicFetchOr_Acquire(p_Memory, v_Value);

		case MemoryOrder::RELEASE:
			return AtomicFetchOr_Release(p_Memory, v_Value);

		case MemoryOrder::ACQ_REL:
			return AtomicFetchOr_AcqRel(p_Memory, v_Value);

		case MemoryOrder::SEQ_CST:
			return AtomicFetchOr_SeqCst(p_Memory, v_Value);

		case MemoryOrder::CONSUME:
			CORIUM_UNREACHABLE();
		}

		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch XOR") CORIUM_FORCEINLINE
		Valid atomicFetchXor(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		switch (v_Ordering) {
		case MemoryOrder::RELAXED:
			return AtomicFetchXor_Relaxed(p_Memory, v_Value);

		case MemoryOrder::ACQUIRE:
			return AtomicFetchXor_Acquire(p_Memory, v_Value);

		case MemoryOrder::RELEASE:
			return AtomicFetchXor_Release(p_Memory, v_Value);

		case MemoryOrder::ACQ_REL:
			return AtomicFetchXor_AcqRel(p_Memory, v_Value);

		case MemoryOrder::SEQ_CST:
			return AtomicFetchXor_SeqCst(p_Memory, v_Value);

		case MemoryOrder::CONSUME:
			CORIUM_UNREACHABLE();
		}

		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch NAND") CORIUM_FORCEINLINE
		Valid atomicFetchNand(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		switch (v_Ordering) {
		case MemoryOrder::RELAXED:
			return AtomicFetchNand_Relaxed(p_Memory, v_Value);

		case MemoryOrder::ACQUIRE:
			return AtomicFetchNand_Acquire(p_Memory, v_Value);

		case MemoryOrder::RELEASE:
			return AtomicFetchNand_Release(p_Memory, v_Value);

		case MemoryOrder::ACQ_REL:
			return AtomicFetchNand_AcqRel(p_Memory, v_Value);

		case MemoryOrder::SEQ_CST:
			return AtomicFetchNand_SeqCst(p_Memory, v_Value);

		case MemoryOrder::CONSUME:
			CORIUM_UNREACHABLE();
		}

		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch min long") CORIUM_FORCEINLINE
		Valid atomicMinFetchLong(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		switch (v_Ordering) {
		case MemoryOrder::RELAXED:
			return AtomicMinFetchLong_Relaxed(p_Memory, v_Value);

		case MemoryOrder::ACQUIRE:
			return AtomicMinFetchLong_Acquire(p_Memory, v_Value);

		case MemoryOrder::RELEASE:
			return AtomicMinFetchLong_Release(p_Memory, v_Value);

		case MemoryOrder::ACQ_REL:
			return AtomicMinFetchLong_AcqRel(p_Memory, v_Value);

		case MemoryOrder::SEQ_CST:
			return AtomicMinFetchLong_SeqCst(p_Memory, v_Value);

		case MemoryOrder::CONSUME:
			CORIUM_UNREACHABLE();
		}

		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch max long") CORIUM_FORCEINLINE
		Valid atomicMaxFetchLong(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		switch (v_Ordering) {
		case MemoryOrder::RELAXED:
			return AtomicMaxFetchLong_Relaxed(p_Memory, v_Value);

		case MemoryOrder::ACQUIRE:
			return AtomicMaxFetchLong_Acquire(p_Memory, v_Value);

		case MemoryOrder::RELEASE:
			return AtomicMaxFetchLong_Release(p_Memory, v_Value);

		case MemoryOrder::ACQ_REL:
			return AtomicMaxFetchLong_AcqRel(p_Memory, v_Value);

		case MemoryOrder::SEQ_CST:
			return AtomicMaxFetchLong_SeqCst(p_Memory, v_Value);

		case MemoryOrder::CONSUME:
			CORIUM_UNREACHABLE();
		}

		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch min unsigned long") CORIUM_FORCEINLINE
		Valid atomicMinFetchULong(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		switch (v_Ordering) {
		case MemoryOrder::RELAXED:
			return AtomicMinFetchULong_Relaxed(p_Memory, v_Value);

		case MemoryOrder::ACQUIRE:
			return AtomicMinFetchULong_Acquire(p_Memory, v_Value);

		case MemoryOrder::RELEASE:
			return AtomicMinFetchULong_Release(p_Memory, v_Value);

		case MemoryOrder::ACQ_REL:
			return AtomicMinFetchULong_AcqRel(p_Memory, v_Value);

		case MemoryOrder::SEQ_CST:
			return AtomicMinFetchULong_SeqCst(p_Memory, v_Value);

		case MemoryOrder::CONSUME:
			CORIUM_UNREACHABLE();
		}

		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch max unsigned long") CORIUM_FORCEINLINE
		Valid atomicMaxFetchULong(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		switch (v_Ordering) {
		case MemoryOrder::RELAXED:
			return AtomicMaxFetchULong_Relaxed(p_Memory, v_Value);

		case MemoryOrder::ACQUIRE:
			return AtomicMaxFetchULong_Acquire(p_Memory, v_Value);

		case MemoryOrder::RELEASE:
			return AtomicMaxFetchULong_Release(p_Memory, v_Value);

		case MemoryOrder::ACQ_REL:
			return AtomicMaxFetchULong_AcqRel(p_Memory, v_Value);

		case MemoryOrder::SEQ_CST:
			return AtomicMaxFetchULong_SeqCst(p_Memory, v_Value);

		case MemoryOrder::CONSUME:
			CORIUM_UNREACHABLE();
		}

		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch test and set") CORIUM_FORCEINLINE
		bool atomicTestAndSet(Valid* p_Memory, int bit, MemoryOrder v_Ordering) {
		switch (v_Ordering) {
		case MemoryOrder::RELAXED:
			return AtomicTestAndSet_Relaxed(p_Memory, bit);

		case MemoryOrder::ACQUIRE:
			return AtomicTestAndSet_Acquire(p_Memory, bit);

		case MemoryOrder::RELEASE:
			return AtomicTestAndSet_Release(p_Memory, bit);

		case MemoryOrder::ACQ_REL:
			return AtomicTestAndSet_AcqRel(p_Memory, bit);

		case MemoryOrder::SEQ_CST:
			return AtomicTestAndSet_SeqCst(p_Memory, bit);

		case MemoryOrder::CONSUME:
			CORIUM_UNREACHABLE();
		}

		CORIUM_UNREACHABLE();
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic clear") CORIUM_FORCEINLINE
		bool atomicClear(Valid* p_Memory, int bit, MemoryOrder v_Ordering) {
		switch (v_Ordering) {
		case MemoryOrder::RELAXED:
			return AtomicClear_Relaxed(p_Memory, bit);

		case MemoryOrder::ACQUIRE:
			return AtomicClear_Acquire(p_Memory, bit);

		case MemoryOrder::RELEASE:
			return AtomicClear_Release(p_Memory, bit);

		case MemoryOrder::ACQ_REL:
			return AtomicClear_AcqRel(p_Memory, bit);

		case MemoryOrder::SEQ_CST:
			return AtomicClear_SeqCst(p_Memory, bit);

		case MemoryOrder::CONSUME:
			CORIUM_UNREACHABLE();
		}

		CORIUM_UNREACHABLE();
	}
}
