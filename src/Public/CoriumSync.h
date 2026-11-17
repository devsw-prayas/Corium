#pragma once
#include "AtomicVar.h"
#include "ThreadUtils.h"
#include "CoriumChrono.h"
#include "CoriumThread.h"

// Some of my favorites from java.util.concurrent now in C++!
// I'm lazy to fix some bugs, once that's done I'll add the TODOS :)

namespace Corium::Runtime::Sync {
	using namespace Core;

	class CORIUM_RUNTIME_API CountDownLatch final {
		// We are using ParkHandle here since all the NativeThread wait-on-address and wake-on-address
		// implemenations are defined on ParkHandle
		Core::ParkHandle m_Counter;

	public:
		explicit CountDownLatch(uint32_t v_Count);

		CountDownLatch(const CountDownLatch&) = delete;
		CountDownLatch& operator=(const CountDownLatch&) = delete;
		CountDownLatch(CountDownLatch&&)            noexcept = default;
		CountDownLatch& operator=(CountDownLatch&&) noexcept = default;

		void countDown();
		void await();
		bool await(Chrono::Instant v_Deadline);
		uint32_t getCount() const;
	};

	class CORIUM_RUNTIME_API CyclicBarrier final {
		Core::ParkHandle m_Generation;
		const uint32_t m_Parties;
		Core::Atomic::AtomicValue32<uint32_t> m_Remaining;
		Core::Atomic::AtomicValue32<bool> m_IsBroken;
	public:
		explicit CyclicBarrier(uint32_t v_Parties);

		CyclicBarrier(const CyclicBarrier&) = delete;
		CyclicBarrier& operator=(const CyclicBarrier&) = delete;

		uint32_t await();
		bool await(Chrono::Instant v_Deadline);
		void reset();
		bool isBroken() const;
		uint32_t getParties() const;
		uint32_t getNumberWaiting() const;
	};

	class CORIUM_RUNTIME_API Semaphore final {
		// We are using ParkHandle here since all the NativeThread wait-on-address and wake-on-address
		// implemenations are defined on ParkHandle
		Core::ParkHandle m_Permits;
	public:
		explicit Semaphore(uint32_t v_Permits);

		Semaphore(const Semaphore&) = delete;
		Semaphore& operator=(const Semaphore&) = delete;

		void acquire();
		void acquire(uint32_t v_Permits);
		bool tryAcquire();
		bool tryAcquire(Chrono::Instant v_Deadline);
		void release();
		void release(uint32_t v_Permits);
		uint32_t availablePermits() const;
	};

	// OS-backed recursive mutex. Fast kernel-assisted user-space path.
	// No timed tryLock — use ReentrantLock when you need deadlines.
	class CORIUM_RUNTIME_API CriticalSection final {
	public:
		static constexpr size_t STORAGE_SIZE = 64;
	private:
		alignas(8) uint8_t m_Storage[STORAGE_SIZE];
	public:
		CriticalSection() noexcept;
		~CriticalSection();
		CriticalSection(const CriticalSection&) = delete;
		CriticalSection& operator=(const CriticalSection&) = delete;
		CriticalSection(CriticalSection&&) = delete;
		CriticalSection& operator=(CriticalSection&&) = delete;

		void lock()    noexcept;
		void unlock()  noexcept;
		bool tryLock() noexcept;
	};

	// Custom recursive lock built on WaitOnAddress. Supports timed tryLock.
	class CORIUM_RUNTIME_API ReentrantLock final {
		Core::Atomic::AtomicValue32<uint32_t> m_OwnerTid{ 0 };
		Core::Atomic::AtomicValue32<uint32_t> m_HoldCount{ 0 };
		Core::Atomic::AtomicValue32<uint32_t> m_Gate{ 0 };
	public:
		ReentrantLock()  noexcept = default;
		~ReentrantLock() = default;
		ReentrantLock(const ReentrantLock&) = delete;
		ReentrantLock& operator=(const ReentrantLock&) = delete;

