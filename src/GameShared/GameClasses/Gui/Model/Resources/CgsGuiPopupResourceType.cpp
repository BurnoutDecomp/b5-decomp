#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiPopupResourceType.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiPopupResource.h"  // CgsGui::GuiPopupResource (the real type)
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// CgsResource::GuiPopupResourceType -- the resource-type handler for Popups.pup (type 31).
//
// FixUp / FixDown are single tail calls into CgsGui::GuiPopupResource with the rw::Resource's
// load base (FixDown passes the deep flag set). The delta is the full-width load base: the
// shipped POPUPS.PUP carries 8-byte pointer slots.

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
            CgsResource::GetLoadBase64(lrResource), true);
    }

    void GuiPopupResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<CgsGui::GuiPopupResource*>(lpResource)->FixUp(
            CgsResource::GetLoadBase64(lrResource));
    }

    // Five-entry descriptor: entry 0 is { miSizeOfPopupResource, 16 } (the console reads the
    // signed 16-bit field at +0x06), entries 1..4 are { 0, 1 }. The size is read by NAME: on
    // the host the pointer slot in front of it is 8 bytes wide.
    ResourceDescriptor GuiPopupResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const s32 liSize = static_cast<const CgsGui::GuiPopupResource*>(lpResource)->miSizeOfPopupResource;

        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = static_cast<u32>(liSize);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16u;
        for (int li = 1; li < 5; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1u;
        }
        return lDescriptor;
    }
}
