#pragma once

#include "types.hpp"
#include "rw/core/memory/memoryfunctions.h"
#include <cstddef>

// CgsMemory::LinearMalloc - a bump allocator over a caller-supplied region: Create()
// borrows [buffer, buffer+size); allocations advance mnNextAddress. Layout/method set
// from the DecFIGS DWARF (CgsLinearMalloc.h).
namespace CgsMemory
{
    class LinearMalloc
    {
    public:
        static const s32 KI_DEFAULT_ALIGNMENT = 4;

        void Construct(); // ARTIST 0x828690E0; leaves mpLastAlloc unchanged
        void Create(void* lpBuffer, size_t lnSize); // ARTIST 0x82869108; borrowed backing
        void* Malloc(size_t lnSize);   // 0x82866DC0 - align the bump pointer, advance, return it (null if it overflows the region)
        void FreeAll();                // 0x82866E60 - reset the bump pointer to the start (frees every allocation)
        void Destruct(); // ARTIST 0x828691C8; stop RW association, then release owned backing
        void StartRWAllocation();
        void StopRWAllocation(); // ARTIST 0x82866C38
        void SetAlignment(size_t lnAlignment);
        size_t GetAlignment() const { return mnAlignment; }   // current bump-pointer alignment
        size_t GetFreeMemory() const;
        size_t GetUsage() const;       // 0x82866D40 - bytes consumed (bump pointer - region start)
        size_t GetSize() const;       // ARTIST 0x82866D50; asserts the allocator is created
        void* GetStartAddress() const; // borrowed region start, independent of mpAlloc

    private:
        static RwMemoryFunctions mCachedMemoryFuncs;
        static LinearMalloc* mpActiveRWAllocator;

        friend void _LinearMalloc_AssertX360Layout();
        bool   mbCreated;
        size_t mnStartAddress;
        size_t mnEndAddress;
        size_t mnNextAddress;
        size_t mnAlignment;
        void*  mpLastAlloc;
        void*  mpAlloc; // owned allocation slot; borrowed Create does not set it
    };

#if defined(_XBOX)
    inline void _LinearMalloc_AssertX360Layout()
    {
        CGS_STATIC_ASSERT((sizeof(LinearMalloc) == 0x1c), "LinearMalloc size");
        CGS_STATIC_ASSERT((offsetof(LinearMalloc, mbCreated) == 0x00), "mbCreated offset");
        CGS_STATIC_ASSERT((offsetof(LinearMalloc, mnStartAddress) == 0x04), "mnStartAddress offset");
        CGS_STATIC_ASSERT((offsetof(LinearMalloc, mnEndAddress) == 0x08), "mnEndAddress offset");
        CGS_STATIC_ASSERT((offsetof(LinearMalloc, mnNextAddress) == 0x0c), "mnNextAddress offset");
        CGS_STATIC_ASSERT((offsetof(LinearMalloc, mnAlignment) == 0x10), "mnAlignment offset");
        CGS_STATIC_ASSERT((offsetof(LinearMalloc, mpLastAlloc) == 0x14), "mpLastAlloc offset");
        CGS_STATIC_ASSERT((offsetof(LinearMalloc, mpAlloc) == 0x18), "mpAlloc offset");
    }
#endif
}
