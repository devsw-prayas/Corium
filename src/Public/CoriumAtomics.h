#pragma once
#include "PlatIntrin.h"
#include "Atomics.h"

// Runtime-order shim over Corium::Atomics. The signatures are the legacy ones (order as a runtime argument, Valid* operands)
// because ThreadSupport's type-erased wrappers and AtomicVar.h still pass orders around as values; every call is a
// switch that lands in the compile-time-order templates, so a constant argument folds away after inlining.
// Legacy behaviour preserved on purpose:
//   - atomicCompareExchange* returns the OBSERVED value and leaves *v_Expected untouched (callers compare against it).
//   - atomicIncrement* / atomicDecrement* return the NEW value, the other fetch* return the PREVIOUS value.
//   - atomicTestAndSet / atomicClear return the bit's value before the operation.

namespace Corium::Core::Atomics {
	namespace Fast = ::Corium::Atomics;

	using MemoryOrder = Fast::MemoryOrder;

	namespace detail {
		// Resolves one order to a compile-time constant. A branch whose order the callable rejects (via its requires clause)
		// is an invalid order for that operation: assert in debug, undefined in release, same as the legacy switches.
		template<typename R, MemoryOrder O, typename F>
		CORIUM_FORCEINLINE R pick(F& r_Fn) {
			if constexpr (requires { r_Fn.template operator()<O>(); }) {
				return r_Fn.template operator()<O>();
			} else {
				CORIUM_ASSERT(false && "invalid memory order for this atomic operation");
				CORIUM_UNREACHABLE();
			}
		}

		template<typename R, typename F>
		CORIUM_FORCEINLINE R withOrder(MemoryOrder v_Order, F&& u_Fn) {
			switch (v_Order) {
			case MemoryOrder::RELAXED: return pick<R, MemoryOrder::RELAXED>(u_Fn);
			case MemoryOrder::CONSUME: return pick<R, MemoryOrder::CONSUME>(u_Fn);
			case MemoryOrder::ACQUIRE: return pick<R, MemoryOrder::ACQUIRE>(u_Fn);
			case MemoryOrder::RELEASE: return pick<R, MemoryOrder::RELEASE>(u_Fn);
			case MemoryOrder::ACQ_REL: return pick<R, MemoryOrder::ACQ_REL>(u_Fn);
			case MemoryOrder::SEQ_CST: return pick<R, MemoryOrder::SEQ_CST>(u_Fn);
			}
			CORIUM_UNREACHABLE();
		}

