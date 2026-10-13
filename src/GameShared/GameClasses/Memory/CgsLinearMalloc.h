#pragma once

#include "types.hpp"
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
        void Destruct(); // owned-release/RW-hook behavior remains a separate recovery
        void StartRWAllocation();
        void StopRWAllocation();
        void SetAlignment(size_t lnAlignment);
        size_t GetAlignment() const { return mnAlignment; }   // current bump-pointer alignment
        size_t GetFreeMemory() const;
        size_t GetUsage() const;       // 0x82866D40 - bytes consumed (bump pointer - region start)
        size_t GetSize() const { return mnEndAddress - mnStartAddress; }   // whole adopted-region size (DWARF accessor; header-inline -- the sound Playback::Module::Prepare stream-buffer sizing divides it by 3)
        void* GetStartAddress() const; // borrowed region start, independent of mpAlloc

    private:
        bool   mbCreated;
        void*  mpAlloc; // owned allocation slot; borrowed Create does not set it
        size_t mnStartAddress;
        size_t mnEndAddress;
        size_t mnNextAddress;
        size_t mnAlignment;
        void*  mpLastAlloc;
    };
}
