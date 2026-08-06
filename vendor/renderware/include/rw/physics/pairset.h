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
// ⚠️⚠️ CORRECTION 2026-08-04 -- THE SENTENCE THAT USED TO STAND HERE SAID
//      "No Feb-2007 reference source and no DecFIGS DWARF exist for this TU."
//      **THAT WAS FALSE.** references/DecFIGS/dwarfdump/SDKs/EATech/include/cmn/rw/physics/
//      carries twenty real headers including pairset.h, and the identical false claim in the
//      sibling rw/physics/simulation.h cost three waves of the physics campaign. The DWARF
//      names for this type are:
//          Link  { partIndex, flags, next, prev }        (pairset.h:121..125)
//          PairSet { m_links, m_linkLists, m_maxPairs, m_maxParts, m_freePair }  (:135..140)
//      plus a nested LinkIterator { m_pairSet, m_cur } (:153..185) and the public method set
//      SetPairFlags / GetPairFlags / UnlinkPart / ClearAll / PartLinksBegin / PartLinksEnd /
//      PairIsValid, none of which this reconstruction declares.
//      ⭐ The recovered LAYOUT below is confirmed by that DWARF member-for-member -- the
//      structural reconstruction was right. Only the NAMES differ (miData is `flags`,
//      miFreeList is `m_freePair`, ...). The rename is deliberately NOT done in this pass:
//      this is a serialised resource block with live users, and the rw physics landing that
//      wrote this note had no reason to touch it. Do it as its own change, with a boot.
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
    // Method NAMES and shapes are the DWARF's; member spellings stay this header's
    // (the DWARF-name mapping is the banner above: miData == `flags`, mpHeads ==
    // `m_linkLists`, ... -- the standing no-rename rule).
    // ⚠️ Cursor width: the DWARF types m_cur uint32_t with (u32)-1 as the end sentinel; the
    // console's compare is `cmpwi cur, -1`. s32 here, matching this header's miNext/-1
    // convention -- value-identical.
    // -------------------------------------------------------------------------------------
    struct LinkIterator
    {
        LinkIterator(PairSet* lpPairSet, s32 liCur) : mpPairSet(lpPairSet), miCur(liCur) {}

        // DWARF :166 -- the pair this link belongs to (link indices are 2*pair / 2*pair+1).
        s32 GetPairIndex() const { return miCur >> 1; }

        // DWARF :167 -- the OTHER part of the pair: the sibling link's filing bucket.
        s32 GetOtherPartIndex() const { return mpPairSet->mpLinks[miCur ^ 1].miPart; }

        // DWARF :171 -- chase the bucket's `next` thread.
        LinkIterator& operator++() { miCur = mpPairSet->mpLinks[miCur].miNext; return *this; }

        // DWARF :172 -- the console compares BOTH fields (see the banner).
        bool operator!=(const LinkIterator& lrOther) const
        { return mpPairSet != lrOther.mpPairSet || miCur != lrOther.miCur; }

    private:
        PairSet* mpPairSet;   // DWARF :184 m_pairSet
        s32      miCur;       // DWARF :185 m_cur (link index; -1 = end)
    };

    // DWARF :103 / :104. Begin = the part's bucket head; end = {this, -1}.
    LinkIterator PartLinksBegin(s32 liPart) { return LinkIterator(this, mpHeads[liPart]); }
    LinkIterator PartLinksEnd()             { return LinkIterator(this, -1); }

    // DWARF :92 -- write the pair's payload/flags word (carried on link0; see the Link
    // banner). The closure's only use stores 0.
    void SetPairFlags(s32 liPairIndex, u32 luFlags) { mpLinks[2 * liPairIndex].miData = static_cast<s32>(luFlags); }

private:
    Link* mpLinks;      // +0x00  base of the 2*miMaxPairs link array (points into this block)
    s32*  mpHeads;      // +0x04  base of the per-part bucket-head index array (-1 = empty)
    u32   miMaxPairs;   // +0x08  pair pool capacity
    u32   miNumParts;   // +0x0C  number of parts (mpHeads length)
    s32   miFreeList;   // +0x10  free pair-list head index (-1 = pool full)
};

} // namespace physics
} // namespace rw
