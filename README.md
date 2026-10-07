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
| `CORIUM_ENABLE_DEBUG_CHECKS` | `ON` | Enables `CORIUM_ASSERT`/`CORIUM_DEBUG_ASSERT`. |

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

Every thread launched through Corium gets a dedicated bump-allocated TLS arena, reclaimed automatically on thread exit:

```cpp
using namespace Corium::Core;

MyObject* obj = this_thread::create<MyObject>(/* ctor args */);
// ...
this_thread::destroy(obj);
```

## License

MIT License. See [LICENSE](LICENSE) for details.
