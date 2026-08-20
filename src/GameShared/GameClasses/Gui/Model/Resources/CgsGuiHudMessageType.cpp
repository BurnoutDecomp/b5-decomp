#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiHudMessageType.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiHudMessage.h"  // CgsGui::GuiHudMessageResource (the REAL type)
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::HudMessageResourceType::FixDown                         @ 0x828465E8
//   CgsResource::HudMessageResourceType::FixUp                           @ 0x828465D8
//   CgsResource::HudMessageResourceType::GetTypeID                       @ 0x82846578
//   CgsResource::HudMessageResourceType::GetSerialisedResourceDescriptor @ 0x8267B110
//
// FixDown un-rebases the message pointer table (count at +8, base pointer at +0) by
// the delta (the rw::Resource's load base). FixUp forwards to the message resource's
// own FixUp (separate TU).

// ⭐ [gateui r4] CE-3: the TU-LOCAL `namespace CgsGui { struct GuiHudMessageResource {
// int FixUp(int); }; }` re-declaration that stood here is DELETED. It was an ODR fork of
// the real type in CgsGuiHudMessage.h (verify_r3_fix3hud NOTE-6) AND it declared a
// `FixUp(int)` that the real struct never had, so the two would not even have linked to
// one another. The real header is included above.

namespace CgsResource
{
    static const uint32_t KU_HUD_MESSAGE_RESOURCE_TYPE_ID = 44;

    uint32_t HudMessageResourceType::GetTypeID() const
    {
        return KU_HUD_MESSAGE_RESOURCE_TYPE_ID;
    }

    // GetSerialisedResourceDescriptor @ 0x8267B110. Five-entry descriptor: entry 0 is
    // { size = lpResource[1], alignment 16 }; entries 1..4 are { 0, 1 }. The serialised
    // size is read from the resource's second u32 (*(lpResource + 4)); alignment 16 is
    // hardcoded. (The Hex-Rays pseudocode hides the 64-bit first store of { size, 16 };
    // the layout is recovered from the PPC asm -- the final `std` writes size@+0 / 16@+4,
    // and the word stores fill entries 1..4 with {0,1}.)
    ResourceDescriptor HudMessageResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        // [gateui r4 verify S1] The console reads the size at +4 -- but that is the second
        // half of the CONSOLE 4-byte mppHudMessageData slot's neighbour; on the host the
        // pointer widened to 8 bytes, so the raw +4 read lands INSIDE the pointer (upper
        // half, zero on the shipped platform-4 payload). Read the field by NAME instead,
        // exactly as FixUp/FixDown in this TU now do (same widening family).
        const u32 luSize = reinterpret_cast<const CgsGui::GuiHudMessageResource*>(lpResource)->miSizeOfHudMessageResource;

        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16u;
        for (int li = 1; li < 5; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1u;
        }
        return lDescriptor;
    }

    // FixDown @0x828465E8. The exact inverse of GuiHudMessageResource::FixUp, and the
    // console open-codes it here rather than forwarding:
    //     v3 = *a2;                                    // the load base
    //     for (i = 0; i < *(result + 8); ++i)          // miHudMessageCount
    //         *(u32*)(*result + i*4) -= v3;            // each table entry
    //     *result -= v3;                               // then the table pointer itself
    // Note the ORDER is the mirror image of FixUp's (entries first, base last) -- which is
    // what makes it correct: the entries are read through the still-rebased base.
    //
    // [gateui r4] CE-3: the delta is now the full-width GetLoadBase64 and every field is
    // reached BY NAME through the real CgsGui::GuiHudMessageResource. The previous body
    // walked `*(int*)(base+8)` and `*(uintptr_t**)base` as raw offsets AND mixed a
    // truncated u32 delta into 64-bit slots -- on the x64 heap that subtracts the wrong
    // number from every entry. Same treatment as
    // BrnStreetData::StreetDataResourceType::FixDown/FixUp.
    void HudMessageResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        const uintptr_t luDelta = CgsResource::GetLoadBase64(lrResource);

        CgsGui::GuiHudMessageResource* lpMessageResource =
            static_cast<CgsGui::GuiHudMessageResource*>(lpResource);

        const s32 liCount = lpMessageResource->miHudMessageCount;
        for (s32 liEntry = 0; liEntry < liCount; ++liEntry)
        {
            lpMessageResource->mppHudMessageData[liEntry] =
                reinterpret_cast<CgsGui::GuiHudMessageData*>(
                    reinterpret_cast<uintptr_t>(lpMessageResource->mppHudMessageData[liEntry])
                    - luDelta);
        }

        lpMessageResource->mppHudMessageData = reinterpret_cast<CgsGui::GuiHudMessageData**>(
            reinterpret_cast<uintptr_t>(lpMessageResource->mppHudMessageData) - luDelta);
    }

    // FixUp @0x828465D8 -- a single tail call, verbatim:
    //     return CgsGui::GuiHudMessageResource::FixUp(a2, *a3);
    void HudMessageResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<CgsGui::GuiHudMessageResource*>(lpResource)->FixUp(
            CgsResource::GetLoadBase64(lrResource));
    }
}
