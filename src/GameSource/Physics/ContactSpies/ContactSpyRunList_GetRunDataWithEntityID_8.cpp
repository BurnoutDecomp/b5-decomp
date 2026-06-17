#include "GameSource/Physics/ContactSpies/BrnContactSpyRunList.h"

// Explicit instantiation of the generic ContactSpyRunList<MaxLength>::GetRunDataWithEntityID
// (inline body in BrnContactSpyRunList.h) for the <8> instantiation -- X360 0x82373C38.
// Namespace is BrnPhysics::ContactSpy (NOT CgsModule). EntityId is the global BrnCommonTypes.h type.
template const BrnPhysics::ContactSpy::ContactSpyRunData*
BrnPhysics::ContactSpy::ContactSpyRunList<8>::GetRunDataWithEntityID(EntityId) const;
