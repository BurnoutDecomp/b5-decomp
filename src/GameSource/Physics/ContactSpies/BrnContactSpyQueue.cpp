#include "GameSource/Physics/ContactSpies/BrnContactSpyQueue.h"   // BrnPhysics::ContactSpy::ContactSpyQueue<T,N> (generic body inline)

// BrnPhysics::ContactSpy::ContactSpyQueue<PhysicalCarPartContact, 150>::GetNumUniqueEntities
//   @ X360 0x825A3F08
// Thin explicit instantiation -- the generic GetNumUniqueEntities body is inline in
// BrnContactSpyQueue.h. Counts distinct consecutive-run mEntityIdA values across the live
// contacts (private per DWARF BrnContactSpyQueue.h:176): walks GetLength() elements, reading
// each &maEvents[i] as a BaseContact* and its mEntityIdA.muValue @+0, tallying transitions.
// The 128-byte element stride (loop `slwi ...,7` == *128) matches sizeof(PhysicalCarPartContact)
// == 128 (committed in BrnContactSpyEvents.h). Called by the physical-car-part contact-spy
// SortAndCreateRunList pass.
template int32_t
BrnPhysics::ContactSpy::ContactSpyQueue<BrnPhysics::ContactSpy::PhysicalCarPartContact, 150>::GetNumUniqueEntities();
