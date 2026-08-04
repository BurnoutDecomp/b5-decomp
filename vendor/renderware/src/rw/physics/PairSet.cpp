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

    // CONSOLE: size = 4 * (8 * liMaxPairs + liNumParts + 5)
    //
    // ⚠️⚠️ FIXED 2026-08-04 (task #135) -- ANOTHER LIVE CONSOLE-STRIDE BUG, and this one had
    // teeth. The trailing `+ 5` is the FIVE-WORD (20-byte) PairSet header, and Initialize below
    // used to lay the link array down at the matching literal `block + 0x14`. On x64
    // sizeof(PairSet) is 32 (two 8-byte pointers), so the link array would have started TWELVE
    // BYTES INSIDE the header and the very first ClearAll would have overwritten miMaxPairs /
    // miNumParts / miFreeList with link data. It was invisible only because nothing in the tree
    // had ever called PairSet::Initialize; PhysicsSimulationModule::AllocateMemoryAndInitialiseRW
    // calls it three times.
    // The `8 * liMaxPairs` words per pair (two 16-byte Links) and the one word per part are
    // all-s32 records and do not widen; they are written through sizeof anyway so a future
    // change to Link cannot silently desynchronise the sizer from the carver.
    const u32 luSize =
        static_cast<u32>(sizeof(PairSet))
        + 2u * static_cast<u32>(sizeof(Link)) * static_cast<u32>(liMaxPairs)
        + static_cast<u32>(sizeof(s32)) * static_cast<u32>(liNumParts);

    // entry[0] = { m_size = size, m_alignment = 4 }
    // ⚠️ The console's 4 is the alignment of its widest member (an s32/4-byte pointer). Two of
    // this object's members are 8 bytes on x64, so the block alignment moves with the type
    // rather than staying on the console literal -- a 4-aligned block would seat both pointers
    // across an 8-byte boundary.
    lpEntries[0].m_size      = luSize;
    lpEntries[0].m_alignment = static_cast<u32>(alignof(PairSet));
    return lpResult;
}

// -------------------------------------------------------------------------------------
// PairSet::ClearAll @ 0x82BC6DC0   (40 instructions)
//
// ⚠️ EXPORT-SET HOLE -- absent from .ida-exports (0x82BC6E60, InitializeLink, is the next
// exported symbol). Recovered with headless IDA 9.3 against a COPY of
// BURNOUT_X360_ARTIST.XEX.i64, the same way PhysicsSimulationModule::Destruct and
// RigidBodyData::RigidBodyData were. This is the fourth confirmed hole in that export set.
//
// Two independent loops:
//   * the pair pool -- walk link0 of every pair (`v2 += 32`, i.e. two 16-byte Links per step),
//     stamping miPart = -1 (the "unfiled" marker) and threading miNext to the NEXT pair index.
//     The last pair's miNext is then overwritten with -1 to terminate the chain
//     (`32*miMaxPairs + mpLinks - 24` == the last pair's link0.miNext, since 32 - 8 == 24).
//     miFreeList becomes 0, or -1 when the pool has no pairs at all.
//   * the bucket heads -- mpHeads[i] = -1 for every part.
//
// ⚠️ THE FREE LIST IS THREADED IN PAIR UNITS, NOT LINK UNITS. `v3[2] = v1` writes the
// POST-incremented counter, so pair p points at p+1 -- an index into the pair pool, which is
// what miFreeList and LinkParts consume. Reading it as a link index would halve the pool.
// -------------------------------------------------------------------------------------
PairSet* PairSet::ClearAll()
{
    s32* lpFreeThreadTail = 0;

    if (miMaxPairs != 0u)
    {
        miFreeList = 0;                                     // stw r11(=0), 0x10(r3)

        u32 luPair = 0u;
        do
        {
            Link* lpLink0 = &mpLinks[2u * luPair];
            ++luPair;
            lpLink0->miPart = -1;                           // stw r11(=-1), 0(r8)
            lpLink0->miNext = static_cast<s32>(luPair);     // stw r10, 8(r8)  -- POST-increment
            lpFreeThreadTail = &lpLink0->miNext;
        }
        while (luPair < miMaxPairs);

        // `stw r11(=-1), -0x18(r10)` off the end of the link array -- the last pair's miNext.
        *lpFreeThreadTail = -1;
    }
    else
    {
        miFreeList = -1;                                    // the pool is empty
    }

    for (u32 luPart = 0u; luPart < miNumParts; ++luPart)
    {
        mpHeads[luPart] = -1;                               // stwx r11(=-1), r8, r10
    }

    return this;
}

// -------------------------------------------------------------------------------------
// PairSet::Initialize @ 0x82BC7038
//
// r3 = the memory slot holding the freshly-allocated block pointer. Lay the header over the
// block: mpLinks starts right after the 5-word header (+0x14); mpHeads starts after the
// link array (2*miMaxPairs links = 32*miMaxPairs bytes). Then tail-call ClearAll to build
// the free list and clear the buckets.
//
// ⚠️⚠️ THE CONSOLE LITERALS WERE +0x14 (the 5-word header) AND 32*liMaxPairs. The first one
// is a HOST BUG -- see the correction in GetResourceDescriptor above: sizeof(PairSet) is 32 on
// x64, so `block + 0x14` seated the link array twelve bytes inside the header. Both offsets now
// come from sizeof and stay locked to the sizer.
// -------------------------------------------------------------------------------------
PairSet* PairSet::Initialize(void** lpMemory, int liNumParts, int liMaxPairs)
{
    PairSet* lpSet   = static_cast<PairSet*>(*lpMemory);
    char*    lpBlock = reinterpret_cast<char*>(lpSet);

    lpSet->miMaxPairs = static_cast<u32>(liMaxPairs);
    lpSet->miNumParts = static_cast<u32>(liNumParts);
    lpSet->mpLinks    = reinterpret_cast<Link*>(lpBlock + sizeof(PairSet));
    lpSet->mpHeads    = reinterpret_cast<s32*>(
        lpBlock + sizeof(PairSet) + 2 * sizeof(Link) * static_cast<u32>(liMaxPairs));

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
