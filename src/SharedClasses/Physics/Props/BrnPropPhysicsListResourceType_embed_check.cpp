// Embed check for BrnPhysics::Props::PropPhysicsResourceType FixDown/FixUp:
//   BrnPhysics::Props::PropPhysicsResourceType::FixDown @ 0x8267F750
//   BrnPhysics::Props::PropPhysicsResourceType::FixUp   @ 0x8267F760
//
// GetTypeID/GetSerialisedResourceDescriptor are sibling virtuals owned by other
// recon passes; the type is NOT instantiated. The bodied forwarders are invoked via
// explicit qualified (non-virtual) calls on a placement reference. The forwarded-to
// PropPhysicsDataHeader::FixUp/FixDown are bodied by their own recon pass.

#include "SharedClasses/Physics/Props/BrnPropPhysicsListResourceType.h"
#include "rw/rwcore_structs.h"

using namespace BrnPhysics::Props;

void ExercisePropPhysicsFix(const PropPhysicsResourceType& lrType, void* lpResource,
                            const rw::Resource& lrRes)
{
    lrType.PropPhysicsResourceType::FixDown(lpResource, lrRes);
    lrType.PropPhysicsResourceType::FixUp(lpResource, lrRes);
}