		void     lock()                              noexcept;
		void     unlock()                            noexcept;
		bool     tryLock()                           noexcept;
		bool     tryLock(Chrono::Instant v_Deadline) noexcept;
		bool     isHeldByCurrentThread()       const noexcept;
		uint32_t getHoldCount()                const noexcept;
	};

	// Concurrent readers or exclusive writers.
	// State encoding: 0 = unlocked, UINT32_MAX = write locked, N = N active readers.
	class CORIUM_RUNTIME_API ReadWriteLock final {
		Core::Atomic::AtomicValue32<uint32_t> m_State{ 0 };
		Core::Atomic::AtomicValue32<uint32_t> m_Gate{ 0 };
	public:
		ReadWriteLock()  noexcept = default;
		~ReadWriteLock() = default;
		ReadWriteLock(const ReadWriteLock&) = delete;
		ReadWriteLock& operator=(const ReadWriteLock&) = delete;

		void lockRead()                               noexcept;
		bool tryLockRead()                            noexcept;
		bool tryLockRead(Chrono::Instant v_Deadline)  noexcept;
		void unlockRead()                             noexcept;

		void lockWrite()                              noexcept;
		bool tryLockWrite()                           noexcept;
		bool tryLockWrite(Chrono::Instant v_Deadline) noexcept;
		void unlockWrite()                            noexcept;

		CORIUM_NODISCARD bool     isWriteLocked()    const noexcept;
		CORIUM_NODISCARD uint32_t getReadLockCount() const noexcept;
	};

	// Optimistic read variant of ReadWriteLock. tryOptimisticRead() returns a stamp
	// with no blocking; validate(stamp) confirms no writer intervened.
	// Stamps are not reentrant.
	class CORIUM_RUNTIME_API StampedLock final {
		Core::Atomic::AtomicValue64<uint64_t> m_State;
		Core::Atomic::AtomicValue32<uint32_t> m_Gate{ 0 };
	public:
		StampedLock() noexcept : m_State(0x100ULL), m_Gate(0) {}
		~StampedLock() = default;
		StampedLock(const StampedLock&) = delete;
		StampedLock& operator=(const StampedLock&) = delete;

		CORIUM_NODISCARD uint64_t writeLock()                          noexcept;
		bool tryWriteLock(uint64_t& ro_Stamp)     noexcept;
		void                      unlockWrite(uint64_t v_Stamp)        noexcept;

		CORIUM_NODISCARD uint64_t readLock()                           noexcept;
		bool                      tryReadLock(uint64_t& ro_Stamp)      noexcept;
		void                      unlockRead(uint64_t v_Stamp)         noexcept;

		CORIUM_NODISCARD uint64_t tryOptimisticRead()                  noexcept;
		CORIUM_NODISCARD bool     validate(uint64_t v_Stamp)     const noexcept;

		CORIUM_NODISCARD uint64_t tryConvertToWriteLock(uint64_t v_Stamp) noexcept;
		CORIUM_NODISCARD uint64_t tryConvertToReadLock(uint64_t v_Stamp)  noexcept;
	};

	// Dynamic barrier. Parties register and deregister at runtime.
	// Supports multiple phases and hierarchical coordination via parent phasers.
	// onAdvance() returning true terminates the phaser.
	class CORIUM_RUNTIME_API Phaser {
		mutable CriticalSection               m_Lock;
		Core::Atomic::AtomicValue32<uint32_t> m_Gate{ 0 };
		Core::Atomic::AtomicValue32<uint32_t> m_Phase{ 0 };
		Core::Atomic::AtomicValue32<uint32_t> m_Terminated{ 0 };
		uint32_t m_Registered{ 0 };
		uint32_t m_Arrived{ 0 };
		Phaser* m_pParent{ nullptr };

		void doAdvance() noexcept;

