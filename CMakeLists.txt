cmake_minimum_required(VERSION 3.20)

option(CORIUM_ENABLE_CUDA "Enable CUDA GPU backend" ON)
option(CORIUM_BUILD_SHARED "Build Corium as a shared library (DLL/SO); OFF builds a static library" ON)

if (CORIUM_ENABLE_CUDA)
    project(Corium LANGUAGES CXX CUDA)
    set(CMAKE_CUDA_ARCHITECTURES 80 86) # Supports only Ampere for now
else()
    project(Corium LANGUAGES CXX)
endif()

# cl.exe has no x64 inline asm, so the context switch is MASM there; clang-cl (compiler ID "Clang")
# uses the naked-function version in CoriumFrameSwitchClang.cpp instead.
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    enable_language(ASM_MASM)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
else()
    target_compile_definitions(Corium PUBLIC CORIUM_STATIC)
endif()
target_compile_definitions(Corium PUBLIC CORIUM_MAX_NUMA=4)
target_compile_definitions(Corium PUBLIC CORIUM_VA_ALLOCATION=524288)
target_compile_definitions(Corium PUBLIC CORIUM_TIMEOUT=50)
target_compile_definitions(Corium PUBLIC CORIUM_SPIN_COUNT=450)
target_compile_definitions(Corium PUBLIC CORIUM_DEFAULT_OS_TLS_SIZE=2)

target_compile_definitions(Corium PUBLIC  CORIUM_SPACE_TLS_MIN_SIZE=2)
target_compile_definitions(Corium PUBLIC  CORIUM_SPACE_TLS_MAX_SIZE=100)

# Per-node TLS VA region in MiB; capped at 10 GiB by a static_assert in CoriumAddrSpace.h.
target_compile_definitions(Corium PUBLIC  CORIUM_SPACE_TLS_TOTAL=4096)

target_compile_definitions(Corium PUBLIC  CORIUM_DEFAULT_FRAME_STACK_SIZE=2)

option(CORIUM_ENABLE_DEBUG_CHECKS "Enable Corium debug asserts/checks" ON)
target_compile_definitions(Corium PUBLIC $<$<BOOL:${CORIUM_ENABLE_DEBUG_CHECKS}>:DEBUG>)

if (MSVC)
    set_property(TARGET Corium PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
endif()