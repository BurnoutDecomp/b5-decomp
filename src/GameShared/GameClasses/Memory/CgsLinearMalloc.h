#pragma once

#include "types.hpp"
#include <cstddef>

// CgsMemory::LinearMalloc - a bump allocator over a caller-supplied region: Create()
// adopts [buffer, buffer+size); allocations advance mnNextAddress. Layout/method set
// from the DecFIGS DWARF (CgsLinearMalloc.h).
namespace CgsMemory
{
    class LinearMalloc
    {
    public:
        static const s32 KI_DEFAULT_ALIGNMENT = 4;

        void Construct();
        void Create(void* lpAlloc, size_t lnSize);
        void* Malloc(size_t lnSize);   // 0x82866DC0 - align the bump pointer, advance, return it (null if it overflows the region)
        void FreeAll();                // 0x82866E60 - reset the bump pointer to the start (frees every allocation)
        void Destruct();
        void StartRWAllocation();
        void StopRWAllocation();
        void SetAlignment(size_t lnAlignment);
        size_t GetFreeMemory() const;
        size_t GetUsage() const;       // 0x82866D40 - bytes consumed (bump pointer - region start)
        void* GetStartAddress() const;

    private:
        bool   mbCreated;
        void*  mpAlloc;
        size_t mnStartAddress;
        size_t mnEndAddress;
        size_t mnNextAddress;
        size_t mnAlignment;
        void*  mpLastAlloc;
    };
}
