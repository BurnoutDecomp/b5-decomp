#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint (overflow diagnostic)

// CgsModule::IOBufferStack - a LIFO bump allocator over a caller-supplied block. Modules push
// their per-frame IO buffers with CreateIOBuffer<T> and pop them (reverse order) with
// DestroyIOBuffer<T>. Recovered from the DecFIGS DWARF (Module/CgsIOBufferStack.cpp).
namespace CgsModule
{
    namespace
    {
        const char KAC_ARTIST_IO_BUFFER_STACK_FILE[] = "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsIOBufferStack.cpp";
    }

    void IOBufferStack::Construct(const char*)
    {
        mpData = 0;
        muSize = 0;
        muAlignment = 0;
        muAllocated = 0;
        muMaxAllocated = 0;
    }

    bool IOBufferStack::Prepare(void* lpMemory, u32 luSize, u32 luAlignment)
    {
        // ARTIST 0x8286E120 validates the requested alignment and the buffer address.

        CGS_ASSERT_AT(luAlignment >= 16, "luAlignment >= 16", KAC_ARTIST_IO_BUFFER_STACK_FILE, 83);
        CGS_ASSERT_AT((reinterpret_cast<intptr_t>(lpMemory) & 0xf) == 0,
            "0 == (reinterpret_cast<intptr_t>(lpData) & 0xf )", KAC_ARTIST_IO_BUFFER_STACK_FILE, 84);

        mpData = static_cast<u8*>(lpMemory);
        muSize = luSize;
        muAlignment = luAlignment;
        muAllocated = 0;
        muMaxAllocated = 0;
        muNumAllocated = 0;
        return true;
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
        const u32 luAlignedSize = (luSize + muAlignment - 1) & ~(muAlignment - 1);
        const u32 luNewAllocated = muAllocated + luAlignedSize;
        if (luNewAllocated > muSize)
        {
            CGS_FAIL_STREAM_AT("Error allocating " << luAlignedSize << " bytes\n",
                                 KAC_ARTIST_IO_BUFFER_STACK_FILE, 171);
            return NULL;
        }

        void* lpReturnVal = mpData + muAllocated;
        muAllocated = luNewAllocated;
        ++muNumAllocated;
        if (muAllocated >= muMaxAllocated)
        {
            muMaxAllocated = muAllocated;
            static bool _lbPrintOut = false;
            if (_lbPrintOut)
            {
                DebugPrintStack();
            }
        }
        return lpReturnVal;
    }

    void IOBufferStack::DebugPrintStack()
    {
        const char* const KAC_RULE = "-------------------------------------\n";

        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << KAC_RULE;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "----STACK INFO\n";
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Allocated:          " << muAllocated << "\n";
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Max Allocated:      " << muMaxAllocated << "\n";
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Total Allocations:  " << muNumAllocated << "\n";
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << KAC_RULE;
    }

    bool IOBufferStack::Free(void* lpData, u32 luSize)
    {
        luSize = (luSize + muAlignment - 1) & ~(muAlignment - 1);
        const uintptr_t luDataOffset = reinterpret_cast<uintptr_t>(lpData) - reinterpret_cast<uintptr_t>(mpData);
        if (luDataOffset + luSize != muAllocated)
        {
            DebugPrintStack();
            CGS_FAIL_STREAM_AT("Attempt to free data that is not on or at top of stack\n",
                                 KAC_ARTIST_IO_BUFFER_STACK_FILE, 255);
            return false;
        }

        muAllocated -= luSize;
        --muNumAllocated;
        return true;
    }
}
