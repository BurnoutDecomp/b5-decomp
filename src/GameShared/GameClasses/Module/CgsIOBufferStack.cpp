#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint (overflow diagnostic)
#include <stdlib.h>                                          // getenv (BRN_IOBUF_ZERO, host only)

// CgsModule::IOBufferStack - a LIFO bump allocator over a caller-supplied block. Modules push
// their per-frame IO buffers with CreateIOBuffer<T> and pop them (reverse order) with
// DestroyIOBuffer<T>. Recovered from the DecFIGS DWARF (Module/CgsIOBufferStack.cpp).
namespace CgsModule
{
    // FLAG PC (host-only diagnostic, no console counterpart). BRN_IOBUF_ZERO=1 restores the old
    // PC-only zero-fill inside CreateIOBuffer<T> so a suspected "buffer relied on being zeroed"
    // regression can be A/B'd in a single boot. The environment is read exactly ONCE (first
    // create); every later create just reads the cached bool -- the whole point of the change is
    // that the per-create path does no work proportional to sizeof(T).
    bool IOBufferHostZeroFillEnabled()
    {
        static const bool sbEnabled = []() -> bool
        {
            const char* lpcValue = getenv("BRN_IOBUF_ZERO");
            return lpcValue != 0 && lpcValue[0] == '1';
        }();
        return sbEnabled;
    }

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

    void* IOBufferStack::Alloc(u32 luSize, const char* lpcDebugName)
    {
        u32 luAligned = (muAllocated + (muAlignment - 1)) & ~(muAlignment - 1);
        if (!mpData || luAligned + luSize > muSize)
        {
            // A silent null here surfaces much later as "mxStatusFlags.IsBitSet(
            // eStatusConstructed)" inside the first Lock on the buffer that never got
            // allocated, so name the overflow at the point it happens. One-shot.
            static bool s_bLoggedOverflow = false;
            if (!s_bLoggedOverflow)
            {
                s_bLoggedOverflow = true;
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint << "[IOBufferStack] OVERFLOW allocating '"
                        << (lpcDebugName ? lpcDebugName : "?") << "' size " << static_cast<s32>(luSize)
                        << " (used " << static_cast<s32>(muAllocated)
                        << " of " << static_cast<s32>(muSize) << ")\n";
            }
            return 0;
        }
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
