# Corium

A high-performance, modern multithreading runtime for C++.

Corium is the concurrency backbone for [SpectraRenderer](https://github.com/devsw-prayas/Spectra), and is designed to be used independently as well.

## Build

### Requirements

- Windows 10/11 (x64) or Linux (x86_64)
- Visual Studio 2022 (MSVC) on Windows, GCC 12+ or Clang 14+ on Linux
- CMake 3.20+
- CUDA Toolkit 13.2+ (only if `CORIUM_ENABLE_CUDA` is on)

> **Note:** Linux support is still under development. Windows is the primary, fully-supported target; several components have Linux code paths stubbed out (`TODO`) rather than implemented.

### Standalone

**Windows:**
```bash
git clone https://github.com/devsw-prayas/Corium.git
cd Corium
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

**Linux:**
```bash
git clone https://github.com/devsw-prayas/Corium.git
cd Corium
mkdir build && cd build
cmake .. -G "Ninja"
cmake --build . --config Release
```

### CMake options

| Option | Default | Effect |
|---|---|---|
| `CORIUM_ENABLE_CUDA` | `ON` | Builds the CUDA GPU backend under `src/Cuda/`. |
| `CORIUM_BUILD_SHARED` | `ON` | Builds Corium as a shared library (DLL/SO). `OFF` builds a static library — the same headers work unchanged either way. |
| `CORIUM_ENABLE_DEBUG_CHECKS` | `AUTO` | Controls `CORIUM_ASSERT`/`CORIUM_DEBUG_ASSERT`. `AUTO` follows `_DEBUG`; `ON`/`OFF` force them on or off. |

### As part of SpectraRenderer

Corium is a git submodule in `common/Corium`. Follow the [SpectraRenderer build instructions](https://github.com/devsw-prayas/SpectraRenderer).

## Starting the runtime

Everything in Corium depends on the runtime being booted once, before anything else runs:

```cpp
#include <CoriumRuntime.h>

Corium::CoriumRuntime::initRuntime();
```

This probes CPU/NUMA topology, reserves per-node virtual address space, and wires up the node-local allocators. Safe to call more than once — every call after the first is a no-op.

## Launching a thread

### Via a thread factory (recommended)

`Corium::Core::Factory` provides a small hierarchy for spawning threads under different placement policies:

```cpp
#include <CoriumFactory.h>
#include <CoriumUtility.h>

Corium::Core::Factory::DefaultThreadFactory factory;

auto closure = Corium::Core::Utils::makeClosure<void()>([]() {
    // runs on the new thread
}, /* preferred NUMA node */ 0u);

Corium::Core::ThreadHandle handle = factory.createAndStart(std::move(closure), "MyThread");

Corium::Core::NativeThread::joinThread(handle);
Corium::Core::NativeThread::closeHandle(handle);
```

| Factory | Placement |
|---|---|
| `DefaultThreadFactory` | Node 0, no affinity mask, normal priority. |
| `AffinityFactory` | Explicit NUMA node + processor affinity mask. |
| `PriorityFactory` | `AffinityFactory` plus an explicit `ThreadPriority`. |

### Via raw descriptors (lower level)

Factories are a thin convenience layer over `ThreadAttrDesc`/`ThreadLaunchDesc`, which can also be built and validated directly:

```cpp
#include <CoriumThread.h>
#include <ThreadUtils.h>

using namespace Corium::Core;

ThreadAttrDesc attrDesc{};
init(attrDesc);
setNumaNode(attrDesc, 0u);
validate(attrDesc);

ThreadLaunchDesc launchDesc{};
init(launchDesc);
setName(launchDesc, "MyThread");
attachLaunchAddr(launchDesc, std::move(closure));
validate(launchDesc);

ThreadHandle handle = NativeThread::createThread(std::move(launchDesc), attrDesc);
```

## Thread-local storage

Every thread launched through Corium gets a dedicated bump-allocated TLS arena, reclaimed as a whole on thread exit:

```cpp
using namespace Corium::Core;

MyObject* obj = this_thread::create<MyObject>(/* ctor args */);
// ...
this_thread::destroy(obj); // runs the destructor; the memory goes back with the arena
```

### Foreign threads

Threads Corium did not launch (the main thread, threads from another library) must be adopted before they use TLS, sync primitives or frames:

```cpp
if (Corium::Core::this_thread::attach(/* NUMA node */ 0u)) {
    // ...
    Corium::Core::this_thread::detach();
}
```

Attach is counted, so pair every `attach` with a `detach`. The TLS slice is never reclaimed: attach once per thread lifetime, not once per task.

## Frames (user-mode context switching)

`Corium::Core::Frame::NativeFrame` runs closures on their own stacks and switches between them cooperatively on the same thread.

```cpp
#include <CoriumFrame.h>

using namespace Corium::Core::Frame;

FrameStackDesc desc{};
init(desc);
setStackSize(desc, 256 * 1024); // 64 KiB .. 8 MiB
setNumaNode(desc, 0u);
validate(desc);                // freezes the desc; createFrame requires a frozen desc

auto frame = NativeFrame::createFrame(FrameEntry{ /* closure */ }, desc);
if (frame) {
    this_frame::captureFrame(*frame);   // runs until the frame yields or terminates
}
```

| Call | Effect |
|---|---|
| `switchTo` / `trySwitchTo` | Switch from one frame to another (`try` fails instead of switching when the target can't be claimed). |
| `yieldToNative` | Return to the thread's native context, optionally terminating the frame. |
| `killFrame` | Wipe a suspended frame and leave a blank `TERMINATED` handle. It is never resumed. |
| `this_frame::currentFrame` | The frame running on this thread. Closures take no arguments, so this is how a frame finds itself. |

## Synchronization

`Corium::Runtime::Sync` provides `java.util.concurrent`-style primitives built on wait-on-address (`ParkHandle`), not kernel objects:

| Primitive | Use |
|---|---|
| `CountDownLatch` | Wait until N events have happened. |
| `CyclicBarrier` | N parties meet at a reusable rendezvous point. |
| `Semaphore` | Counting permits. |
| `CriticalSection` | OS-backed recursive mutex. No timed lock. |
| `ReentrantLock` | Custom recursive lock with timed `tryLock`. |
| `ReadWriteLock` | Many readers or one writer. |
| `StampedLock` | Read-write lock with optimistic, non-blocking reads (`tryOptimisticRead` + `validate`). |
| `Phaser` | Dynamic-party, multi-phase barrier. |
| `Condition` | Condition variable. |
| `Exchanger` | Two threads swap values. |

Every blocking call has a deadline overload that takes a `Chrono::Instant`:

```cpp
#include <CoriumSync.h>

using namespace Corium::Core::Chrono::Literals;

Corium::Runtime::Sync::CountDownLatch latch(4);
// workers call latch.countDown();
bool done = latch.await(Corium::Core::Chrono::until(500_ms));
```

## Time

`Corium::Core::Chrono` holds nanosecond-resolution `Interval` (a duration) and `Instant` (a monotonic point in time). Create an `Instant` with `MonotonicClock::now()` or `until(interval)`. The `Literals` namespace provides `_ns`, `_us`, `_ms`, `_s` and `_min`.

## Atomics

Corium uses its own atomics in place of `<atomic>`, with MSVC and GCC/Clang intrinsic backends:

- `Corium::Atomics::AtomicValue<T>` wraps any 1-, 2-, 4- or 8-byte word, with no padding (`sizeof(AtomicValue<T>) == sizeof(T)`). The memory order is a template argument, checked at compile time against what the operation allows:
  ```cpp
  Corium::Atomics::AtomicValue<uint32_t> counter{ 0 };
  using Corium::Atomics::MemoryOrder;
  counter.fetchAdd<MemoryOrder::RELAXED>(1);
  uint32_t v = counter.load<MemoryOrder::ACQUIRE>();
  ```
- `Corium::Core::Atomic::AtomicValue32` / `AtomicValue64` / `AtomicPointer` are fixed-width variants used throughout the runtime.

## Smart pointers

`Corium::Memory` provides allocator-aware ownership types. None of them call `new`/`delete`:

| Type | Notes |
|---|---|
| `UniquePtr<T, TAllocator>` | Sole owner. `view()` hands out a non-owning `UniqueView<T>`. |
| `SharedPtr<T, TAllocator>` | Ref-counted. Create with `make(objAlloc, ctrlAlloc, args...)` or `adopt(ptr, objAlloc, ctrlAlloc)`. Control blocks always come from `ControlBlockAllocator`. |
| `WeakPtr<T, TAllocator>` | Non-owning. `lock()` returns a `SharedPtr` if the object is still alive. |

`TAllocator` defaults to `GeneralAllocator`.

## Task pipeline

`Corium::Execution` is the task system. It is in progress:

- **Inductor** (`Inductor.h`): describes tasks and their memory (`TaskDesc`, `TaskMemoryDesc`). Done.
- **Builder** (`Builder.h`): `TaskBuilder` assembles tasks into a pipeline. Done.
- **Executor**: not started yet.

`ChaseLevDeque.h` provides the work-stealing deque that the executor will build on.

## CUDA

With `CORIUM_ENABLE_CUDA=ON`, `src/Cuda/` provides thin RAII wrappers over the CUDA driver API:

| Header | Covers |
|---|---|
| `CudaBootstrap.h` | Driver init (`CudaDriver::initCuda`) and device queries (`CudaDeviceManager`). |
| `CudaContextManager.h` | Contexts. |
| `CudaDeviceMemory.h`, `CudaPinnedMemory.h`, `CudaVirtualMemory.h` | Device, pinned host and virtual-memory allocations. |
| `CudaStream.h`, `CudaEvent.h`, `CudaGraph.h` | Streams, events and graphs. |

## License

MIT License. See [LICENSE](LICENSE) for details.
