#pragma once

#include <stddef.h>

#if !defined(_XBOX) || !defined(_M_PPCBE)
#error This experimental branch requires the Xbox 360 PowerPC toolchain.
#endif

// Compile-time checks and alignment must also work with the original XDK compiler.
#define CGS_JOIN_IMPL(a, b) a##b
#define CGS_JOIN(a, b) CGS_JOIN_IMPL(a, b)
#define CGS_STATIC_ASSERT(condition, message) \
    typedef char CGS_JOIN(CgsStaticAssert_, __COUNTER__)[(condition) ? 1 : -1]
#define CGS_ALIGN(alignment) __declspec(align(alignment))

typedef char CgsX360PointerWidthCheck[(sizeof(void*) == 4) ? 1 : -1];

typedef unsigned char u8;
typedef signed char   s8;

typedef unsigned short u16;
typedef signed short   s16;

typedef unsigned int u32;
typedef signed int   s32;

typedef unsigned __int64 u64;
typedef signed __int64   s64;

// C99-width spellings used by the reconstructed middleware. The 2007 CRT does
// not provide <stdint.h>; intptr_t/uintptr_t already come from its <stddef.h>.
typedef u8  uint8_t;
typedef s8  int8_t;
typedef u16 uint16_t;
typedef s16 int16_t;
typedef u32 uint32_t;
typedef s32 int32_t;
typedef u64 uint64_t;
typedef s64 int64_t;

typedef char CgsX360IntegerWidthCheck[
    (sizeof(u8) == 1 && sizeof(u16) == 2 && sizeof(u32) == 4 && sizeof(u64) == 8) ? 1 : -1];

typedef float  f32;
typedef double f64;

typedef size_t usize;
