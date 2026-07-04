#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                 // BaseEventQueue<T>::GetEvent (inline generic)
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"            // BrnPhysics::ContactSpy::PropContact (112-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::PropContact>::GetEvent(s32) const  @ 0x822C7450
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked element accessor body is already inline
// in CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// BrnWorld::RaceCarEntityModule::ProcessPropContactQueue, BrnWorld::PropEntityModule::ProcessContacts,
// BrnGameState::GameStateModule::ProcessContacts and
// CollisionStateManager::ImportContactSpies<ContactSpyQueue<PropContact,100>> to walk the
// prop-contact queue.
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:272), liIndex < GetLength() (:274)
// and liIndex >= 0 (:275) -- the const GetEvent(int) const overload (DWARF :270) -- then returns
// &mpEvents[liIndex] (`result = 112*liIndex + mpEvents`, `mulli r11, liIndex, 0x70`). The 112-byte
// stride is sizeof(PropContact) == BaseContact(96) + u16 muType + u8 muState + u8 muFlags +
// u8 muBeganMoving, alignas(16) => 112. The Hex-Rays `int` return is the ABI-returned const-T&
// pointer; the DWARF (CgsBaseEventQueue.h:3122) gives the real `const PropContact&`.
template const BrnPhysics::ContactSpy::PropContact&
CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::PropContact>::GetEvent(s32) const;
