#include "SDKs/XGraphics/XGraphicsInternalHashTable.h"

#include <new> // placement new

// ===========================================================================
// XGRAPHICS::InternalHashTable -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// XGraphicsInternalHashTable.h for the attested layout. Each store / branch below
// is store-for-store with the X360 asm.
//
// A bucket is an InternalVector of u32 entries. To reach entry `i` of a bucket the
// X360 compiler inlined the InternalVector element accessor (auto-grow past the
// end): `i >= vec->muCount ? vec->Grow(i) : &vec->mpElements[i]`. In every loop
// below the index is guarded to stay < muCount, so the Grow arm never actually
// fires -- it is reproduced verbatim from the inlined accessor for parity.
// ===========================================================================

namespace XGRAPHICS
{

// Inlined InternalVector element accessor (see header note): pointer to entry
// `auIndex`, growing the vector if the index is past the live count.
static inline u32* ChainSlot(InternalVector* apChain, u32 auIndex)
{
    return auIndex >= apChain->muCount ? apChain->Grow(auIndex)
                                       : &apChain->mpElements[auIndex];
}

u32 InternalHashTable::Lookup(u32 auKey)
{
    InternalVector* lpChain = mppBuckets[(muBucketCount - 1) & mpfnHash(auKey)];
    if (lpChain && lpChain->muCount)
    {
        u32 luIndex = 0;
        while (true)
        {
            u32 luEntry = *ChainSlot(lpChain, luIndex);
            if (mpfnCompare(luEntry, auKey) == 0)
                return luEntry;
            if (++luIndex >= lpChain->muCount)
                break;
        }
    }
    return 0;
}

u32* InternalHashTable::Insert(u32 auKey)
{
    u32 luBucket = (muBucketCount - 1) & mpfnHash(auKey);

    // Lazily allocate this bucket's chain vector, arena-prefixed. On OOM the
    // prefixed allocator yields null and the chain slot is left null (matching the
    // asm, which stores the `Malloc + 4` result even when it comes out null).
    if (mppBuckets[luBucket] == nullptr)
    {
        void* lpMem = ArenaAllocPrefixed(mpArena, sizeof(InternalVector));
        mppBuckets[luBucket] = lpMem ? new (lpMem) InternalVector(mpArena) : nullptr;
    }

    // Push the key at the front of the chain and stamp the slot with it.
    InternalVector* lpChain = mppBuckets[luBucket];
    u32* lpSlot = lpChain->Insert(0);
    *lpSlot = auKey;

    // If this chain now runs deeper than the bucket count, rehash-grow the table;
    // Grow returns the (possibly relocated) slot for the just-inserted key.
    u32* lpResult = lpSlot;
    if (lpChain->muCount > muBucketCount)
        lpResult = Grow();

    ++muCount;
    return lpResult;
}

void InternalHashTable::Remove(u32 auKey)
{
    InternalVector* lpChain = mppBuckets[(muBucketCount - 1) & mpfnHash(auKey)];
    if (lpChain == nullptr || lpChain->muCount == 0)
        return;

    u32 luIndex = 0;
    while (true)
    {
        if (mpfnCompare(*ChainSlot(lpChain, luIndex), auKey) == 0)
        {
            // Match: drop the entry from the chain (inlined InternalVector::Remove
            // -- shift the tail down one slot and shrink the count).
            lpChain->Remove(luIndex);
            return;
        }
        if (++luIndex >= lpChain->muCount)
            return;
    }
}

} // namespace XGRAPHICS
