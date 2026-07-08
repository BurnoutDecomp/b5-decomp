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

SpliceBankStatistics::SpliceBankStatistics(const SpliceManager::SpliceContainer* lpSpliceBank,
                                           const SplicerContent* lpSpliceContent, u32 luIndex)
{
    mpSpliceContent = lpSpliceContent;  // +0x00  (a3 / r5)
    mpSpliceBank    = lpSpliceBank;     // +0x04  (a2 / r4)
    mpaStats        = 0;                // +0x08
    muStatCount     = 0;                // +0x0C
    muIndex         = luIndex;          // +0x14  (a4 / r6)

    if (lpSpliceBank)
    {
        const u32 luCount = lpSpliceBank->mNumSplices; // *(a2 + 8)
        muStatCount = luCount;

        mpaStats = static_cast<Stats*>(
            gpSpliceManager->Allocate(sizeof(Stats) * luCount, "SpliceBankStatistics"));

        if (muStatCount > 0)
        {
            for (u16 lu = 0; lu < muStatCount; ++lu)
            {
                Stats* lpSlot = mpaStats + lu;
                if (lpSlot)          // asm's redundant `cmplwi r10,0; beq` preserved
                    lpSlot->muPlayCount = 0;
            }
        }

        // Prepend onto the intrusive statistics list.
        mpNext = spHead;   // +0x10
        spHead = this;
    }
}

SpliceBankStatistics::~SpliceBankStatistics()
{
    if (mpaStats)
    {
        // The X360 inlines SpliceManager::Free here (build a zeroed free descriptor and
        // forward the block through the manager's heap allocator, mgr->mEnvironment's
        // +0x30 block allocator). De-inlined to the owning call: free through the global
        // SpliceManager (DWARF-attested SpliceManager::Free, own TU).
        gpSpliceManager->Free(mpaStats);
        mpaStats = 0;   // +0x08
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

    mpSpliceBank = 0; // +0x04
    muStatCount  = 0; // +0x0C
}

}} // namespace CgsSound::Playback
