#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"

// ============================================================================
// GameShared/GameClasses/Sound/Logic/CgsVoiceHierarchy.cpp
//
// CgsSound::Logic::VoiceHierarchyNode::FixUp -- the resource-relocation pass for
// one node of a serialised voice hierarchy. Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//
//   CgsSound::Logic::VoiceHierarchyNode::FixUp @ 0x826A5C78
//
// A VoiceHierarchyNode is deserialised in place from a resource blob whose
// internal pointers are stored as byte offsets relative to the blob base. FixUp
// is handed the owning resource (whose first word is that base address) and
// rebases the node's two stored pointers, then validates its other-send array.
//
// Behaviour-faithful to the X360 asm (store-for-store):
//
//   delta = *(Resource*)a2;                  // lwz r10, 0(r4)  -- the base addr
//   if (mpcDebugName != 0)                    // lwz r11,0(r31); cmplwi 0; beq
//       mpcDebugName += delta;                // add; stw r11,0(r31)
//   if (miNumOtherSends > 0)                  // lwz r9,0x14; cmpwi; ble
//       mpOtherSends += delta;                // add; stw r11,0x10(r31)
//   for (i = 0; i < miNumOtherSends; ++i)     // li r29,0; loop on r29 vs *(this+0x14)
//   {
//       // inlined GetOtherSend(i) guards (CgsVoiceHierarchy.h:764/765):
//       if (i >= miNumOtherSends)  CGS_ASSERT(... "Requested other send number out of range")
//       if (mpOtherSends == 0)     CGS_ASSERT(mpOtherSends, "mpOtherSends")
//   }
//
// The X360 reads the loop bound (this+0x14) and the pointer (this+0x10) FRESH on
// every iteration (lwz r11,0x14(r31) / lwz r11,0x10(r31) inside the loop), so the
// reconstruction re-reads the members each pass rather than caching them. The two
// assert sites are the inlined accessor's range/non-null checks at .h lines 764
// (0x2FC) and 765 (0x2FD); FireAssert's message + file are reproduced exactly.
//
// The stored "pointers" are byte offsets pre-relocation, so the rebase is pointer
// arithmetic on a byte delta -- modelled BY NAME on byte-addressed members. The
// Resource is reached only for its base-address word; it is forward-modelled with
// a single base accessor (the X360 `*a2` first-word read) rather than pulling in
// the full CgsResource::Resource surface (deferred to its own TU).
// ============================================================================

namespace CgsResource
{
    // The owning resource. FixUp reads only its first word -- the address of the
    // deserialised blob's base, against which the node's stored byte offsets are
    // rebased into live pointers. Forward-modelled BY NAME (the X360 `lwz r10,0(r4)`
    // is exactly this load); the full Resource (CgsResourceType.h) is another TU.
    struct Resource
    {
        // The blob base address. The X360 stores it as the resource's first word and
        // FixUp loads it as `*a2`.
        uintptr_t muBaseAddress;

        // Read the relocation base. Named replacement for the raw first-word load.
        uintptr_t GetBaseAddress() const { return muBaseAddress; }
    };
}

namespace CgsSound
{
namespace Logic
{

// One outgoing send descriptor referenced by a node's other-send array. Only its
// existence (as the element type of mpOtherSends) is load-bearing for FixUp; the
// full SendDescriptor (CgsVoiceHierarchy.h:455) is reconstructed in its own slice.
struct SendDescriptor;

// CgsVoiceHierarchy.h:88. A single node of a serialised voice hierarchy. Modelled
// here with the members FixUp touches, at their X360 word positions (proven by the
// asm offsets 0x00 / 0x10 / 0x14):
//   mpcDebugName  @ +0x00  (word 0)   -- relocated if non-null
//   mpOtherSends  @ +0x10  (word 4)   -- relocated if miNumOtherSends > 0
//   miNumOtherSends @ +0x14 (word 5)  -- loop bound
// The intervening words 1..3 (mId / mParentId / mDefaultSendName, each a 4-byte
// QueueElement on X360) are pinned BY NAME so the two relocated pointers land at
// their observed word indices. FLAG: MINIMAL home for the FixUp TU -- the full
// VoiceHierarchyNode API (CgsVoiceHierarchy.h:113-267 accessors/mutators) and the
// trailing members (mVoiceSpec / meVoiceType / mb8IsGlobal) are DEFERRED to the
// node's own TU; this slice models only the relocation-relevant prefix.
struct VoiceHierarchyNode
{
public:
    // CgsVoiceHierarchy.h:267. Rebase the node's stored offsets against the owning
    // resource's base, then validate the other-send array. @ 0x826A5C78.
    void FixUp(const CgsResource::Resource& lrResource);

private:
    // CgsVoiceHierarchy.h:283 (+0x00). Debug name -- a stored byte offset before
    // relocation, rebased to a live pointer when non-zero.
    const char* mpcDebugName;

    // CgsVoiceHierarchy.h:284/287/288 (+0x04/+0x08/+0x0C). mId / mParentId /
    // mDefaultSendName, each a 4-byte QueueElement on X360. Pinned BY NAME so the
    // relocated members below sit at their observed word indices. Untouched by FixUp.
    u32 mId;
    u32 mParentId;
    u32 mDefaultSendName;

    // CgsVoiceHierarchy.h:290 (+0x10). The other-send array -- a stored byte offset
    // before relocation, rebased when there is at least one send.
    const SendDescriptor* mpOtherSends;

    // CgsVoiceHierarchy.h:291 (+0x14). Number of entries in mpOtherSends; the FixUp
    // validation loop bound.
    s32 miNumOtherSends;
};

void VoiceHierarchyNode::FixUp(const CgsResource::Resource& lrResource)
{
    // delta = base address of the deserialised blob (the X360 `*a2`).
    const uintptr_t luDelta = lrResource.GetBaseAddress();

    // Relocate the debug-name offset into a live pointer (only if present).
    if (mpcDebugName != 0)
    {
        mpcDebugName = reinterpret_cast<const char*>(
            reinterpret_cast<uintptr_t>(mpcDebugName) + luDelta);
    }

    // Relocate the other-send array offset into a live pointer (only if non-empty).
    if (miNumOtherSends > 0)
    {
        mpOtherSends = reinterpret_cast<const SendDescriptor*>(
            reinterpret_cast<uintptr_t>(mpOtherSends) + luDelta);
    }

    // Validate every other-send entry. The X360 re-reads miNumOtherSends and
    // mpOtherSends from memory on each pass (the loads sit inside the loop), so the
    // bound and pointer are re-evaluated each iteration. The two checks are the
    // inlined GetOtherSend(i) guards (CgsVoiceHierarchy.h:764/765).
    for (s32 liIndex = 0; liIndex < miNumOtherSends; ++liIndex)
    {
        if (liIndex >= miNumOtherSends)
        {
            CGS_ASSERT(liIndex < miNumOtherSends,
                       "Requested other send number out of range\n");
        }
        if (mpOtherSends == 0)
        {
            CGS_ASSERT(mpOtherSends, "mpOtherSends");
        }
    }
}

} // namespace Logic
} // namespace CgsSound
