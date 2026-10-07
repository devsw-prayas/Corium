cmake_minimum_required(VERSION 3.20)

# No vcpkg dependencies; opt out of a user-wide `vcpkg integrate install` (its post-build step needs pwsh).
if(NOT DEFINED CMAKE_VS_GLOBALS)
    set(CMAKE_VS_GLOBALS "VcpkgEnabled=false")
endif()

project(Corium LANGUAGES CXX)

# CUDA defaults to whatever the machine has, so a host without the toolkit still configures.
include(CheckLanguage)
check_language(CUDA)
if (CMAKE_CUDA_COMPILER)
    set(_corium_cuda_default ON)
else()
    set(_corium_cuda_default OFF)
endif()
option(CORIUM_ENABLE_CUDA "Enable CUDA GPU backend" ${_corium_cuda_default})
option(CORIUM_BUILD_SHARED "Build Corium as a shared library (DLL/SO); OFF builds a static library" ON)

if (CORIUM_ENABLE_CUDA)
    if (NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
        set(CMAKE_CUDA_ARCHITECTURES 80 86) # Supports only Ampere for now
    endif()
    enable_language(CUDA)
endif()

# cl.exe has no x64 inline asm, so the context switch is MASM there; clang-cl (compiler ID "Clang")
# uses the naked-function version in CoriumFrameSwitchClang.cpp instead.
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    enable_language(ASM_MASM)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

file(GLOB_RECURSE CORIUM_PUBLIC    CONFIGURE_DEPENDS "src/Public/*.h")
file(GLOB_RECURSE CORIUM_PRIVATE   CONFIGURE_DEPENDS "src/Private/*.cpp")
file(GLOB_RECURSE CORIUM_INTERNAL  CONFIGURE_DEPENDS "src/Internal/*.h")

# Other toolchains get the same symbol from the Clang TU via the *.cpp glob.
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    file(GLOB_RECURSE CORIUM_ASM CONFIGURE_DEPENDS "src/Private/*.asm")
    set_source_files_properties(${CORIUM_ASM} PROPERTIES LANGUAGE ASM_MASM)
else()
    set(CORIUM_ASM "")
endif()

if (CORIUM_ENABLE_CUDA)
    file(GLOB_RECURSE CORIUM_CUDA CONFIGURE_DEPENDS
        "src/Cuda/*.cu"
        "src/Cuda/*.cuh"
        "src/Cuda/*.cpp"
        "src/Cuda/*.h"
    )
endif()

if (CORIUM_BUILD_SHARED)
    set(CORIUM_LIB_TYPE SHARED)
else()
    set(CORIUM_LIB_TYPE STATIC)
endif()

add_library(Corium ${CORIUM_LIB_TYPE}
    ${CORIUM_PUBLIC}
    ${CORIUM_PRIVATE}
    ${CORIUM_INTERNAL}
    ${CORIUM_ASM}
    $<$<BOOL:${CORIUM_ENABLE_CUDA}>:${CORIUM_CUDA}>
)

# Unconditional: source_group only maps IDE folders, it doesn't compile anything.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES
    ${CORIUM_PUBLIC}
    ${CORIUM_PRIVATE}
    ${CORIUM_INTERNAL}
    ${CORIUM_ASM}
    ${CORIUM_CUDA}
)

