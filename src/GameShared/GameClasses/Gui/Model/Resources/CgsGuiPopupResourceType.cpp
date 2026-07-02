#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiPopupResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::GuiPopupResourceType::FixDown                       @ 0x82851FA8
//   CgsResource::GuiPopupResourceType::FixUp                         @ 0x82851F98
//   CgsResource::GuiPopupResourceType::GetSerialisedResourceDescriptor @ 0x82855A18
//   CgsResource::GuiPopupResourceType::GetTypeID                     @ 0x8284BC90
//
// FixUp/FixDown forward to CgsGui::GuiPopupResource (own TU); FixDown passes the
// "deep" flag and the delta (the rw::Resource's load base). GetSerialisedResource-
// Descriptor returns the five-entry descriptor: entry 0 is {size=count, align=16},
// the remaining four are {size=count, align=1}; count is read from the resource.

namespace CgsGui
{
    struct GuiPopupResource
    {
        GuiPopupResource* FixUp(int liDelta);
        GuiPopupResource* FixDown(int liDelta, bool lbDeep);
    };
}

namespace CgsResource
{
    static const uint32_t KU_GUI_POPUP_RESOURCE_TYPE_ID = 31;

    uint32_t GuiPopupResourceType::GetTypeID() const
    {
        return KU_GUI_POPUP_RESOURCE_TYPE_ID;
    }

    void GuiPopupResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<CgsGui::GuiPopupResource*>(lpResource)->FixDown(
            static_cast<int>(CgsResource::GetLoadBase(lrResource)), true);
    }

    void GuiPopupResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<CgsGui::GuiPopupResource*>(lpResource)->FixUp(
            static_cast<int>(CgsResource::GetLoadBase(lrResource)));
    }

    ResourceDescriptor GuiPopupResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        // Count is a signed 16-bit field at byte offset 6 in the X360 build
        // (lhz + extsh), sign-extended to 32 bits.
        s16 liCount16 = *reinterpret_cast<const s16*>(reinterpret_cast<uintptr_t>(lpResource) + 6);
        u32 luCount   = static_cast<u32>(static_cast<s32>(liCount16));

        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luCount;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
        for (int li = 1; li < 5; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = luCount;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        return lDescriptor;
    }
}
