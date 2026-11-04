#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceHierarchyResourceType.h"
#include <cstring>

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
// Behaviour-faithful to the X360 asm (store-for-store), and reconciled against the
// PS3 DecFIGS body (._ZN8CgsSound5Logic18VoiceHierarchyNode5FixUpERKN2rw8ResourceE
// @ PS3 0x833624), which keeps the named `const rw::Resource&` parameter the X360
// obscured to `int*a2`:
//
//   delta = *(rw::Resource*)a2;              // lwz r11, 0(r4)  -- the base addr
//   if (mpcDebugName != 0)                    // lwz r0,0(r31); cmpdi 0; beq
//       mpcDebugName += delta;                // add; stw r0,0(r31)
//   if (miNumOtherSends > 0)                  // lwz r9,0x14; cmpwi; ble
//   {
//       mpOtherSends += delta;                // add; stw r0,0x10(r31)
//       for (i = 0; i < miNumOtherSends; ++i) // li r31,0; loop on r31 vs *(this+0x14)
//       {
//           // inlined GetOtherSend(i) guards (CgsVoiceHierarchy.h:764/765); both
//           // are evaluated every pass and only FIRE on failure:
//           if (i >= miNumOtherSends)  CGS_ASSERT(... "Requested other send number out of range\n")  // :764
//           if (mpOtherSends == 0)     CGS_ASSERT(mpOtherSends, "mpOtherSends")                      // :765
//       }
//   }
//
// The PS3 re-reads the loop bound (this+0x14) FRESH on every iteration (lwz r9,0x14
// inside the loop), so the reconstruction re-reads miNumOtherSends each pass. The
// two assert sites are the inlined accessor's range/non-null checks at .h lines 764
// (0x2FC) and 765 (0x2FD); FireAssert's message + file are reproduced exactly (PS3
// FireAssert cites CgsVoiceHierarchy.h:764/765 with these strings).
//
// The stored "pointers" are byte offsets pre-relocation, so the rebase is pointer
// arithmetic on a byte delta -- modelled BY NAME on byte-addressed members. The
// Resource parameter is the canonical vendor rw::Resource
// (: public BaseResources<5>); the X360/PS3 first-word load (`lwz r11,0(r4)`) is
// its m_baseResources[0] slot, read by name below. (An earlier revision of this TU
// fabricated a local single-word rw::Resource -- an ODR violation against the
// vendor type, retired 2026-08-25 audio-faithfulness wave 1.)
// ============================================================================

#include "rw/rwcore_structs.h"   // the canonical five-lane rw::Resource

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
// their observed word indices. The trailing fields restore the 36-byte ARTIST
// stride; the remaining accessor API is not needed here.
struct VoiceHierarchyNode
{
    friend class CgsResource::VoiceHierarchyResourceType;
public:
    // CgsVoiceHierarchy.h:267. Rebase the node's stored offsets against the owning
    // resource's base, then validate the other-send array. @ X360 0x826A5C78 /
    // PS3 0x833624.
    void FixUp(const rw::Resource& lrResource);

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
    u32 mVoiceSpec;
    enum eVoiceType { E_VOICE_TYPE_INVALID = 0, E_VOICE_TYPE_UNKNOWN = 1,
                      E_VOICE_TYPE_PLAYER = 2, E_VOICE_TYPE_SUBMIX = 3,
                      E_VOICE_TYPE_COUNT = 4 } meVoiceType;
    bool mb8IsGlobal;
};

CGS_STATIC_ASSERT((sizeof(VoiceHierarchyNode) == 36), "ARTIST voice node stride");

struct ExternalConnectionDescriptor;
// DecFIGS CgsVoiceHierarchy.h:315, gated by ARTIST 826A5D40 / 826C5328.
struct VoiceHierarchy
{
    const VoiceHierarchyNode* mpNodes;
    s32 miNumNodes;
    const ExternalConnectionDescriptor* mpExternalConnections;
    s32 miNumExternalConnections;
    s32 miMapState;
    u8 mu8DebugDataPresent;
};
CGS_STATIC_ASSERT((sizeof(VoiceHierarchy) == 24), "ARTIST voice hierarchy size");

