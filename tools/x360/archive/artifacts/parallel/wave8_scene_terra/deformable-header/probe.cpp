#define private public
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"
#undef private
VecFloat ProbeRadius(const BrnPhysics::Deformation::DeformableObject& p) { return p.GetEntitySphereSize(); }
