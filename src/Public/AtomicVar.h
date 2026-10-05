#pragma once
#include "CoriumAtomics.h"
#include "PlatIntrin.h"

namespace Corium::Core::Atomic {
	template<typename T>
	struct AtomicValue32 final {
	private:
		using Valid = Intrinsic::ValidAtomicParameter<T>::Type;

		CORIUM_ALIGNAS(sizeof(Valid)) Valid m_Value;

	public:

		AtomicValue32() noexcept = default;
		~AtomicValue32() = default;

		explicit AtomicValue32(Valid v_Value) noexcept
			: m_Value(v_Value) {
		}

		AtomicValue32(const AtomicValue32&) = default;
		AtomicValue32& operator=(const AtomicValue32&) = default;
		AtomicValue32(AtomicValue32&&) noexcept = default;
		AtomicValue32& operator=(AtomicValue32&&) noexcept = default;

		CORIUM_FORCEINLINE
			Valid load(Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) const noexcept {
			return Atomics::atomicLoad<Valid>(
				const_cast<Valid*>(&m_Value),
				v_Ordering
			);
		}

		CORIUM_FORCEINLINE
			void store(
				Valid v_Value,
				Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			Atomics::atomicStore<Valid>(&m_Value, v_Value, v_Ordering);
		}

		CORIUM_FORCEINLINE
			Valid exchange(Valid v_Value, Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicExchange32<Valid>(&m_Value, v_Value, v_Ordering
			);
		}

		CORIUM_FORCEINLINE
			Valid compareExchange(Valid* v_Expected, Valid v_Desired, Atomics::MemoryOrder v_Success, Atomics::MemoryOrder v_Failure) noexcept {
			return Atomics::atomicCompareExchange32<Valid>(&m_Value, v_Expected, v_Desired, v_Success, v_Failure);
		}

		CORIUM_FORCEINLINE
			Valid fetchAdd(Valid v_Value, Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicFetchAdd32<Valid>(&m_Value, v_Value, v_Ordering);
		}

		CORIUM_FORCEINLINE
			Valid increment(Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicIncrement32<Valid>(&m_Value, v_Ordering
			);
		}

		CORIUM_FORCEINLINE
			Valid decrement(Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicDecrement32<Valid>(&m_Value, v_Ordering);
		}

		CORIUM_FORCEINLINE
			Valid fetchAnd(Valid v_Value, Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicFetchAnd<Valid>(&m_Value, v_Value, v_Ordering);
		}

		CORIUM_FORCEINLINE
			Valid fetchOr(Valid v_Value, Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicFetchOr<Valid>(&m_Value, v_Value, v_Ordering);
		}

		CORIUM_FORCEINLINE
			Valid fetchXor(
				Valid v_Value,
				Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicFetchXor<Valid>(&m_Value, v_Value, v_Ordering);
		}

		CORIUM_FORCEINLINE
			Valid fetchNand(
				Valid v_Value,
				Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicFetchNand<Valid>(&m_Value, v_Value, v_Ordering);
		}

		CORIUM_FORCEINLINE
			bool testAndSet(
				int bit,
				Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicTestAndSet<Valid>(&m_Value, bit, v_Ordering);
		}

		CORIUM_FORCEINLINE
			bool clear(
				int bit,
				Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicClear<Valid>(&m_Value, bit, v_Ordering);
		}

		CORIUM_FORCEINLINE Valid*       data() noexcept       { return &m_Value; }
		CORIUM_FORCEINLINE const Valid* data() const noexcept { return &m_Value; }
	};

	template<typename T>
	struct  AtomicValue64 {
	private:
		using Valid = Intrinsic::ValidAtomicParameter<T>::Type;

		CORIUM_ALIGNAS(sizeof(Valid)) Valid m_Value;

	public:

		AtomicValue64() noexcept = default;

		explicit AtomicValue64(Valid v_Value) noexcept
			: m_Value(v_Value) {
		}

		AtomicValue64(const AtomicValue64&) = default;
		AtomicValue64& operator=(const AtomicValue64&) = default;
		AtomicValue64(AtomicValue64&&) noexcept = default;
		AtomicValue64& operator=(AtomicValue64&&) noexcept = default;

		CORIUM_FORCEINLINE
			Valid load(Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) const noexcept {
			return Atomics::atomicLoad<Valid>(
				const_cast<Valid*>(&m_Value),
				v_Ordering
			);
		}

		CORIUM_FORCEINLINE
			void store(
				Valid v_Value,
				Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			Atomics::atomicStore<Valid>(&m_Value, v_Value, v_Ordering);
		}

		CORIUM_FORCEINLINE
			Valid exchange(Valid v_Value, Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicExchange64<Valid>(&m_Value, v_Value, v_Ordering
			);
		}

		CORIUM_FORCEINLINE
			Valid compareExchange(Valid *v_Expected, Valid v_Desired, Atomics::MemoryOrder v_Success, Atomics::MemoryOrder v_Failure) noexcept {
			return Atomics::atomicCompareExchange64<Valid>(&m_Value, v_Expected, v_Desired, v_Success, v_Failure);
		}

		CORIUM_FORCEINLINE
			Valid fetchAdd(Valid v_Value, Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicFetchAdd64<Valid>(&m_Value, v_Value, v_Ordering);
		}

		CORIUM_FORCEINLINE
			Valid increment(Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicIncrement64<Valid>(&m_Value, v_Ordering
			);
		}

		CORIUM_FORCEINLINE
			Valid decrement(Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicDecrement64<Valid>(&m_Value, v_Ordering);
		}

		CORIUM_FORCEINLINE
			Valid fetchAnd(Valid v_Value, Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicFetchAnd<Valid>(&m_Value, v_Value, v_Ordering);
		}

		CORIUM_FORCEINLINE
			Valid fetchOr(Valid v_Value, Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicFetchOr<Valid>(&m_Value, v_Value, v_Ordering);
		}

		CORIUM_FORCEINLINE
			Valid fetchXor(
				Valid v_Value,
				Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicFetchXor<Valid>(&m_Value, v_Value, v_Ordering);
		}

		CORIUM_FORCEINLINE
			Valid fetchNand(
				Valid v_Value,
				Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicFetchNand<Valid>(&m_Value, v_Value, v_Ordering);
		}

		CORIUM_FORCEINLINE
			bool testAndSet(
				int bit,
				Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicTestAndSet<Valid>(&m_Value, bit, v_Ordering);
		}

		CORIUM_FORCEINLINE
			bool clear(
				int bit,
				Atomics::MemoryOrder v_Ordering = Atomics::MemoryOrder::SEQ_CST) noexcept {
			return Atomics::atomicClear<Valid>(&m_Value, bit, v_Ordering);
		}

		CORIUM_FORCEINLINE Valid*       data() noexcept       { return &m_Value; }
		CORIUM_FORCEINLINE const Valid* data() const noexcept { return &m_Value; }
	};

	template<typename T>
	struct AtomicPointer final : AtomicValue64<T*> {
		using AtomicValue64<T*>::AtomicValue64;
	};
}