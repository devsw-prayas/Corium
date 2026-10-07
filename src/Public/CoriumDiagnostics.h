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
#include <CoriumCompiler.h>

// CORIUM_DEBUG_CHECKS (0/1) comes from CORIUM_ENABLE_DEBUG_CHECKS=ON/OFF; unset (AUTO) follows _DEBUG.
#if defined(CORIUM_DEBUG_CHECKS)
#define CORIUM_BUILD_DEBUG CORIUM_DEBUG_CHECKS
#elif defined(_DEBUG)
#define CORIUM_BUILD_DEBUG 1
#else
#define CORIUM_BUILD_DEBUG 0
#endif
#define CORIUM_BUILD_RELEASE (!CORIUM_BUILD_DEBUG)

// Asserts on a constant (e.g. `false && "msg"`) are intentional; MSVC flags them as C4127
// when instantiated inside templates.
#if CORIUM_COMPILER_MSVC
#define CORIUM_CONSTANT_CONDITION_BEGIN CORIUM_PRAGMA(warning(push)) CORIUM_PRAGMA(warning(disable : 4127))
#define CORIUM_CONSTANT_CONDITION_END   CORIUM_PRAGMA(warning(pop))
#else
#define CORIUM_CONSTANT_CONDITION_BEGIN
#define CORIUM_CONSTANT_CONDITION_END
#endif

#if CORIUM_BUILD_DEBUG

#define CORIUM_ASSERT(expr)                                             CORIUM_CONSTANT_CONDITION_BEGIN                                do {                                                               if (!(expr)) {                                                    CORIUM_DEBUG_BREAK();                                              CORIUM_TRAP();                                                 }                                                          } while (0)                                                    CORIUM_CONSTANT_CONDITION_END

#else

#define CORIUM_ASSERT(expr) CORIUM_CONSTANT_CONDITION_BEGIN do { (void)sizeof(expr); } while (0) CORIUM_CONSTANT_CONDITION_END

#endif

#if CORIUM_BUILD_DEBUG
#define CORIUM_ASSUME(expr) CORIUM_ASSERT(expr)
#else
// Clang first: clang-cl also defines _MSC_VER, and __assume there discards side effects with -Wassume.
#if CORIUM_COMPILER_CLANG || CORIUM_COMPILER_GCC
#define CORIUM_ASSUME(expr) do { if (!(expr)) __builtin_unreachable(); } while (0)
#elif CORIUM_COMPILER_MSVC
#define CORIUM_ASSUME(expr) __assume(expr)
#else
#define CORIUM_ASSUME(expr) do { } while (0)
#endif
#endif

#if CORIUM_BUILD_DEBUG
#define CORIUM_DEBUG_ASSERT(expr) CORIUM_ASSERT(expr)
#define CORIUM_DEBUG_ASSUME(expr) CORIUM_ASSUME(expr)
#else
#define CORIUM_DEBUG_ASSERT(expr) do {} while (0)
#define CORIUM_DEBUG_ASSUME(expr) do {} while (0)
#endif

#define CORIUM_STATIC_ASSERT(expr, msg) static_assert(expr, msg)

#define CORIUM_UNUSED(x) (void)(x)