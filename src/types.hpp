#pragma once

#include <stddef.h>

#if !defined(_XBOX) || !defined(_M_PPCBE)
#error This experimental branch requires the Xbox 360 PowerPC toolchain.
#endif

typedef char CgsX360PointerWidthCheck[(sizeof(void*) == 4) ? 1 : -1];

typedef unsigned char u8;
typedef signed char   s8;

typedef unsigned short u16;
typedef signed short   s16;

typedef unsigned int u32;
typedef signed int   s32;

typedef unsigned __int64 u64;
typedef signed __int64   s64;

typedef char CgsX360IntegerWidthCheck[
    (sizeof(u8) == 1 && sizeof(u16) == 2 && sizeof(u32) == 4 && sizeof(u64) == 8) ? 1 : -1];

typedef float  f32;
typedef double f64;

typedef size_t usize;
