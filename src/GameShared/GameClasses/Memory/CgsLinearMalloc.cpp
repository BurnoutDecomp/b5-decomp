#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"

// CgsMemory::LinearMalloc - bump allocator over a caller-supplied region. Recovered
// behaviour: Construct() resets the allocator to empty; Create() adopts [buffer,
// buffer+size) and allocations advance mnNextAddress.

namespace CgsMemory
{
    void LinearMalloc::Construct()
    {
        mbCreated = false;
        mpAlloc = nullptr;
        mnStartAddress = 0;
        mnEndAddress = 0;
        mnNextAddress = 0;
        mnAlignment = KI_DEFAULT_ALIGNMENT;
        mpLastAlloc = nullptr;
    }

    void LinearMalloc::Create(void* lpAlloc, size_t lnSize)
    {
        mpAlloc = lpAlloc;
        mnStartAddress = reinterpret_cast<size_t>(lpAlloc);
        mnEndAddress = mnStartAddress + lnSize;
        mnNextAddress = mnStartAddress;
        mnAlignment = KI_DEFAULT_ALIGNMENT;
        mpLastAlloc = nullptr;
        mbCreated = true;
    }

    void LinearMalloc::Destruct()
    {
        mbCreated = false;
        mpAlloc = nullptr;
        mnStartAddress = 0;
        mnEndAddress = 0;
        mnNextAddress = 0;
        mpLastAlloc = nullptr;
    }

    void LinearMalloc::SetAlignment(size_t lnAlignment)
    {
        mnAlignment = lnAlignment;
    }

    size_t LinearMalloc::GetFreeMemory() const
    {
        return mnEndAddress - mnNextAddress;
    }

    void* LinearMalloc::GetStartAddress() const
    {
        return mpAlloc;
    }

    void LinearMalloc::StartRWAllocation()
    {
    }

    void LinearMalloc::StopRWAllocation()
    {
    }
}