	public:
		Phaser()                                   noexcept = default;
		explicit Phaser(uint32_t v_Parties)        noexcept;
		Phaser(Phaser& ro_Parent, uint32_t v_Parties) noexcept;
		virtual ~Phaser() = default;

		Phaser(const Phaser&) = delete;
		Phaser& operator=(const Phaser&) = delete;
		Phaser(Phaser&&) = delete;
		Phaser& operator=(Phaser&&) = delete;

		uint32_t register_()                                           noexcept;
		uint32_t bulkRegister(uint32_t v_Parties)                      noexcept;
		uint32_t arrive()                                              noexcept;
		uint32_t arriveAndDeregister()                                 noexcept;
		uint32_t arriveAndAwaitAdvance()                               noexcept;
		uint32_t awaitAdvance(uint32_t v_Phase)                        noexcept;
		bool     awaitAdvance(uint32_t v_Phase, Chrono::Instant v_Deadline) noexcept;
		void     forceTermination()                                    noexcept;

		CORIUM_NODISCARD uint32_t getPhase()              const noexcept;
		CORIUM_NODISCARD uint32_t getRegisteredParties()  const noexcept;
		CORIUM_NODISCARD uint32_t getArrivedParties()     const noexcept;
		CORIUM_NODISCARD uint32_t getUnarrivedParties()   const noexcept;
		CORIUM_NODISCARD bool     isTerminated()          const noexcept;

	protected:
		virtual bool onAdvance(uint32_t v_Phase, uint32_t v_RegisteredParties) noexcept;
	};

	// Condition variable companion to ReentrantLock.
	// await() reads the sequence counter while holding the lock, fully releases,
	// then parks on WaitOnAddress — a signal that fires between release and park
	// is not lost because the counter already changed.
	class CORIUM_RUNTIME_API Condition final {
		ReentrantLock& m_Lock;
		Core::Atomic::AtomicValue32<uint32_t> m_Seq{ 0 };
	public:
		explicit Condition(ReentrantLock& ro_Lock) noexcept : m_Lock(ro_Lock) {}
		~Condition() = default;
		Condition(const Condition&) = delete;
		Condition& operator=(const Condition&) = delete;

		void await()                           noexcept;
		bool await(Chrono::Instant v_Deadline) noexcept;
		void signal()                          noexcept;
		void signalAll()                       noexcept;
	};

	// Two-thread rendezvous with value swap. Blocks until both parties arrive.
	// Per-thread slots from g_RuntimeVA via GeneralAllocator. T must be movable.
	namespace Internal {
		// Out of line so the Exchanger template doesn't pull the allocator registry into public headers.
		CORIUM_RUNTIME_API uint32_t exchangerSlotCount() noexcept;
		CORIUM_RUNTIME_API void* allocateExchangerSlots(size_t v_Bytes, size_t v_Align) noexcept;
		CORIUM_RUNTIME_API void freeExchangerSlots(void* p_Memory, size_t v_Bytes) noexcept;
	}

	template<typename T>
	class Exchanger final {
		// FILLING and TAKING fence the buffers: a slot is only WAITING once the waiter's value is constructed,
		// and only FULFILLED once the partner's value is.
		static constexpr uint32_t SLOT_EMPTY = 0u;
		static constexpr uint32_t SLOT_FILLING = 1u;
		static constexpr uint32_t SLOT_WAITING = 2u;
		static constexpr uint32_t SLOT_TAKING = 3u;
		static constexpr uint32_t SLOT_FULFILLED = 4u;

		struct alignas(64) Slot {
			Core::Atomic::AtomicValue32<uint32_t> m_State{ SLOT_EMPTY };
			// Bumped after FULFILLED is published; the waiter parks on the value it read before checking state.
			Core::ParkHandle                       m_Gate{ 0u };
			alignas(alignof(T)) uint8_t            m_WaiterBuf[sizeof(T)];
			alignas(alignof(T)) uint8_t            m_FulfillerBuf[sizeof(T)];
		};

