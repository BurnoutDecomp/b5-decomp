#pragma once

#include <cstdint>
#include <cstddef>

#if !defined(_XBOX) || !defined(_M_PPCBE)
#error This experimental branch requires the Xbox 360 PowerPC toolchain.
#endif

static_assert(sizeof(void*) == 4, "Xbox 360 uses 32-bit pointers");

typedef uint8_t u8;
typedef int8_t  s8;

typedef uint16_t u16;
typedef int16_t  s16;

typedef uint32_t u32;
typedef int32_t  s32;

typedef uint64_t u64;
typedef int64_t  s64;

typedef float  f32;
typedef double f64;

typedef size_t usize;
