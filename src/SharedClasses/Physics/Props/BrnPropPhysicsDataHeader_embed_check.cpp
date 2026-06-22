// Embed check: force the header-only TU to compile + instantiate its inline accessors.
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"

namespace
{
    // Pin the inline accessors so the header is exercised by the gate.
    uint32_t (BrnPhysics::Props::PropPhysicsDataHeader::*const kpGetNum)() const =
        &BrnPhysics::Props::PropPhysicsDataHeader::GetNumberOfPropTypes;

    const BrnPhysics::Props::PropTypeData* (BrnPhysics::Props::PropPhysicsDataHeader::*const kpGetType)(uint32_t) const =
        &BrnPhysics::Props::PropPhysicsDataHeader::GetType;

    const BrnPhysics::Props::PropPartTypeData* (BrnPhysics::Props::PropPhysicsDataHeader::*const kpGetPart)(uint32_t) const =
        &BrnPhysics::Props::PropPhysicsDataHeader::GetPartType;
}
