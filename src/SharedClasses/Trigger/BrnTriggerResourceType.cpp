#include "SharedClasses/Trigger/BrnTriggerResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTrigger::TriggerResourceType::FixDown   @ 0x826800C8
//   BrnTrigger::TriggerResourceType::FixUp     @ 0x826800D8
//   BrnTrigger::TriggerResourceType::GetTypeID @ 0x826765D0
//
// FixUp/FixDown pass the native segment-0 load base to the trigger data.

namespace BrnTrigger
{
    static const uint32_t KU_TRIGGER_RESOURCE_TYPE_ID = 65539;

    uint32_t TriggerResourceType::GetTypeID() const
    {
        return KU_TRIGGER_RESOURCE_TYPE_ID;
    }

    void TriggerResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<TriggerData*>(lpResource)->FixDown(CgsResource::GetLoadBase(lrResource));
    }

    void TriggerResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<TriggerData*>(lpResource)->FixUp(CgsResource::GetLoadBase(lrResource));
    }
}
