#pragma once

#include <cstddef>

// Declaration shape from DecFIGS memoryfunctions.h; ARTIST's global table has
// these three function-pointer slots in malloc/realloc/free order.
typedef void* (*RwMallocFunc)(std::size_t);
typedef void* (*RwReallocFunc)(void*, std::size_t);
typedef void (*RwFreeFunc)(void*);

struct RwMemoryFunctions
{
    RwMallocFunc rwmalloc;
    RwReallocFunc rwrealloc;
    RwFreeFunc rwfree;
};

extern RwMemoryFunctions rwMemoryFunctions;

void* RwMallocMemStub(std::size_t lnSize);
void* RwReallocMemStub(void* lpMemory, std::size_t lnSize);
void RwFreeMemStub(void* lpMemory);

// ARTIST's aligned callbacks share the table at 0x82F91A04.
// The unaligned callback table starts at 0x82F91A10.
typedef void* (*RwMallocAlignedFunc)(std::size_t, std::size_t);
typedef void* (*RwReallocAlignedFunc)(void*, std::size_t, std::size_t);
typedef void (*RwFreeAlignedFunc)(void*);

struct RwAlignedMemoryFunctions
{
    RwMallocAlignedFunc rwmalloc;
    RwReallocAlignedFunc rwrealloc;
    RwFreeAlignedFunc rwfree;
};

extern RwAlignedMemoryFunctions rwAlignedMemoryFunctions;

void* RwMallocMemAlignedStub(std::size_t lnSize, std::size_t lnAlignment);
void* RwReallocMemAlignedStub(void* lpMemory, std::size_t lnSize, std::size_t lnAlignment);
void RwFreeMemAlignedStub(void* lpMemory);
