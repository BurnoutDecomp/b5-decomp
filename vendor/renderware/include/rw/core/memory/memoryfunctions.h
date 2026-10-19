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