void VoiceHierarchyNode::FixUp(const rw::Resource& lrResource)
{
    // delta = base address of the deserialised blob -- the resource's first word
    // (the PS3 `*a2` == rw::BaseResources<5>::m_baseResources[0], read by name).
    const uintptr_t luDelta = reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]);

    // Relocate the debug-name offset into a live pointer (only if present).
    if (mpcDebugName != 0)
    {
        mpcDebugName = reinterpret_cast<const char*>(
            reinterpret_cast<uintptr_t>(mpcDebugName) + luDelta);
    }

    // Relocate the other-send array offset and validate it, only when non-empty (the
    // PS3 guards the whole block on miNumOtherSends > 0).
    if (miNumOtherSends > 0)
    {
        mpOtherSends = reinterpret_cast<const SendDescriptor*>(
            reinterpret_cast<uintptr_t>(mpOtherSends) + luDelta);

        // Validate every other-send entry. The PS3 re-reads miNumOtherSends from
        // memory on each pass (lwz r9,0x14 sits inside the loop), so the bound is
        // re-evaluated each iteration. The two checks are the inlined GetOtherSend(i)
        // guards (CgsVoiceHierarchy.h:764/765): the range check and the non-null
        // check are both evaluated every pass and only fire on failure.
        for (s32 liIndex = 0; liIndex < miNumOtherSends; ++liIndex)
        {
            CGS_ASSERT(liIndex < miNumOtherSends,
                       "Requested other send number out of range\n");   // :764
            CGS_ASSERT(mpOtherSends, "mpOtherSends");                    // :765
        }
    }
}

} // namespace Logic
} // namespace CgsSound

namespace CgsResource
{
    // ARTIST 826C5328: include node, send, connection and optional debug-name bytes.
    ResourceDescriptor VoiceHierarchyResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const CgsSound::Logic::VoiceHierarchy* lpHierarchy = static_cast<const CgsSound::Logic::VoiceHierarchy*>(lpResource);
        const s32 liNumNodes = lpHierarchy->miNumNodes;
        s32 liSize = 8 * (lpHierarchy->miNumExternalConnections + 3) + 36 * liNumNodes;
        for (s32 i = 0; i < liNumNodes; ++i)
        {
            if (i >= lpHierarchy->miNumNodes)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("Requested node number out of range\n", "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsVoiceHierarchy.h", 889);
                CgsDev::Assert::EndAssert();
            }
            if (!lpHierarchy->mpNodes)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("mpNodes", "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsVoiceHierarchy.h", 890);
                CgsDev::Assert::EndAssert();
            }
            const CgsSound::Logic::VoiceHierarchyNode& lrNode = lpHierarchy->mpNodes[i];
            liSize += 16 * lrNode.miNumOtherSends;
            if (lpHierarchy->mu8DebugDataPresent)
            {
                if (!lrNode.mpcDebugName)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert("Debug data not set\n", "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsVoiceHierarchy.h", 728);
                    CgsDev::Assert::EndAssert();
                }
                liSize += static_cast<s32>(std::strlen(lrNode.mpcDebugName)) + 1;
            }
        }
        ResourceDescriptor lResult;
        lResult.m_baseResourceDescriptors[0].m_size = liSize;
        lResult.m_baseResourceDescriptors[0].m_alignment = 16;
        return lResult;
    }

    // ARTIST 826A5D40: rebase both pointers even when their stored offsets are zero.
    void VoiceHierarchyResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        CgsSound::Logic::VoiceHierarchy* lpHierarchy = static_cast<CgsSound::Logic::VoiceHierarchy*>(lpResource);
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lrResource.m_baseResources[0]);
        lpHierarchy->mpNodes = reinterpret_cast<const CgsSound::Logic::VoiceHierarchyNode*>(reinterpret_cast<uintptr_t>(lpHierarchy->mpNodes) + luBase);
        lpHierarchy->mpExternalConnections = reinterpret_cast<const CgsSound::Logic::ExternalConnectionDescriptor*>(reinterpret_cast<uintptr_t>(lpHierarchy->mpExternalConnections) + luBase);
        for (s32 i = 0; i < lpHierarchy->miNumNodes; ++i)
        {
            if (i >= lpHierarchy->miNumNodes)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("Requested node number out of range\n", "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsVoiceHierarchy.h", 889);
                CgsDev::Assert::EndAssert();
            }
            if (!lpHierarchy->mpNodes)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("mpNodes", "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsVoiceHierarchy.h", 890);
                CgsDev::Assert::EndAssert();
            }
            const_cast<CgsSound::Logic::VoiceHierarchyNode&>(lpHierarchy->mpNodes[i]).FixUp(lrResource);
        }
        for (s32 i = 0; i < lpHierarchy->miNumExternalConnections; ++i)
        {
            if (i >= lpHierarchy->miNumExternalConnections)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("Requested external connection number out of range\n", "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsVoiceHierarchy.h", 943);
                CgsDev::Assert::EndAssert();
            }
            if (!lpHierarchy->mpExternalConnections)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("mpExternalConnections", "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsVoiceHierarchy.h", 944);
                CgsDev::Assert::EndAssert();
            }
        }
    }
}
