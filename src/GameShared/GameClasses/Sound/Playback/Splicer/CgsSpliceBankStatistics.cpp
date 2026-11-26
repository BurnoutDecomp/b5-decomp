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
#include <new>   // placement new

namespace CgsSound { namespace Playback {

SpliceBankStatistics* SpliceBankStatistics::spHead = 0;

SpliceBankStatistics::SpliceBankStatistics()
    : mpSpliceContent(0)
    , mpSpliceBank(0)
    , mpaStats(0)
    , muStatCount(0)
    , mpNext(0)
    , muIndex(0)
{
}

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
        // DWARF locals luSize (CgsSplicerContent.h:68) and i (h:71). ARTIST reads the bank's
        // count twice (0x826D5824 / 0x826D5838) and constructs each Stats in place: the
        // placement new's null test is the `cmplwi r10,0; beq` at 0x826D5864.
        muStatCount = lpSpliceBank->mNumSplices;
        size_t luSize = sizeof(Stats) * lpSpliceBank->mNumSplices;
        mpaStats = static_cast<Stats*>(gpSpliceManager->Allocate(luSize, "SpliceBankStatistics"));

        for (u16 i = 0; i < muStatCount; ++i)
        {
            new (&mpaStats[i]) Stats;
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

void SpliceBankStatistics::DoPlay(u16 lu16SpliceIndex)
{
    if (mpaStats && lu16SpliceIndex < muStatCount)
        ++mpaStats[lu16SpliceIndex].muPlayCount;
}

}} // namespace CgsSound::Playback
