#include "SharedClasses/Sound/Engines/BrnSoundLoopModelResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnSound::Vehicles::Engines::LoopModelResourceType::FixDown   @ 0x826801E0
//   BrnSound::Vehicles::Engines::LoopModelResourceType::FixUp     @ 0x826801D0
//   BrnSound::Vehicles::Engines::LoopModelResourceType::GetTypeID @ 0x82675570
//
// FixUp/FixDown forward to LoopModelData (own TU), passing the delta (the
// rw::Resource's load base).

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{
    static const uint32_t KU_LOOP_MODEL_RESOURCE_TYPE_ID = 0x10000;

    uint32_t LoopModelResourceType::GetTypeID() const
    {
        return KU_LOOP_MODEL_RESOURCE_TYPE_ID;
    }

    void LoopModelResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<LoopModelData*>(lpResource)->FixDown(static_cast<int>(CgsResource::GetLoadBase(lrResource)));
    }

    void LoopModelResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<LoopModelData*>(lpResource)->FixUp(static_cast<int>(CgsResource::GetLoadBase(lrResource)));
    }
}
}
}
