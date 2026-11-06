#include "GameShared/GameClasses/Geometric/Primitives/CgsCapsule.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsBox.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsCylinder.h"
CGS_STATIC_ASSERT(sizeof(CgsGeometric::Capsule)==32,"capsule");
CGS_STATIC_ASSERT(sizeof(CgsGeometric::Box)==80,"box");
void probe(CgsGeometric::Capsule& a, CgsGeometric::Box& b, CgsGeometric::Cylinder& c, Vector3 x, VecFloat f) { a.Set(x,x,f,f); f=a.GetRadius(); f=a.GetLength(); f=b.GetFatness(); f=c.GetRadius(); f=c.GetLength(); }
