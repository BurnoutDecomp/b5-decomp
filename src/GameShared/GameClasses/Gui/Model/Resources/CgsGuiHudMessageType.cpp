#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiHudMessageType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::HudMessageResourceType::FixDown   @ 0x828465E8
//   CgsResource::HudMessageResourceType::FixUp     @ 0x828465D8
//   CgsResource::HudMessageResourceType::GetTypeID @ 0x82846578
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
