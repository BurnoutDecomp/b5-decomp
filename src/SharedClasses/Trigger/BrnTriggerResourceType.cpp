#include "SharedClasses/Trigger/BrnTriggerResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTrigger::TriggerResourceType::FixDown   @ 0x826800C8
//   BrnTrigger::TriggerResourceType::FixUp     @ 0x826800D8
//   BrnTrigger::TriggerResourceType::GetTypeID @ 0x826765D0
//
// FixUp/FixDown forward to BrnTrigger::TriggerData (own TU), passing the delta (the
// rw::Resource's load base). ⚠️ WIDENED to GetLoadBase64: the 32-bit GetLoadBase
// truncated the x64 allocation address, and the ported TRIGGERS.DAT now carries
// 64-bit pointer slots.

namespace BrnTrigger
{
    static const uint32_t KU_TRIGGER_RESOURCE_TYPE_ID = 65539;

    uint32_t TriggerResourceType::GetTypeID() const
    {
        return KU_TRIGGER_RESOURCE_TYPE_ID;
    }

    void TriggerResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<TriggerData*>(lpResource)->FixDown(CgsResource::GetLoadBase64(lrResource));
    }

    void TriggerResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<TriggerData*>(lpResource)->FixUp(CgsResource::GetLoadBase64(lrResource));
    }
}
