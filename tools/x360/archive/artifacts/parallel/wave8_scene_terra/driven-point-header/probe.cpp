#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKDrivenPoint.h"
using namespace BrnPhysics::Deformation;
void DrivenSet(IKDrivenPoint& p, Vector3 v) { p.SetPosition(v); }
void DrivenOffset(const IKDrivenPoint& p, Vector3& out) { out=p.GetOffsetFromInitialPosition(); }
const Vector3* DrivenPosition(const IKDrivenPoint& p) { return &p.GetPosition(); }
const Vector3* DrivenDirection(const IKDrivenPoint& p) { return &p.GetDirection(); }