		// Read-modify-write ops never took CONSUME.
		template<MemoryOrder O>
		concept RmwOrder = O != MemoryOrder::CONSUME;
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic load") CORIUM_FORCEINLINE
		Valid atomicLoad(Valid* p_Memory, MemoryOrder v_Ordering) {
		// A load has no release semantics; the legacy code promoted those to seq_cst.
		if (v_Ordering == MemoryOrder::RELEASE || v_Ordering == MemoryOrder::ACQ_REL) v_Ordering = MemoryOrder::SEQ_CST;
		return detail::withOrder<Valid>(v_Ordering, [&]<MemoryOrder O>() requires Fast::LoadOrder<O> {
			return Fast::load<O>(p_Memory);
		});
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_FORCEINLINE
		void atomicStore(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		detail::withOrder<void>(v_Ordering, [&]<MemoryOrder O>() requires Fast::StoreOrder<O> {
			Fast::store<O>(p_Memory, static_cast<Valid>(v_Value));
		});
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic exchange") CORIUM_FORCEINLINE
		Valid atomicExchange32(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		static_assert(sizeof(Valid) <= 4, "atomicExchange32 needs an operand of at most 4 bytes");
		return detail::withOrder<Valid>(v_Ordering, [&]<MemoryOrder O>() requires detail::RmwOrder<O> {
			return Fast::exchange<O>(p_Memory, static_cast<Valid>(v_Value));
		});
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic exchange") CORIUM_FORCEINLINE
		Valid atomicExchange64(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		static_assert(sizeof(Valid) == 8, "atomicExchange64 needs an 8-byte operand");
		return detail::withOrder<Valid>(v_Ordering, [&]<MemoryOrder O>() requires detail::RmwOrder<O> {
			return Fast::exchange<O>(p_Memory, static_cast<Valid>(v_Value));
		});
	}

	namespace detail {
		// Success order is resolved first, then the failure order; the legacy matrix accepted RELAXED/ACQUIRE/SEQ_CST failures
		// and every success order except CONSUME.
		template<typename Valid, typename T>
		CORIUM_FORCEINLINE Valid compareExchangeImpl(Valid* p_Memory, T* p_Expected, T v_Desired, MemoryOrder v_Success, MemoryOrder v_Failure) {
			return withOrder<Valid>(v_Success, [&]<MemoryOrder S>() requires RmwOrder<S> {
				return withOrder<Valid>(v_Failure, [&]<MemoryOrder F>() requires (F == MemoryOrder::RELAXED || F == MemoryOrder::ACQUIRE || F == MemoryOrder::SEQ_CST) {
					Valid observed = static_cast<Valid>(*p_Expected);
					(void)Fast::compareExchange<S, F>(p_Memory, observed, static_cast<Valid>(v_Desired));
					return observed;
				});
			});
		}
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic compare exchange") CORIUM_FORCEINLINE
		Valid atomicCompareExchange32(Valid* p_Memory, T* v_Expected, T v_Desired, MemoryOrder v_OrderingSuccess, MemoryOrder v_OrderingFailure) {
		static_assert(sizeof(Valid) <= 4, "atomicCompareExchange32 needs an operand of at most 4 bytes");
		return detail::compareExchangeImpl(p_Memory, v_Expected, v_Desired, v_OrderingSuccess, v_OrderingFailure);
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic compare exchange") CORIUM_FORCEINLINE
		Valid atomicCompareExchange64(Valid* p_Memory, T* v_Expected, T v_Desired, MemoryOrder v_OrderingSuccess, MemoryOrder v_OrderingFailure) {
		static_assert(sizeof(Valid) == 8, "atomicCompareExchange64 needs an 8-byte operand");
		return detail::compareExchangeImpl(p_Memory, v_Expected, v_Desired, v_OrderingSuccess, v_OrderingFailure);
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch add") CORIUM_FORCEINLINE
		Valid atomicFetchAdd32(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		static_assert(sizeof(Valid) <= 4, "atomicFetchAdd32 needs an operand of at most 4 bytes");
		return detail::withOrder<Valid>(v_Ordering, [&]<MemoryOrder O>() requires detail::RmwOrder<O> {
			return Fast::fetchAdd<O>(p_Memory, static_cast<Valid>(v_Value));
		});
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch add") CORIUM_FORCEINLINE
		Valid atomicFetchAdd64(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		static_assert(sizeof(Valid) == 8, "atomicFetchAdd64 needs an 8-byte operand");
		return detail::withOrder<Valid>(v_Ordering, [&]<MemoryOrder O>() requires detail::RmwOrder<O> {
			return Fast::fetchAdd<O>(p_Memory, static_cast<Valid>(v_Value));
		});
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic increment") CORIUM_FORCEINLINE
		Valid atomicIncrement32(Valid* p_Memory, MemoryOrder v_Ordering) {
		static_assert(sizeof(Valid) <= 4, "atomicIncrement32 needs an operand of at most 4 bytes");
		return detail::withOrder<Valid>(v_Ordering, [&]<MemoryOrder O>() requires detail::RmwOrder<O> {
			return Fast::increment<O>(p_Memory);
		});
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic increment") CORIUM_FORCEINLINE
		Valid atomicIncrement64(Valid* p_Memory, MemoryOrder v_Ordering) {
		static_assert(sizeof(Valid) == 8, "atomicIncrement64 needs an 8-byte operand");
		return detail::withOrder<Valid>(v_Ordering, [&]<MemoryOrder O>() requires detail::RmwOrder<O> {
			return Fast::increment<O>(p_Memory);
		});
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic decrement") CORIUM_FORCEINLINE
		Valid atomicDecrement32(Valid* p_Memory, MemoryOrder v_Ordering) {
		static_assert(sizeof(Valid) <= 4, "atomicDecrement32 needs an operand of at most 4 bytes");
		return detail::withOrder<Valid>(v_Ordering, [&]<MemoryOrder O>() requires detail::RmwOrder<O> {
			return Fast::decrement<O>(p_Memory);
		});
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic decrement") CORIUM_FORCEINLINE
		Valid atomicDecrement64(Valid* p_Memory, MemoryOrder v_Ordering) {
		static_assert(sizeof(Valid) == 8, "atomicDecrement64 needs an 8-byte operand");
		return detail::withOrder<Valid>(v_Ordering, [&]<MemoryOrder O>() requires detail::RmwOrder<O> {
			return Fast::decrement<O>(p_Memory);
		});
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch AND") CORIUM_FORCEINLINE
		Valid atomicFetchAnd(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		return detail::withOrder<Valid>(v_Ordering, [&]<MemoryOrder O>() requires detail::RmwOrder<O> {
			return Fast::fetchAnd<O>(p_Memory, static_cast<Valid>(v_Value));
		});
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch OR") CORIUM_FORCEINLINE
		Valid atomicFetchOr(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		return detail::withOrder<Valid>(v_Ordering, [&]<MemoryOrder O>() requires detail::RmwOrder<O> {
			return Fast::fetchOr<O>(p_Memory, static_cast<Valid>(v_Value));
		});
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch XOR") CORIUM_FORCEINLINE
		Valid atomicFetchXor(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		return detail::withOrder<Valid>(v_Ordering, [&]<MemoryOrder O>() requires detail::RmwOrder<O> {
			return Fast::fetchXor<O>(p_Memory, static_cast<Valid>(v_Value));
		});
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch NAND") CORIUM_FORCEINLINE
		Valid atomicFetchNand(Valid* p_Memory, T v_Value, MemoryOrder v_Ordering) {
		return detail::withOrder<Valid>(v_Ordering, [&]<MemoryOrder O>() requires detail::RmwOrder<O> {
			return Fast::fetchNand<O>(p_Memory, static_cast<Valid>(v_Value));
		});
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic fetch test and set") CORIUM_FORCEINLINE
		bool atomicTestAndSet(Valid* p_Memory, int bit, MemoryOrder v_Ordering) {
		return detail::withOrder<bool>(v_Ordering, [&]<MemoryOrder O>() requires detail::RmwOrder<O> {
			return Fast::bitTestAndSet<O>(p_Memory, static_cast<unsigned>(bit));
		});
	}

	template<typename T, typename Valid = Intrinsic::ValidAtomicParameter<T>::Type>
	CORIUM_NODISCARD_MSG("Cannot discard an atomic clear") CORIUM_FORCEINLINE
		bool atomicClear(Valid* p_Memory, int bit, MemoryOrder v_Ordering) {
		return detail::withOrder<bool>(v_Ordering, [&]<MemoryOrder O>() requires detail::RmwOrder<O> {
			return Fast::bitTestAndReset<O>(p_Memory, static_cast<unsigned>(bit));
		});
	}
}
