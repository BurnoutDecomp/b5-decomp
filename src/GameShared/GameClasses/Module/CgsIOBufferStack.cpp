#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"

// CgsModule::IOBufferStack - a LIFO bump allocator over a caller-supplied block. Modules push
// their per-frame IO buffers with CreateIOBuffer<T> and pop them (reverse order) with
// DestroyIOBuffer<T>. Recovered from the DecFIGS DWARF (Module/CgsIOBufferStack.cpp).
namespace CgsModule
{
    void IOBufferStack::Construct(const char*)
    {
        mpData = 0;
        muSize = 0;
        muAlignment = 16;
        muAllocated = 0;
        muMaxAllocated = 0;
        muNumAllocated = 0;
    }

    bool IOBufferStack::Prepare(void* lpMemory, u32 luSize, u32 luAlignment)
    {
        mpData = static_cast<u8*>(lpMemory);
        muSize = luSize;
        muAlignment = luAlignment ? luAlignment : 16;
        muAllocated = 0;
        muMaxAllocated = 0;
        muNumAllocated = 0;
        return mpData != 0;
    }

    bool IOBufferStack::Release()
    {
        mpData = 0;
        muSize = 0;
        return true;
    }

    void IOBufferStack::Destruct() {}

    void IOBufferStack::Clear()
    {
        muAllocated = 0;
        muNumAllocated = 0;
    }

    void* IOBufferStack::Alloc(u32 luSize, const char*)
    {
        u32 luAligned = (muAllocated + (muAlignment - 1)) & ~(muAlignment - 1);
        if (!mpData || luAligned + luSize > muSize)
            return 0;
        void* lpResult = mpData + luAligned;
        muAllocated = luAligned + luSize;
        if (muAllocated > muMaxAllocated)
            muMaxAllocated = muAllocated;
        ++muNumAllocated;
        return lpResult;
    }

    bool IOBufferStack::Free(void* lpMemory, u32 luSize)
    {
        // LIFO pop: the active top buffer is freed first. Rewind the bump pointer to the
        // freed block's start (callers free in reverse allocation order).
        if (lpMemory && mpData)
            muAllocated = static_cast<u32>(static_cast<u8*>(lpMemory) - mpData);
        if (muNumAllocated)
            --muNumAllocated;
        return true;
    }
}
