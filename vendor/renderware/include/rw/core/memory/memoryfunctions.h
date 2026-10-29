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

// ARTIST's separate aligned callback slots82F91A04/08/0C. The unaligned
// rwMemoryFunctions object at82F91A10 remains its original three-slot table.
typedef void* (*RwMallocAlignedFunc)(std::size_t, std::size_t);
typedef void* (*RwReallocAlignedFunc)(void*, std::size_t, std::size_t);
typedef void (*RwFreeAlignedFunc)(void*);

extern RwMallocAlignedFunc RwMallocMemAligned;
extern RwReallocAlignedFunc RwReallocMemAligned;
extern RwFreeAlignedFunc RwFreeMemAligned;

void* RwMallocMemAlignedStub(std::size_t lnSize, std::size_t lnAlignment);
void* RwReallocMemAlignedStub(void* lpMemory, std::size_t lnSize, std::size_t lnAlignment);
void RwFreeMemAlignedStub(void* lpMemory);
