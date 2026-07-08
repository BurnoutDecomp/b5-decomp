#pragma once

// =====================================================================================
// rw::physics::PairSet -- the EATech RenderWare physics "pair set": a fixed-capacity pool
// of undirected pairs (contacts / joints / drives) between physics parts, indexed so that
// every part can be walked for the pairs it participates in.
//
// EATech RenderWare physics. Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm
// is authoritative. No Feb-2007 reference source and no DecFIGS DWARF exist for this TU.
// Shares the rw::physics vocabulary with the sibling Simulation / SimulationWorkspace TUs.
//
// LAYOUT (recovered from the union of this TU's offset accesses; the X360 asm is
// authoritative for placement). The whole object is a single RW resource block, sized by
// GetResourceDescriptor and laid out by Initialize as:
//
//     [ 5-word header | Link mpLinks[2 * miMaxPairs] | s32 mpHeads[miNumParts] ]
//
// mpLinks / mpHeads are stored back-pointers INTO this same block; the +0x14 header stride
// and the trailing arrays are X360 (4-byte-word) block geometry fixed by the resource
// sizer, so they are treated as external serialised-block layout (semantic parity, not
// byte matching -- the PC/x64 pointer widths differ but nothing here relies on the offset).
//
// A "pair" is two Link nodes (link index = 2*pairIndex and 2*pairIndex+1). Each link is
// filed into the doubly-linked bucket list of one part (mpHeads[part]); its sibling link
// records the OTHER part of the pair. Free pairs are threaded on link0.miNext through
// miFreeList.
// =====================================================================================

#include "types.hpp"                 // s32 / u32
#include "rw/rwcore_structs.h"       // rw::BaseResourceDescriptors<5>

namespace rw
{
namespace physics
{

class PairSet
{
public:
    // One half of a pair: a node in a part's intrusive doubly-linked bucket list. 16 bytes.
    struct Link
    {
        s32 miPart;   // +0x00  the part id this link is filed under (mpHeads bucket)
        s32 miData;   // +0x04  pair payload (contact/joint/drive id); carried on link0 only
        s32 miNext;   // +0x08  next link index in the bucket (-1 = end); free-list thread on link0
        s32 miPrev;   // +0x0C  prev link index in the bucket (-1 = list head)
    };

    // -------------------------------------------------------------------------------------
    // GetResourceDescriptor @ 0x82BC7060 -- build-time sizer for the pair-set block. STATIC
    // (r3 is the output descriptor, not a `this`). Fills a 5-entry serialised resource
    // descriptor whose entry[0] sizes the single block Initialize will carve up:
    //     entry[0] = { m_size = 4 * (8 * liMaxPairs + liNumParts + 5), m_alignment = 4 }
    //     entry[1..4] = { m_size = 0, m_alignment = 1 }
    // -------------------------------------------------------------------------------------
    static rw::BaseResourceDescriptors<5>* GetResourceDescriptor(
        rw::BaseResourceDescriptors<5>* lpResult, int liNumParts, int liMaxPairs);

    // Initialize @ 0x82BC7038 -- lay the header pointers over the freshly-allocated block
    // (r3 = the memory slot holding the block pointer) and thread the free list via ClearAll.
    static PairSet* Initialize(void** lpMemory, int liNumParts, int liMaxPairs);

    // ClearAll -- resets the buckets and rebuilds the free list. Defined by its own TU;
    // declared here because Initialize tail-calls it. (Not one of this TU's ledger funcs.)
    PairSet* ClearAll();

    // InitializeLink @ 0x82BC6E60 -- file link `liLinkIndex` into part `liPart`'s bucket
    // (push front). Returns this.
    PairSet* InitializeLink(int liLinkIndex, int liPart);

    // ReleaseLink @ 0x82BC6EB8 -- unlink `liLinkIndex` from its bucket list. Returns this.
    PairSet* ReleaseLink(int liLinkIndex);

    // LinkParts @ 0x82BC6F18 -- allocate a free pair joining part `liPartA` and part
    // `liPartB`, tagging it with `liData`. Returns the pair index, or -1 if the pool is full.
    int LinkParts(int liPartA, int liPartB, int liData);

    // UnlinkPair @ 0x82BC6F98 -- release both links of pair `liPairIndex` and return it to
    // the free list. Returns this.
    PairSet* UnlinkPair(int liPairIndex);

    // UnlinkParts @ 0x82BC6FF0 -- find the pair joining part `liPartA` to part `liPartB`
    // (walk liPartA's bucket for the link whose sibling is liPartB) and release it.
    PairSet* UnlinkParts(int liPartA, int liPartB);

private:
    Link* mpLinks;      // +0x00  base of the 2*miMaxPairs link array (points into this block)
    s32*  mpHeads;      // +0x04  base of the per-part bucket-head index array (-1 = empty)
    u32   miMaxPairs;   // +0x08  pair pool capacity
    u32   miNumParts;   // +0x0C  number of parts (mpHeads length)
    s32   miFreeList;   // +0x10  free pair-list head index (-1 = pool full)
};

} // namespace physics
} // namespace rw
