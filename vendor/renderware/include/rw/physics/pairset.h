#pragma once

// =====================================================================================
// rw::physics::PairSet -- the EATech RenderWare physics "pair set": a fixed-capacity pool
// of undirected pairs (contacts / joints / drives) between physics parts, indexed so that
// every part can be walked for the pairs it participates in.
//
// EATech RenderWare physics. Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm
// is authoritative. Shares the rw::physics vocabulary with the sibling Simulation /
// SimulationWorkspace TUs.
//
// DecFIGS pairset.h declares unsigned link, part and pair indices, including the
// free-list head and bucket cursor. ARTIST compares part indices unsigned at 0x82BC7014
// and uses the all-ones index as the end marker at 0x82BC6FFC and 0x82BC7028.
//
// LAYOUT (recovered from the union of this TU's offset accesses; the X360 asm is
// authoritative for placement). The whole object is a single RW resource block, sized by
// GetResourceDescriptor and laid out by Initialize as:
//
//     [ 5-word header | Link mpLinks[2 * miMaxPairs] | u32 mpHeads[miNumParts] ]
//
// mpLinks / mpHeads are stored back-pointers INTO this same block; the +0x14 header stride
// and the trailing arrays are X360 (4-byte-word) block geometry fixed by the resource
// sizer, so they are treated as external serialised-block layout (semantic parity, not
// byte matching -- the PC/x64 pointer widths differ but nothing here relies on the offset).
//
// A "pair" is two Link nodes (link index = 2*pairIndex and 2*pairIndex+1). Each link is
// filed into the doubly-linked bucket list of one part (mpHeads[part]); its sibling link
// records the other part of the pair. Free pairs are threaded on link0.next through
// muFreeList.
// =====================================================================================

#include "types.hpp"                 // s32 / u32
#include "rw/rwcore_structs.h"       // rw::BaseResourceDescriptors<5>

namespace rw
{
namespace physics
{

class PairSet
{
    static const u32 KU_INVALID_INDEX = 0xFFFFFFFFu;

public:
    // One half of a pair: a node in a part's intrusive doubly-linked bucket list. 16 bytes.
    struct Link
    {
        u32 partIndex; // +0x00  bucket indexed by part
        u32 flags;     // +0x04  pair flags on the first link
        u32 next;      // +0x08  next bucket link or next free pair
        u32 prev;      // +0x0C  previous bucket link
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

    // ClearAll @ 0x82BC6DC0 -- resets the buckets and rebuilds the free list.
    void ClearAll();

    // LinkParts @ 0x82BC6F18 -- join two parts with a free pair and store its flags.
    // Returns the pair index or KU_INVALID_INDEX if the pool is full.
    u32 LinkParts(u32 luPartA, u32 luPartB, u32 luFlags);

    // UnlinkPair @ 0x82BC6F98 -- release both links and return the pair to the free list.
    void UnlinkPair(u32 luPairIndex);

    // UnlinkParts @ 0x82BC6FF0 -- find the pair joining two parts and release it.
    void UnlinkParts(u32 luPartA, u32 luPartB);

    // -------------------------------------------------------------------------------------
    // ⭐ ADDITIVE GROW 2026-08-06 (the game-side closure wave): the DWARF's bucket-walk API
    // (pairset.h:103/:104/:92 + the nested LinkIterator :153..:185), witnessed as the inline
    // the console folded into CgsPhysics::PhysicsSimulationModule::ActiveSetClosure
    // @0x828A0808 -- three identical walks (contact/jointed/driven sets):
    //   * begin: `lwz r10,4(set)` + `lwzx head, r10, 4*part`   == m_linkLists[part];
    //   * end/compare: the {PairSet*, cursor} pair is compared against {set, -1}
    //     (0x828A0C48..0x828A0C5C -- BOTH words, which is operator!= on the two-field
    //     iterator struct, not a bare index compare);
    //   * partner: `slwi r11,cur,4; xori r11,0x10; lwzx partner,(links)` ==
    //     m_links[cur ^ 1].partIndex == GetOtherPartIndex();
    //   * advance: `lwz next, 8(links + 16*cur)` == m_links[cur].next;
    //   * flags clear: `extlwi r11,cur,27,4` == (cur << 4) & ~0x1F == 32 * (cur >> 1)
    //     then `stw 0, 4(links + that)` == m_links[2 * GetPairIndex()].flags = 0 ==
    //     SetPairFlags(GetPairIndex(), 0) -- the LINK0 slot, proving the >>1 in
    //     GetPairIndex and the *2 in SetPairFlags.
    // The all-ones cursor marks the end of a bucket.
    // -------------------------------------------------------------------------------------
    struct LinkIterator
    {
        LinkIterator(PairSet* lpPairSet, u32 luCur) : mpPairSet(lpPairSet), muCur(luCur) {}

        // DWARF :166 -- the pair this link belongs to (link indices are 2*pair / 2*pair+1).
        u32 GetPairIndex() const { return muCur >> 1; }

        // DWARF :167 -- the OTHER part of the pair: the sibling link's filing bucket.
        u32 GetOtherPartIndex() const { return mpPairSet->mpLinks[muCur ^ 1u].partIndex; }

        // DWARF :171 -- chase the bucket's `next` thread.
        LinkIterator& operator++() { muCur = mpPairSet->mpLinks[muCur].next; return *this; }

        // DWARF :172 -- the console compares BOTH fields (see the banner).
        bool operator!=(const LinkIterator& lrOther) const
        { return mpPairSet != lrOther.mpPairSet || muCur != lrOther.muCur; }

    private:
        PairSet* mpPairSet;   // DWARF :184 m_pairSet
        u32      muCur;       // DWARF :185 m_cur (link index)
    };

    // DWARF :103 / :104. Begin = the part's bucket head; end = {this, -1}.
    LinkIterator PartLinksBegin(u32 luPart) { return LinkIterator(this, mpHeads[luPart]); }
    LinkIterator PartLinksEnd()            { return LinkIterator(this, KU_INVALID_INDEX); }

    // DWARF :92 -- write the pair's payload/flags word (carried on link0; see the Link
    // banner). The closure's only use stores 0.
    void SetPairFlags(u32 luPairIndex, u32 luFlags) { mpLinks[2u * luPairIndex].flags = luFlags; }

private:
    void InitializeLink(u32 luLinkIndex, u32 luPart); // @ 0x82BC6E60
    void ReleaseLink(u32 luLinkIndex);                // @ 0x82BC6EB8

    Link* mpLinks;      // +0x00  base of the 2*miMaxPairs link array (points into this block)
    u32*  mpHeads;      // +0x04  base of the per-part bucket-head index array
    u32   miMaxPairs;   // +0x08  pair pool capacity
    u32   miNumParts;   // +0x0C  number of parts (mpHeads length)
    u32   muFreeList;   // +0x10  free pair-list head index
};

} // namespace physics
} // namespace rw
