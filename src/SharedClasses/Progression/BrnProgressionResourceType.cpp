#include "SharedClasses/Progression/BrnProgressionResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnProgression::ProgressionResourceType::FixDown   @ 0x8267F480
//   BrnProgression::ProgressionResourceType::FixUp     @ 0x8267F490
//   BrnProgression::ProgressionResourceType::GetTypeID @ 0x82676B88
//
// FixUp/FixDown forward to BrnProgression::ProgressionData (own TU), passing the
// delta (the rw::Resource's load base).

namespace BrnProgression
{
    static const uint32_t KU_PROGRESSION_RESOURCE_TYPE_ID = 65550;

    uint32_t ProgressionResourceType::GetTypeID() const
    {
        return KU_PROGRESSION_RESOURCE_TYPE_ID;
    }

    void ProgressionResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<ProgressionData*>(lpResource)->FixDown(static_cast<int>(CgsResource::GetLoadBase(lrResource)));
    }

    void ProgressionResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<ProgressionData*>(lpResource)->FixUp(static_cast<int>(CgsResource::GetLoadBase(lrResource)));
    }
}
