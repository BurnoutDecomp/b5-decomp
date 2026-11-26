// XDK size optimization: ARTIST compiled RenderWare's rwphysics for size (record-form cr0
// tests, as in the rwcore filesys sources);
// PairSet::ClearAll 0x82BC6DC0 tests its anchor with `cmplwi r11,0` and matches only so.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
// =====================================================================================
// rw::physics::PairSet -- definition home for the RenderWare physics pair-set pool.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC). DecFIGS pairset.h
// supplies the original link and index types.
//
// The pool owns a link array and a per-part bucket table. Initialize lays out
// both arrays and tail-calls ClearAll to reset them.
//
// A "pair" = two adjacent Link nodes; pair p owns link indices 2*p and 2*p+1. Each link is
// filed into the intrusive doubly-linked bucket list of ONE part (mpHeads[part]); the
// sibling link (index ^ 1) records the OTHER part. Free pairs are threaded through
// link0.next from muFreeList.
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
        + static_cast<u32>(sizeof(u32)) * static_cast<u32>(liNumParts);

    // entry[0] = { m_size = size, m_alignment = 4 }
    // ⚠️ The console's 4 is the alignment of its widest member (an s32/4-byte pointer). Two of
    // this object's members are 8 bytes on x64, so the block alignment moves with the type
    // rather than staying on the console literal -- a 4-aligned block would seat both pointers
    // across an 8-byte boundary.
    lpEntries[0].m_size      = luSize;
    lpEntries[0].m_alignment = static_cast<u32>(__alignof(PairSet));
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
//     stamping the invalid part index and threading each next free pair.
//     The final next index marks the end of the free list.
//   * the bucket heads -- mpHeads[i] = -1 for every part.
//
// ⚠️ THE FREE LIST IS THREADED IN PAIR UNITS, NOT LINK UNITS. `v3[2] = v1` writes the
// POST-incremented counter, so pair p points at p+1 in the pair pool.
// -------------------------------------------------------------------------------------
void PairSet::ClearAll()
{
    if (miMaxPairs != 0u)
    {
        muFreeList = 0u;
        for (u32 luPair = 0u; luPair < miMaxPairs; ++luPair)
        {
            Link& lrFirst = mpLinks[2u * luPair];
            lrFirst.partIndex = KU_INVALID_INDEX;
            lrFirst.next = luPair + 1u;
        }
        mpLinks[2u * (miMaxPairs - 1u)].next = KU_INVALID_INDEX;
    }
    else
    {
        muFreeList = KU_INVALID_INDEX;
    }

    for (u32 luPart = 0u; luPart < miNumParts; ++luPart)
        mpHeads[luPart] = KU_INVALID_INDEX;
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
    lpSet->mpHeads    = reinterpret_cast<u32*>(
        lpBlock + sizeof(PairSet) + 2 * sizeof(Link) * static_cast<u32>(liMaxPairs));

    lpSet->ClearAll();
    return lpSet;
}

// -------------------------------------------------------------------------------------
// PairSet::InitializeLink @ 0x82BC6E60
//
// File the link at the front of the part's bucket.
// -------------------------------------------------------------------------------------
void PairSet::InitializeLink(u32 luLinkIndex, u32 luPart)
{
    Link* lpLink = &mpLinks[luLinkIndex];
    lpLink->partIndex = luPart;
    lpLink->flags = 0u;
    lpLink->prev = KU_INVALID_INDEX;

    const u32 luHead = mpHeads[luPart];
    lpLink->next = luHead;
    if (luHead != KU_INVALID_INDEX)
        mpLinks[luHead].prev = luLinkIndex;
    mpHeads[luPart] = luLinkIndex;
}

// -------------------------------------------------------------------------------------
// PairSet::ReleaseLink @ 0x82BC6EB8
//
// Splice the link out of its bucket's doubly-linked list.
// -------------------------------------------------------------------------------------
void PairSet::ReleaseLink(u32 luLinkIndex)
{
    Link* lpLink = &mpLinks[luLinkIndex];
    const u32 luPrev = lpLink->prev;
    const u32 luNext = lpLink->next;

    if (luPrev == KU_INVALID_INDEX)
        mpHeads[lpLink->partIndex] = luNext;
    else
        mpLinks[luPrev].next = luNext;

    if (lpLink->next != KU_INVALID_INDEX)
        mpLinks[lpLink->next].prev = lpLink->prev;
}

// -------------------------------------------------------------------------------------
// PairSet::LinkParts @ 0x82BC6F18
//
// Pop a free pair, file its two links into their part buckets and store the pair flags.
// -------------------------------------------------------------------------------------
u32 PairSet::LinkParts(u32 luPartA, u32 luPartB, u32 luFlags)
{
    const u32 luPair = muFreeList;
    if (luPair == KU_INVALID_INDEX)
        return KU_INVALID_INDEX;

    const u32 luFirstLink = 2u * luPair;
    muFreeList = mpLinks[luFirstLink].next;

    InitializeLink(luFirstLink, luPartA);
    InitializeLink(luFirstLink + 1u, luPartB);
    mpLinks[luFirstLink].flags = luFlags;
    return luPair;
}

// -------------------------------------------------------------------------------------
// PairSet::UnlinkPair @ 0x82BC6F98
//
// Release both links and push the pair back onto the free list.
// -------------------------------------------------------------------------------------
void PairSet::UnlinkPair(u32 luPairIndex)
{
    const u32 luFirstLink = 2u * luPairIndex;
    ReleaseLink(luFirstLink);
    ReleaseLink(luFirstLink + 1u);

    mpLinks[luFirstLink].next = muFreeList;
    mpLinks[luFirstLink].partIndex = KU_INVALID_INDEX;
    muFreeList = luPairIndex;
}

// -------------------------------------------------------------------------------------
// PairSet::UnlinkParts @ 0x82BC6FF0
//
// Find the link in the first part's bucket whose sibling is filed under the second part.
// -------------------------------------------------------------------------------------
void PairSet::UnlinkParts(u32 luPartA, u32 luPartB)
{
    u32 luLink = mpHeads[luPartA];
    if (luLink != KU_INVALID_INDEX)
    {
        do
        {
            if (mpLinks[luLink ^ 1u].partIndex == luPartB)
                break;
            luLink = mpLinks[luLink].next;
        }
        while (luLink != KU_INVALID_INDEX);
    }

    UnlinkPair(luLink >> 1);
}

} // namespace physics
} // namespace rw
