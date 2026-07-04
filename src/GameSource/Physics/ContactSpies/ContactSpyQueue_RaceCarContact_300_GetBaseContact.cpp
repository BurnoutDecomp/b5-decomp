#include "GameSource/Physics/ContactSpies/BrnContactSpyQueue.h"   // BrnPhysics::ContactSpy::ContactSpyQueue<T,N> (generic GetBaseContact body inline)

// BrnPhysics::ContactSpy::ContactSpyQueue<BrnPhysics::ContactSpy::RaceCarContact, 300>::GetBaseContact(int)
//   @ X360 0x825A3C38
// Thin explicit instantiation -- the generic GetBaseContact body is inline in BrnContactSpyQueue.h.
// The non-const overload (DWARF BrnContactSpyQueue.h:142). The X360 body carries the single checked
// index assert "liEventIndex < GetLength()" (BrnContactSpyQueue.h:144, `lwz r11,8(this)` miLength vs
// liIndex, `blt` skips) then returns the address of the indexed element inside the inline maEvents
// buffer at this+0x10: `result = 96*liIndex + this + 0x10` (`slwi r11,liIndex,1; add r11,liIndex,r11;
// slwi r11,r11,5` == liIndex*3*32 == liIndex*96, `add r11,r11,this; addi r3,r11,0x10`). The 96-byte
// element stride matches sizeof(RaceCarContact) == 96 (committed in BrnContactSpyEvents.h). Called by
// ContactSpyQueue<RaceCarContact,300>::GetNumUniqueEntities and the RaceCarContact SortAndCreateRunList pass.
template BrnPhysics::ContactSpy::BaseContact*
BrnPhysics::ContactSpy::ContactSpyQueue<BrnPhysics::ContactSpy::RaceCarContact, 300>::GetBaseContact(int);
