#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                 // BaseEventQueue<T>::GetEvent (inline generic)
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"            // BrnPhysics::ContactSpy::HingedPartContact (112-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::HingedPartContact>::GetEvent(s32) const  @ 0x82285D10
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked element accessor body is already inline
// in CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// BrnEffects::EffectsModule::ProcessHingedPartContacts to walk the hinged-part contact queue.
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:272), liIndex < GetLength() (:274)
// and liIndex >= 0 (:275) -- the const GetEvent(int) const overload (DWARF :270) -- then returns
// &mpEvents[liIndex] (`result = 112*a2 + mpEvents`, `mulli r11, liIndex, 0x70`). The 112-byte stride
// is sizeof(HingedPartContact) == BaseContact(96) + EBodyParts meType, alignas(16) => 112. The
// Hex-Rays `int` return is the ABI-returned const-T& pointer; the DWARF (CgsBaseEventQueue.h:3055)
// gives the real `const HingedPartContact&`.
template const BrnPhysics::ContactSpy::HingedPartContact&
CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::HingedPartContact>::GetEvent(s32) const;
