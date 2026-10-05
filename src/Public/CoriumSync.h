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
	template<typename T>
	class Exchanger final {
		static constexpr uint32_t SLOT_EMPTY = 0u;
		static constexpr uint32_t SLOT_WAITING = 1u;
		static constexpr uint32_t SLOT_FULFILLED = 2u;

		struct alignas(64) Slot {
			Core::Atomic::AtomicValue32<uint32_t> m_State{ SLOT_EMPTY };
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

	public:
		Exchanger() {
			CORIUM_ASSERT(Memory::Internal::AllocatorRegistry::isRegistered);
			m_SlotCount = Memory::Internal::AllocatorRegistry::s_NodeCount * 8u;
			if (m_SlotCount < 8u) m_SlotCount = 8u;
			void* memory = Memory::Internal::AllocatorRegistry::s_GeneralAllocator[0]
				.allocateImpl(sizeof(Slot) * m_SlotCount, alignof(Slot));
			CORIUM_ASSERT(memory);
			m_pSlots = static_cast<Slot*>(memory);
			for (uint32_t i = 0; i < m_SlotCount; ++i)
				new (&m_pSlots[i]) Slot{};
		}

		~Exchanger() {
			if (m_pSlots) {
				for (uint32_t i = 0; i < m_SlotCount; ++i)
					m_pSlots[i].~Slot();
				Memory::Internal::AllocatorRegistry::s_GeneralAllocator[0]
					.deallocateImpl(m_pSlots, sizeof(Slot) * m_SlotCount);
			}
		}

		Exchanger(const Exchanger&) = delete;
		Exchanger& operator=(const Exchanger&) = delete;
		Exchanger(Exchanger&&) = delete;
		Exchanger& operator=(Exchanger&&) = delete;

		T exchange(T&& v_Value) {
			const uint32_t start = pickSlot();
			// Try fulfiller: scan for a WAITING slot
			for (uint32_t i = 0; i < m_SlotCount; ++i) {
				Slot& slot = m_pSlots[(start + i) % m_SlotCount];
				uint32_t expected = SLOT_WAITING;
				if (slot.m_State.compareExchange(&expected, SLOT_FULFILLED,
					Core::Atomics::MemoryOrder::ACQ_REL,
					Core::Atomics::MemoryOrder::RELAXED) == SLOT_WAITING) {
					T result = std::move(*reinterpret_cast<T*>(slot.m_WaiterBuf));
					reinterpret_cast<T*>(slot.m_WaiterBuf)->~T();
					new (slot.m_FulfillerBuf) T(std::move(v_Value));
					Core::ParkingSupport support(&slot.m_Gate);
					NativeThread::wakeOnAddress(support);
					return result;
				}
			}
			// Become waiter on preferred slot
			for (;;) {
				Slot& slot = m_pSlots[start];
				uint32_t expected = SLOT_EMPTY;
				if (slot.m_State.compareExchange(&expected, SLOT_WAITING,
					Core::Atomics::MemoryOrder::ACQ_REL,
					Core::Atomics::MemoryOrder::RELAXED) == SLOT_EMPTY) {
					new (slot.m_WaiterBuf) T(std::move(v_Value));
					while (slot.m_State.load(Core::Atomics::MemoryOrder::ACQUIRE) != SLOT_FULFILLED) {
						Core::ParkingSupport support(&slot.m_Gate);
						Core::NativeThread::waitOnAddress(support);
					}
					T result = std::move(*reinterpret_cast<T*>(slot.m_FulfillerBuf));
					reinterpret_cast<T*>(slot.m_FulfillerBuf)->~T();
					slot.m_State.store(SLOT_EMPTY, Core::Atomics::MemoryOrder::RELEASE);
					return result;
				}
				Intrinsic::Pause();
			}
		}

		bool exchange(T&& v_Value, T& ro_Result, Chrono::Instant v_Deadline) {
			const uint32_t start = pickSlot();
			// Try fulfiller: scan for a WAITING slot
			for (uint32_t i = 0; i < m_SlotCount; ++i) {
				Slot& slot = m_pSlots[(start + i) % m_SlotCount];
				uint32_t expected = SLOT_WAITING;
				if (slot.m_State.compareExchange(&expected, SLOT_FULFILLED,
					Core::Atomics::MemoryOrder::ACQ_REL,
					Core::Atomics::MemoryOrder::RELAXED) == SLOT_WAITING) {
					ro_Result = std::move(*reinterpret_cast<T*>(slot.m_WaiterBuf));
					reinterpret_cast<T*>(slot.m_WaiterBuf)->~T();
					new (slot.m_FulfillerBuf) T(std::move(v_Value));
					Core::ParkingSupport support(&slot.m_Gate);
					Core::NativeThread::wakeOnAddress(support);
					return true;
				}
			}
			// Become waiter on preferred slot
			for (;;) {
				if (v_Deadline.isExpired()) return false;
				Slot& slot = m_pSlots[start];
				uint32_t expected = SLOT_EMPTY;
				if (slot.m_State.compareExchange(&expected, SLOT_WAITING,
					Core::Atomics::MemoryOrder::ACQ_REL,
					Core::Atomics::MemoryOrder::RELAXED) == SLOT_EMPTY) {
					new (slot.m_WaiterBuf) T(std::move(v_Value));
					while (slot.m_State.load(Core::Atomics::MemoryOrder::ACQUIRE) != SLOT_FULFILLED) {
						if (v_Deadline.isExpired()) {
							// Try to cancel — only succeeds if fulfiller hasn't raced us
							uint32_t waiting = SLOT_WAITING;
							if (slot.m_State.compareExchange(&waiting, SLOT_EMPTY,
								Core::Atomics::MemoryOrder::ACQ_REL,
								Core::Atomics::MemoryOrder::RELAXED) == SLOT_WAITING) {
								reinterpret_cast<T*>(slot.m_WaiterBuf)->~T();
								return false;
							}
							break; // fulfiller raced us — fall through to collect result
						}
						Core::ParkingSupport support(&slot.m_Gate);
						Core::NativeThread::waitOnAddressFor(support, v_Deadline);
					}
					ro_Result = std::move(*reinterpret_cast<T*>(slot.m_FulfillerBuf));
					reinterpret_cast<T*>(slot.m_FulfillerBuf)->~T();
					slot.m_State.store(SLOT_EMPTY, Core::Atomics::MemoryOrder::RELEASE);
					return true;
				}
				Intrinsic::Pause();
			}
		}
	};
}
