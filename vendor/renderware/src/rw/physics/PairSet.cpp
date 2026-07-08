// =====================================================================================
// rw::physics::PairSet -- definition home for the RenderWare physics pair-set pool.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is authoritative. No
// Feb-2007 reference source and no DecFIGS DWARF exist for this TU.
//
// This TU's 7 ledger functions are all self-contained (they touch only the pair-set block:
// its header pointers, the Link array, and the per-part head array), so all are
// reconstructed faithfully here. The only external callee is ClearAll (its own TU),
// tail-called by Initialize.
//
// A "pair" = two adjacent Link nodes; pair p owns link indices 2*p and 2*p+1. Each link is
// filed into the intrusive doubly-linked bucket list of ONE part (mpHeads[part]); the
// sibling link (index ^ 1) records the OTHER part. Free pairs are threaded through
// link0.miNext from miFreeList.
// =====================================================================================

#include "rw/physics/pairset.h"

namespace rw
{
namespace physics
{

// -------------------------------------------------------------------------------------
// PairSet::GetResourceDescriptor @ 0x82BC7060
//
// STATIC sizer (r3 = output descriptor). The loop fills all five {size=0, alignment=1}
// entries, then entry[0] is overwritten with a big-endian {size, 4} qword (std): +0 = size,
// +4 = alignment.
//
//   size = 4 * (8 * liMaxPairs + liNumParts + 5)     ; (5-word header + 8w/pair + 1w/part)
// -------------------------------------------------------------------------------------
rw::BaseResourceDescriptors<5>* PairSet::GetResourceDescriptor(
    rw::BaseResourceDescriptors<5>* lpResult, int liNumParts, int liMaxPairs)
{
    rw::BaseResourceDescriptor* lpEntries = lpResult->m_baseResourceDescriptors;

    // entry[0..4] = { m_size = 0, m_alignment = 1 }
    for (u32 luEntry = 0u; luEntry < 5u; ++luEntry)
    {
        lpEntries[luEntry].m_size      = 0u;
        lpEntries[luEntry].m_alignment = 1u;
    }

    // size = 4 * (8 * liMaxPairs + liNumParts + 5)
    const u32 luSize =
        4u * static_cast<u32>(8 * liMaxPairs + liNumParts + 5);

    // entry[0] = { m_size = size, m_alignment = 4 }
    lpEntries[0].m_size      = luSize;
    lpEntries[0].m_alignment = 4u;
    return lpResult;
}

// -------------------------------------------------------------------------------------
// PairSet::Initialize @ 0x82BC7038
//
// r3 = the memory slot holding the freshly-allocated block pointer. Lay the header over the
// block: mpLinks starts right after the 5-word header (+0x14); mpHeads starts after the
// link array (2*miMaxPairs links = 32*miMaxPairs bytes). Then tail-call ClearAll to build
// the free list and clear the buckets.
//
// The +0x14 / 32*liMaxPairs offsets are the X360 (4-byte-word) block geometry fixed by
// GetResourceDescriptor -- external serialised-block layout, walked by raw arithmetic.
// -------------------------------------------------------------------------------------
PairSet* PairSet::Initialize(void** lpMemory, int liNumParts, int liMaxPairs)
{
    PairSet* lpSet   = static_cast<PairSet*>(*lpMemory);
    char*    lpBlock = reinterpret_cast<char*>(lpSet);

    lpSet->miMaxPairs = static_cast<u32>(liMaxPairs);
    lpSet->miNumParts = static_cast<u32>(liNumParts);
    lpSet->mpLinks    = reinterpret_cast<Link*>(lpBlock + 0x14);
    lpSet->mpHeads    = reinterpret_cast<s32*>(lpBlock + 32 * static_cast<u32>(liMaxPairs) + 0x14);

    return lpSet->ClearAll();
}

// -------------------------------------------------------------------------------------
// PairSet::InitializeLink @ 0x82BC6E60
//
// File link `liLinkIndex` (payload part `liPart`) at the front of part `liPart`'s bucket.
// -------------------------------------------------------------------------------------
PairSet* PairSet::InitializeLink(int liLinkIndex, int liPart)
{
    Link* lpLink = &mpLinks[liLinkIndex];
    lpLink->miPart = liPart;
    lpLink->miData = 0;
    lpLink->miPrev = -1;

    s32 liHead = mpHeads[liPart];
    lpLink->miNext = liHead;
    if (liHead != -1)
        mpLinks[liHead].miPrev = liLinkIndex;
    mpHeads[liPart] = liLinkIndex;
    return this;
}

// -------------------------------------------------------------------------------------
// PairSet::ReleaseLink @ 0x82BC6EB8
//
// Splice link `liLinkIndex` out of its bucket's doubly-linked list.
// -------------------------------------------------------------------------------------
PairSet* PairSet::ReleaseLink(int liLinkIndex)
{
    Link* lpLink = &mpLinks[liLinkIndex];
    s32   liPrev = lpLink->miPrev;
    s32   liNext = lpLink->miNext;

    if (liPrev == -1)
        mpHeads[lpLink->miPart] = liNext;   // was the bucket head
    else
        mpLinks[liPrev].miNext = liNext;

    if (lpLink->miNext != -1)
        mpLinks[lpLink->miNext].miPrev = lpLink->miPrev;
    return this;
}

// -------------------------------------------------------------------------------------
// PairSet::LinkParts @ 0x82BC6F18
//
// Pop a free pair, file its two links into parts `liPartA` and `liPartB`, tag link0 with
// `liData`, and return the pair index (-1 when the pool is exhausted).
// -------------------------------------------------------------------------------------
int PairSet::LinkParts(int liPartA, int liPartB, int liData)
{
    s32 liPair = miFreeList;
    if (liPair == -1)
        return -1;

    // Pop the free list, threaded through link0.miNext of the free pair.
    miFreeList = mpLinks[2 * liPair].miNext;

    InitializeLink(2 * liPair,     liPartA);
    InitializeLink(2 * liPair + 1, liPartB);
    mpLinks[2 * liPair].miData = liData;
    return liPair;
}

// -------------------------------------------------------------------------------------
// PairSet::UnlinkPair @ 0x82BC6F98
//
// Release both links of pair `liPairIndex` and push it back onto the free list.
// -------------------------------------------------------------------------------------
PairSet* PairSet::UnlinkPair(int liPairIndex)
{
    ReleaseLink(2 * liPairIndex);
    ReleaseLink(2 * liPairIndex + 1);

    mpLinks[2 * liPairIndex].miNext = miFreeList;   // thread onto the free list
    mpLinks[2 * liPairIndex].miPart = -1;
    miFreeList = liPairIndex;
    return this;
}

// -------------------------------------------------------------------------------------
// PairSet::UnlinkParts @ 0x82BC6FF0
//
// Find the pair joining part `liPartA` to part `liPartB` -- walk liPartA's bucket for the
// link whose SIBLING (index ^ 1) is filed under liPartB -- then release that pair.
// -------------------------------------------------------------------------------------
PairSet* PairSet::UnlinkParts(int liPartA, int liPartB)
{
    s32 liLink = mpHeads[liPartA];
    if (liLink != -1)
    {
        do
        {
            if (mpLinks[liLink ^ 1].miPart == liPartB)
                break;
            liLink = mpLinks[liLink].miNext;
        }
        while (liLink != -1);
    }

    // Pair index = link index >> 1 (logical shift, matching the X360 srwi).
    return UnlinkPair(static_cast<int>(static_cast<u32>(liLink) >> 1));
}

} // namespace physics
} // namespace rw
