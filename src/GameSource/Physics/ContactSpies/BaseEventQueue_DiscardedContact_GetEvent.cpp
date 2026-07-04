#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                 // BaseEventQueue<T>::GetEvent (inline generic)
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"            // BrnPhysics::ContactSpy::DiscardedContact (64-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::DiscardedContact>::GetEvent(s32) const  @ 0x8259D898
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked element accessor body is already inline
// in CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// BrnPhysics::PhysicsModule::BridgeSimulationToOutput and
// CollisionStateManager::ImportContactSpies<EventQueue<DiscardedContact,20>> to walk the
// discarded-contact queue.
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:272), liIndex < GetLength() (:274)
// and liIndex >= 0 (:275) -- the const GetEvent(int) const overload (DWARF :270) -- then returns
// &mpEvents[liIndex] (`result = (liIndex << 6) + mpEvents`, `slwi r11, liIndex, 6`). The 64-byte
// stride is sizeof(DiscardedContact) == EntityId(4) + EntityId(4) + f32(4), then three 16-byte
// Vector3s @16/32/48, alignas(16) => 64. The Hex-Rays `int` return is the ABI-returned const-T&
// pointer; the DWARF (CgsBaseEventQueue.h:3189) gives the real `const DiscardedContact&`.
template const BrnPhysics::ContactSpy::DiscardedContact&
CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::DiscardedContact>::GetEvent(s32) const;
