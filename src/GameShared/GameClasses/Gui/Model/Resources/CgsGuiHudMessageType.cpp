#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiHudMessageType.h"
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

namespace CgsGui
{
    struct GuiHudMessageResource
    {
        int FixUp(int liDelta);
    };
}

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
        const u32 luSize = reinterpret_cast<const u32*>(lpResource)[1];   // r9 = *(a3 + 4)

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

    void HudMessageResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        int       liDelta = static_cast<int>(CgsResource::GetLoadBase(lrResource));
        uintptr_t lBase   = reinterpret_cast<uintptr_t>(lpResource);

        int liCount = *reinterpret_cast<int*>(lBase + 8);
        if (liCount > 0)
        {
            uintptr_t* lpTable = *reinterpret_cast<uintptr_t**>(lBase);
            for (int li = 0; li < liCount; ++li)
                lpTable[li] -= liDelta;
        }
        *reinterpret_cast<uintptr_t*>(lBase) -= liDelta;
    }

    void HudMessageResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<CgsGui::GuiHudMessageResource*>(lpResource)->FixUp(
            static_cast<int>(CgsResource::GetLoadBase(lrResource)));
    }
}