# PUBLIC: Corium's headers need C++20, so consumers get it too.
target_compile_features(Corium PUBLIC cxx_std_20)
set_target_properties(Corium PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(Corium PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Public
)

target_include_directories(Corium PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Internal
)

if (CORIUM_ENABLE_CUDA)
    find_package(CUDAToolkit REQUIRED)
    target_include_directories(Corium PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Cuda/Public
    )
    target_link_libraries(Corium PRIVATE CUDA::cuda_driver)
    # CUDA 13's cuda.lib pulls in LIBCMT; everything else uses the DLL CRT.
    target_link_options(Corium PRIVATE $<$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>:/NODEFAULTLIB:LIBCMT>)
    target_compile_definitions(Corium PRIVATE CORIUM_CUDA_AVAILABLE)
    set_target_properties(Corium PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_STANDARD 20
    )
endif()

set_target_properties(Corium PROPERTIES FOLDER "Base")

target_precompile_headers(Corium PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Public/Corium.h
)

if (CORIUM_BUILD_SHARED)
    target_compile_definitions(Corium PRIVATE CORIUM_SHARED)
    unset(CORIUM_STATIC)
else()
    set(CORIUM_STATIC ON)
endif()

# Tuning values land in a generated CoriumConfig.h, so consumers that don't use this CMake still see them.
set(CORIUM_MAX_NUMA                 4      CACHE STRING "Maximum supported NUMA nodes")
set(CORIUM_VA_ALLOCATION            524288 CACHE STRING "Total VA reservation in MiB")
set(CORIUM_TIMEOUT                  50     CACHE STRING "Sync wait timeout")
set(CORIUM_SPIN_COUNT               450    CACHE STRING "Spins before a sync primitive parks")
set(CORIUM_DEFAULT_OS_TLS_SIZE      2      CACHE STRING "Default OS TLS size in MiB")
set(CORIUM_SPACE_TLS_MIN_SIZE       2      CACHE STRING "Minimum per-thread TLS arena in MiB")
set(CORIUM_SPACE_TLS_MAX_SIZE       100    CACHE STRING "Maximum per-thread TLS arena in MiB")
# Per-node TLS VA region in MiB; capped at 10 GiB by a static_assert in CoriumAddrSpace.h.
set(CORIUM_SPACE_TLS_TOTAL          4096   CACHE STRING "Per-node TLS VA region in MiB")
set(CORIUM_DEFAULT_FRAME_STACK_SIZE 2      CACHE STRING "Default pooled frame stack in MiB")

# NativeFrame register blob in bytes, fixed by the calling convention: Win64 also saves XMM6-15 plus the
# TEB stack bounds and x87 control word (288 B used), SysV saves no XMM (68 B used, rounded up to 16 so the
# stack after it stays aligned). Not a cache variable: it must match the switch.
if(WIN32)
    set(CORIUM_FRAME_REG_BLOB_SIZE 288)
else()
    set(CORIUM_FRAME_REG_BLOB_SIZE 80)
endif()

set(CORIUM_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
configure_file(src/Public/CoriumConfig.h.in ${CORIUM_GENERATED_DIR}/CoriumConfig.h @ONLY)
target_include_directories(Corium PUBLIC ${CORIUM_GENERATED_DIR})
target_sources(Corium PRIVATE ${CORIUM_GENERATED_DIR}/CoriumConfig.h)

# Only CORIUM_RUNTIME_API symbols leave the shared object on ELF, matching the DLL export list.
set_target_properties(Corium PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# AUTO follows _DEBUG (set by the debug CRT); ON/OFF pin it. Corium-scoped, so it never collides with a host's DEBUG.
set(CORIUM_ENABLE_DEBUG_CHECKS AUTO CACHE STRING "Corium asserts: AUTO (follow _DEBUG), ON or OFF")
set_property(CACHE CORIUM_ENABLE_DEBUG_CHECKS PROPERTY STRINGS AUTO ON OFF)
if (CORIUM_ENABLE_DEBUG_CHECKS STREQUAL "ON")
    target_compile_definitions(Corium PUBLIC CORIUM_DEBUG_CHECKS=1)
elseif (CORIUM_ENABLE_DEBUG_CHECKS STREQUAL "OFF")
    target_compile_definitions(Corium PUBLIC CORIUM_DEBUG_CHECKS=0)
endif()

# Fiber-safe TLS: frames migrate between threads.
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(Corium PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/GT>)
endif()

# Only a standalone default: a host's CMAKE_MSVC_RUNTIME_LIBRARY knows its own config names and wins.
if (MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set_property(TARGET Corium PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
endif()