		Slot* m_pSlots{ nullptr };
		uint32_t m_SlotCount{ 0 };

		CORIUM_FORCEINLINE uint32_t pickSlot() const noexcept {
			const uintptr_t address = reinterpret_cast<uintptr_t>(
				Core::this_thread::currentPermit().m_ParkingPermit.data());
			return static_cast<uint32_t>((address >> 6) % m_SlotCount);
		}

		CORIUM_FORCEINLINE static bool transition(Slot& r_Slot, uint32_t v_From, uint32_t v_To) noexcept {
			uint32_t expected = v_From;
			return r_Slot.m_State.compareExchange(&expected, v_To,
				Core::Atomics::MemoryOrder::ACQ_REL, Core::Atomics::MemoryOrder::RELAXED) == v_From;
		}

		bool tryFulfill(uint32_t v_Start, T& r_Value, T& ro_Result) {
			for (uint32_t i = 0; i < m_SlotCount; ++i) {
				Slot& slot = m_pSlots[(v_Start + i) % m_SlotCount];
				if (!transition(slot, SLOT_WAITING, SLOT_TAKING)) continue;
				T* waiter = reinterpret_cast<T*>(slot.m_WaiterBuf);
				ro_Result = std::move(*waiter);
				waiter->~T();
				new (slot.m_FulfillerBuf) T(std::move(r_Value));
				slot.m_State.store(SLOT_FULFILLED, Core::Atomics::MemoryOrder::RELEASE);
				slot.m_Gate.m_ParkingPermit.increment(Core::Atomics::MemoryOrder::ACQ_REL);
				Core::ParkingSupport support(&slot.m_Gate);
				Core::NativeThread::wakeOnAddress(support);
				return true;
			}
			return false;
		}

		void collect(Slot& r_Slot, T& ro_Result) {
			T* fulfiller = reinterpret_cast<T*>(r_Slot.m_FulfillerBuf);
			ro_Result = std::move(*fulfiller);
			fulfiller->~T();
			r_Slot.m_State.store(SLOT_EMPTY, Core::Atomics::MemoryOrder::RELEASE);
		}

		enum class Outcome : uint8_t { EXCHANGED, TIMED_OUT, RETRY };

		// Wakes waiters on lower slots so they re-scan; true when a waiter sits on a higher slot.
		// Lower waiters yield to higher ones: symmetric "both withdraw" livelocks, and a waiter that parked before
		// the other arrived would never look again, hence the kick.
		bool scanOtherWaiters(const Slot& ro_Own) noexcept {
			const uint32_t own = static_cast<uint32_t>(&ro_Own - m_pSlots);
			bool higher = false;
			for (uint32_t i = 0; i < m_SlotCount; ++i) {
				if (i == own) continue;
				Slot& other = m_pSlots[i];
				if (other.m_State.load(Core::Atomics::MemoryOrder::SEQ_CST) != SLOT_WAITING) continue;
				if (i > own) { higher = true; continue; }
				other.m_Gate.m_ParkingPermit.increment(Core::Atomics::MemoryOrder::ACQ_REL);
				Core::ParkingSupport support(&other.m_Gate);
				Core::NativeThread::wakeOnAddress(support);
			}
			return higher;
		}

		// Withdraw through FILLING so no partner or new waiter touches the buffer while it is emptied.
		bool withdraw(Slot& r_Slot, T& r_Value) {
			if (!transition(r_Slot, SLOT_WAITING, SLOT_FILLING)) return false;
			T* waiter = reinterpret_cast<T*>(r_Slot.m_WaiterBuf);
			r_Value = std::move(*waiter);
			waiter->~T();
			r_Slot.m_State.store(SLOT_EMPTY, Core::Atomics::MemoryOrder::RELEASE);
			return true;
		}

