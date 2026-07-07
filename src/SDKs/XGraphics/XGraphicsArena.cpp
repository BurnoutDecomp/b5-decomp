#include "SDKs/XGraphics/XGraphicsArena.h"

#include <cstring> // memset

// ===========================================================================
// XGRAPHICS::Arena -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// XGraphicsArena.h for the attested layout. Each store below is store-for-store
// with the X360 asm; the four stats counters and the LIFO-free path are gated
// on mbTrackStats (the +0x28 byte flag) exactly as the asm branches on it.
// ===========================================================================

namespace XGRAPHICS
{

// Smallest payload a fresh block is grown with (0x2FD8). Grow bumps this by the
// block header, so a new block is at least KI_MIN_BLOCK_PAYLOAD + sizeof(header).
static const int KI_MIN_BLOCK_PAYLOAD = 12248;

void* Arena::Grow(int aiSize)
{
    if (aiSize < KI_MIN_BLOCK_PAYLOAD)
        aiSize = KI_MIN_BLOCK_PAYLOAD;

    // Total block = requested payload + the header the arena writes at its head.
    // (On X360 the header is the literal `8` the asm adds; sizeof keeps the PC
    // x64 build correct once the chain pointer widens.)
    const u32 luBlockSize = static_cast<u32>(aiSize + static_cast<int>(sizeof(ArenaBlock)));
    ArenaBlock* lpPrevBlock = mpBlock;

    ArenaBlock* lpBlock = static_cast<ArenaBlock*>(
        mpHeap->mpfnAllocBlock(mpHeap->mpAllocContext, static_cast<int>(luBlockSize)));
    mpBlock = lpBlock;

    // Write the block header: chain the old block, record the block size.
    lpBlock->mpPrevBlock = lpPrevBlock;
    mpBlock->muBlockSize = luBlockSize;

    // Bump pointer starts just past the header; end sits at the block's tail.
    u8* lpBase = reinterpret_cast<u8*>(mpBlock);
    mpCurrent   = lpBase + sizeof(ArenaBlock);
    mpEnd       = lpBase + luBlockSize;
    mpLastAlloc = lpBase + sizeof(ArenaBlock);
    return lpBlock;
}

void* Arena::Malloc(int aiSize)
{
    // Round the request up to the next 4-byte boundary.
    const u32 luSize = static_cast<u32>(aiSize + 3) & 0xFFFFFFFCu;

    // Grow a new block if this allocation would run past the current one.
    if (mpCurrent + luSize > mpEnd)
        Grow(static_cast<int>(luSize));

    u8* lpAlloc = mpCurrent;
    mpLastAlloc = lpAlloc;

    // Stats: if this allocation lands below the high-water mark it is reusing
    // space a prior LIFO Free() gave back.
    if (mbTrackStats)
    {
        if (lpAlloc < mpHighWater)
        {
            ++muReuseCount;
            const u32 luReused = (lpAlloc + luSize >= mpHighWater)
                                     ? static_cast<u32>(mpHighWater - lpAlloc)
                                     : luSize;
            muReuseBytes += luReused;
        }
    }

    u8* lpNext = lpAlloc + luSize;
    mpCurrent = lpNext;
    if (mpHighWater < lpNext)
        mpHighWater = lpNext;

    return mpLastAlloc;
}

void* Arena::Free(void* apPtr)
{
    // LIFO free: only the most recent Malloc can be rolled back, and only while
    // stats tracking is on. Anything else is a no-op that returns the pointer.
    if (mbTrackStats)
    {
        if (apPtr == mpLastAlloc)
        {
            u8* lpLast = mpLastAlloc;
            const u32 luFreedSize = static_cast<u32>(mpCurrent - lpLast);
            memset(apPtr, 0, luFreedSize);
            mpCurrent = lpLast; // rewind the bump pointer to the freed alloc
            ++muFreeCount;
            muFreeBytes += luFreedSize;
        }
    }
    return apPtr;
}

} // namespace XGRAPHICS
