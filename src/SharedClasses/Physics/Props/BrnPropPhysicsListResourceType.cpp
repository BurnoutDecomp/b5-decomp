#include "SharedClasses/Physics/Props/BrnPropPhysicsListResourceType.h"
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"  // PropPhysicsDataHeader::FixUp/FixDown
#include "rw/rwcore_structs.h"                                      // rw::Resource complete for the calls
#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnPhysics::Props::PropPhysicsResourceType::FixDown @ 0x8267F750
//   BrnPhysics::Props::PropPhysicsResourceType::FixUp   @ 0x8267F760
//
// The serialised payload IS a BrnPhysics::Props::PropPhysicsDataHeader. Both fixups
// are thin forwarders: the X360 tail-calls PropPhysicsDataHeader::FixDown / ::FixUp
// with lpResource as the header `this` and the rw::Resource arg passed through. The
// header methods themselves are bodied by their own recon pass (declared in
// BrnPropPhysicsDataHeader.h).

namespace BrnPhysics
{
namespace Props
{
    // @ 0x8267F750 -- tail-call PropPhysicsDataHeader::FixDown(lrResource).
    void PropPhysicsResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        reinterpret_cast<PropPhysicsDataHeader*>(lpResource)->FixDown(lrResource);
    }

    // @ 0x8267F760 -- tail-call PropPhysicsDataHeader::FixUp(lrResource).
    void PropPhysicsResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        reinterpret_cast<PropPhysicsDataHeader*>(lpResource)->FixUp(lrResource);
    }
}
}
