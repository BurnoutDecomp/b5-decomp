#include "GameShared/GameClasses/Memory/CgsScatterStream.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Core/X360/CgsCoreX360.h"  // CgsCore::MemCpy
#include "types.hpp"                                    // uintptr_t
#define CGS_ARTIST_CGSSCATTERSTREAM_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\memory\\CgsScatterStream.cpp"
// CgsMemory::ScatterStream / ScatterStreamBase - see the header. Faithful port; the
// transfer is XMemCpy -> memcpy. Copies packed -> scattered.
namespace CgsMemory
{
    // @ 0x82867540 - move muBytesToStream bytes from the packed base out to the scattered
    // entry addresses, advancing the (entry, offset) cursor; true when the list is done.
    bool ScatterStreamBase::UpdateStream()
    {
        CGS_ASSERT_AT((mu32Flags & 1u) != 0, "Stream update called when not active", CGS_ARTIST_CGSSCATTERSTREAM_FILE, 72);

        u32 luBudget = muBytesToStream;
        while (luBudget != 0)
        {
            CGS_ASSERT_AT(muCurrentEntry < muListLength, "Gone past end of distribution list length", CGS_ARTIST_CGSSCATTERSTREAM_FILE, 82);
            const DistributionStreamEntry& lrEntry = mpList[muCurrentEntry];
            CGS_ASSERT_AT(muEntryOffset < lrEntry.muLength, "Gone past end of entry data", CGS_ARTIST_CGSSCATTERSTREAM_FILE, 83);

            const u32 luOff       = muEntryOffset;
            const u32 luPackedPos = lrEntry.muPackedOffset + luOff;       // X360 v12
            const u32 luRemaining = lrEntry.muLength - luOff;             // X360 v13 (full)
            u8* const lpDst       = lrEntry.mpScatteredAddress + luOff;   // X360 v15 (memcpy dst)
            u8* const lpSrc       = mpBaseAddress + luPackedPos;          // X360 v14 (memcpy src)
            muStreamPosition      = luPackedPos;                          // X360 a1[7] = v12

            u32 luMoved;
            if (luRemaining <= luBudget)        // the whole rest of this entry fits in the budget
            {
                CgsCore::MemCpy(lpDst, lpSrc, luRemaining);
                luMoved = luRemaining;
                ++muCurrentEntry;
                muEntryOffset = 0;
                if (muCurrentEntry >= muListLength)
                {
                    mu32Flags &= ~1u;           // list complete -> deactivate
                    return true;
                }
            }
            else                                // budget runs out mid-entry
            {
                luMoved = luBudget;
                CgsCore::MemCpy(lpDst, lpSrc, luBudget);
                muEntryOffset += luBudget;
            }
            muStreamPosition += luMoved;         // X360 a1[7] += v13 (loop increment)
            luBudget -= luMoved;                 // X360 i -= v13
        }
        return false;
    }

    // @ 0x82868680 - the X360 leaf reuses the base clear.
    ScatterStream* ScatterStream::Construct()
    {
        DistributionStream::Construct();
        return this;
    }

    // @ 0x828686B0 - validate the (128-byte aligned) list then run the base stream; an
    // inactive stream reports "done".
    bool ScatterStream::Update()
    {
        if ((mu32Flags & 1u) == 0)
            return true;   // not active -> nothing to do, report complete

        CGS_ASSERT(mpList != 0, "No distribution list");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(mpList) & 0x7Fu) == 0,
                   "Distribution list not 128 byte aligned");
        return UpdateStream();
    }
}