		// TIMED_OUT and RETRY both mean the offer was withdrawn and r_Value holds the value again.
		Outcome awaitPartner(Slot& r_Slot, T& r_Value, T& ro_Result, const Chrono::Instant* p_Deadline) {
			Core::ParkingSupport support(&r_Slot.m_Gate);
			for (;;) {
				const uint32_t gate = r_Slot.m_Gate.m_ParkingPermit.load(Core::Atomics::MemoryOrder::ACQUIRE);
				const uint32_t state = r_Slot.m_State.load(Core::Atomics::MemoryOrder::ACQUIRE);
				if (state == SLOT_FULFILLED) break;
				// TAKING means a partner is mid-handoff: wait it out even past the deadline, it finishes shortly.
				if (state != SLOT_WAITING) { Intrinsic::Pause(); continue; }
				if (p_Deadline && p_Deadline->isExpired()) {
					if (withdraw(r_Slot, r_Value)) return Outcome::TIMED_OUT;
					continue;
				}
				// Waiters only sit in their own slot and nobody but a fulfiller scans, so two waiters in different
				// slots would park forever. The lower one withdraws and goes back to fulfilling.
				if (scanOtherWaiters(r_Slot)) {
					if (withdraw(r_Slot, r_Value)) return Outcome::RETRY;
					continue;
				}
				if (p_Deadline) Core::NativeThread::waitOnAddressFor(support, *p_Deadline, gate);
				else Core::NativeThread::waitOnAddress(support, gate);
			}
			collect(r_Slot, ro_Result);
			return Outcome::EXCHANGED;
		}

		bool exchangeImpl(T& r_Value, T& ro_Result, const Chrono::Instant* p_Deadline) {
			const uint32_t start = pickSlot();
			for (;;) {
				// Retried every round: a partner may have published WAITING after our last scan.
				if (tryFulfill(start, r_Value, ro_Result)) return true;
				if (p_Deadline && p_Deadline->isExpired()) return false;
				Slot& slot = m_pSlots[start];
				if (transition(slot, SLOT_EMPTY, SLOT_FILLING)) {
					new (slot.m_WaiterBuf) T(std::move(r_Value));
					// SEQ_CST, paired with the SEQ_CST loads in scanOtherWaiters: two waiters each publish then read the
					// other slot, and under release/acquire x86 may let both reads pass both stores, so both park.
					slot.m_State.store(SLOT_WAITING, Core::Atomics::MemoryOrder::SEQ_CST);
					const Outcome outcome = awaitPartner(slot, r_Value, ro_Result, p_Deadline);
					if (outcome != Outcome::RETRY) return outcome == Outcome::EXCHANGED;
				}
				Intrinsic::Pause();
			}
		}

	public:
		Exchanger() {
			m_SlotCount = Internal::exchangerSlotCount();
			void* memory = Internal::allocateExchangerSlots(sizeof(Slot) * m_SlotCount, alignof(Slot));
			CORIUM_ASSERT(memory);
			m_pSlots = static_cast<Slot*>(memory);
			for (uint32_t i = 0; i < m_SlotCount; ++i)
				new (&m_pSlots[i]) Slot{};
		}

		~Exchanger() {
			if (m_pSlots) {
				for (uint32_t i = 0; i < m_SlotCount; ++i)
					m_pSlots[i].~Slot();
				Internal::freeExchangerSlots(m_pSlots, sizeof(Slot) * m_SlotCount);
			}
		}

		Exchanger(const Exchanger&) = delete;
		Exchanger& operator=(const Exchanger&) = delete;
		Exchanger(Exchanger&&) = delete;
		Exchanger& operator=(Exchanger&&) = delete;

		T exchange(T&& v_Value) {
			T result{};
			CORIUM_UNUSED(exchangeImpl(v_Value, result, nullptr));
			return result;
		}

		// On timeout v_Value is handed back intact, since no partner took it.
		bool exchange(T&& v_Value, T& ro_Result, Chrono::Instant v_Deadline) {
			return exchangeImpl(v_Value, ro_Result, &v_Deadline);
		}
	};
}
