#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                 // BaseEventQueue<T>::GetEvent (inline generic)
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"            // BrnPhysics::ContactSpy::RaceCarContact (96-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::RaceCarContact>::GetEvent(s32) const  @ 0x82285BB8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked element accessor body is already inline
// in CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// BrnEffects::EffectsModule::ProcessRaceCarContacts, BrnWorld::RaceCarEntityModule::Update{Crashing,}RaceCarContacts,
// BrnPhysics::Vehicle::VehicleManager::Process{ShowtimeShunts,ContactSpies} and
// CollisionStateManager::ImportContactSpies<ContactSpyQueue<RaceCarContact,300>> to walk the
// race-car contact queue.
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:272), liIndex < GetLength() (:274)
// and liIndex >= 0 (:275) -- the const GetEvent(int) const overload (DWARF :270) -- then returns
// &mpEvents[liIndex] (`result = 96*liIndex + mpEvents`, `slwi r11,liIndex,1; add r11,liIndex,r11;
// slwi r11,r11,5` == liIndex*3*32 == liIndex*96). The 96-byte stride is sizeof(RaceCarContact) == 96
// (adds no members over BaseContact). The Hex-Rays `int` return is the ABI-returned const-T&
// pointer; the DWARF (CgsBaseEventQueue.h:2854) gives the real `const RaceCarContact&`.
template const BrnPhysics::ContactSpy::RaceCarContact&
CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::RaceCarContact>::GetEvent(s32) const;
