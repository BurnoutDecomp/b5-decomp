// ============================================================================
// CgsSpliceBankStatistics.cpp -- CgsSound::Playback::SpliceBankStatistics.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   SpliceBankStatistics::SpliceBankStatistics @ 0x826D57E8
//   SpliceBankStatistics::~SpliceBankStatistics @ 0x826A1158
//
// The ctor register/param map is (this=r3, source=r4, tag=r5, handle=r6): a2 is the
// source descriptor (the asm reads *(a2+8) as the entry count), so the source is the
// FIRST C++ argument after `this`, not the tag. With a source it allocates a per-entry
// u16 bank through the global SpliceManager, zero-fills it, and prepends the record
// onto the intrusive spHead list. The dtor frees the bank through the SpliceManager
// heap's embedded allocator and unlinks.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSpliceBankStatistics.h"
#include "GameShared/GameClasses/Sound/Playback/Splicer/SpliceManager.h"

namespace CgsSound { namespace Playback {

SpliceBankStatistics* SpliceBankStatistics::spHead = 0;

SpliceBankStatistics::SpliceBankStatistics(SourceDescriptor* lprSource, int liTag, int liHandle)
{
    miTag    = liTag;        // +0x00  (a3 / r5)
    mpSource = lprSource;    // +0x04  (a2 / r4)
    mpBuffer = 0;            // +0x08
    muCount  = 0;            // +0x0C
    mHandle  = liHandle;     // +0x14  (a4 / r6)

    if (lprSource)
    {
        const u32 luCount = lprSource->muEntryCount; // *(a2 + 8)
        muCount = luCount;

        mpBuffer = static_cast<u16*>(
            gpSpliceManager->Allocate(2 * luCount, "SpliceBankStatistics"));

        if (muCount > 0)
        {
            for (u16 lu = 0; lu < muCount; ++lu)
            {
                u16* lpSlot = mpBuffer + lu;
                if (lpSlot)          // asm's redundant `cmplwi r10,0; beq` preserved
                    *lpSlot = 0;
            }
        }

        // Prepend onto the intrusive statistics list.
        mpNext = spHead;   // +0x10
        spHead = this;
    }
}

namespace
{
    // The embedded block allocator reached via mpHeap+0x30. Only its Free entry
    // (vtable slot index 5 / byte 0x14) is exercised here. This is an external
    // heap-internal object (raw-offset walk documented) modelled locally so the free
    // forwards faithfully without mutating the committed SpliceManager.h.
    struct SpliceEmbeddedAllocator
    {
        struct VTable
        {
            void* mapReserved[5];                                  // slots 0..4
            void  (*mpfnFree)( SpliceEmbeddedAllocator*, void* );   // slot 5 / +0x14
        };
        const VTable* mpVTable; // +0x00
    };

    // 24-byte free descriptor: block pointer in word 0, remaining five words zero.
    struct SpliceFreeRequest
    {
        void* mpBlock;        // [0]
        s32   maiZero[5];     // [1..5]
    };
}

SpliceBankStatistics::~SpliceBankStatistics()
{
    if (mpBuffer)
    {
        SpliceFreeRequest lRequest;
        for (int li = 0; li < 5; ++li)
            lRequest.maiZero[li] = 0;
        lRequest.mpBlock = mpBuffer;

        // mgr -> mpHeap(+0x6C4) -> embedded allocator(+0x30) -> vtable slot 0x14.
        // Raw-offset walk into external heap internals (documented external data).
        u8* lpHeap  = *reinterpret_cast<u8**>(reinterpret_cast<u8*>(gpSpliceManager) + 0x6C4);
        SpliceEmbeddedAllocator* lpAllocator =
            *reinterpret_cast<SpliceEmbeddedAllocator**>(lpHeap + 0x30);
        lpAllocator->mpVTable->mpfnFree(lpAllocator, &lRequest);

        mpBuffer = 0;   // +0x08
    }

    // Unlink from the intrusive statistics list headed by spHead.
    SpliceBankStatistics* lpCursor = spHead;
    if (this == spHead)
    {
        spHead = mpNext;
    }
    else if (spHead)
    {
        for (;;)
        {
            SpliceBankStatistics* lpNext = lpCursor->mpNext;
            if (lpNext == this)
            {
                lpCursor->mpNext = lpCursor->mpNext->mpNext;
                break;
            }
            lpCursor = lpCursor->mpNext;
            if (!lpNext)
                break;
        }
    }

    mpSource = 0; // +0x04
    muCount  = 0; // +0x0C
}

}} // namespace CgsSound::Playback
