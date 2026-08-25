#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"   // BaseEventQueue<T>::GetEvent (inline generic)
#include "types.hpp"

// CgsModule::BaseEventQueue<T>::GetEvent(s32)  @ 0x8268EA38
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked element accessor body is already inline
// in CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// BrnSound::Logic::Passby::PassbyStateManager::UpdateDynamicPropBys.
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:272, li r5,0x110), liIndex <
// GetLength() (:274, li r5,0x112; length read at a1[2], lwz r11,8) and liIndex >= 0 (:275,
// li r5,0x113), then returns &mpEvents[liIndex] (result = (a2<<6) + *a1, slwi r11,r29,6). The
// 64-byte stride is sizeof(the queue element). Non-const GetEvent(int) overload (baked lines
// 272/274/275 == the ImpactEvent GetEvent sibling).
//
// The element type PassbyPropByEvent is HOMED in BrnPassbyPropByEvent.h
// (2026-08-25 wave 5; it used to be a namespace-scope class defined inside this
// .cpp -- an ODR hazard). It stays a FLAG'd 64-byte opaque placeholder there: the
// stride is X360-attested, the field layout is not (see the header's note).
#include "GameSource/Sound/Passby/BrnPassbyPropByEvent.h"

template BrnSound::Logic::Passby::PassbyPropByEvent&
CgsModule::BaseEventQueue<BrnSound::Logic::Passby::PassbyPropByEvent>::GetEvent(s32);
