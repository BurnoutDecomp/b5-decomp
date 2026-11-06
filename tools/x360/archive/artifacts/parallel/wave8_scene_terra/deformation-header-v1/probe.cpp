#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnTagPoint.h"
void ProbeEvent(const BrnPhysics::Deformation::PhysicalBodyPart& p, Matrix44Affine& out) { out=p.GetEventRenderTransform(); }
void ProbeTag(const BrnPhysics::Deformation::TagPoint& p, Vector3& out) { out=p.GetOffsetFromInitialPosition(); }

const Vector3* ProbeSpecA(const BrnPhysics::Deformation::TagPointSpec& p) { return &p.GetOffsetFromDeformationSensorA(); }
const Vector3* ProbeSpecB(const BrnPhysics::Deformation::TagPointSpec& p) { return &p.GetOffsetFromDeformationSensorB(); }
const Vector3* ProbeSpecInitial(const BrnPhysics::Deformation::TagPointSpec& p) { return &p.GetInitialPosition(); }

void ProbeSetJoint(BrnPhysics::Deformation::PhysicalBodyPart& p, VecFloat v) { p.SetJointVelocity(v); }
