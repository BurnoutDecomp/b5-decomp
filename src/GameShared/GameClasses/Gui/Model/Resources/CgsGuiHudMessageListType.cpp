#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiHudMessageListType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the body
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::HudMessageListResourceType::FixDown   @ 0x82846648
//   CgsResource::HudMessageListResourceType::GetTypeID @ 0x828465D0
//
// FixDown is the inverse (save-time) relocation: it subtracts the delta (the
// rw::Resource's load base) from every one of the `count` (offset 4) entry pointers,
// then from the table base pointer (offset 8) itself.

namespace CgsResource
{
    static const uint32_t KU_HUD_MESSAGE_LIST_RESOURCE_TYPE_ID = 45;

    uint32_t HudMessageListResourceType::GetTypeID() const
    {
        return KU_HUD_MESSAGE_LIST_RESOURCE_TYPE_ID;
    }

    void HudMessageListResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        int       liDelta = static_cast<int>(CgsResource::GetLoadBase(lrResource));
        uintptr_t lBase   = reinterpret_cast<uintptr_t>(lpResource);

        int liCount = *reinterpret_cast<int*>(lBase + 4);
        if (liCount > 0)
        {
            // The table entries themselves are X360 4-byte values (lwz/stw, stride
            // of 4 in the asm) -- keep int32_t here rather than widening to native
            // pointer width, or the per-entry stride would no longer match the asm.
            int32_t* lpTable = *reinterpret_cast<int32_t**>(lBase + 8);
            for (int li = 0; li < liCount; ++li)
                lpTable[li] -= liDelta;
        }
        *reinterpret_cast<uintptr_t*>(lBase + 8) -= liDelta;
    }
}
