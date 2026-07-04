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
// FLAG (LOW CONFIDENCE -- element TYPE NOT ATTESTED): the DWARF for the caller
// PassbyStateManager::UpdateDynamicPropBys (BrnPassbyStateManager.cpp:265) opens `using namespace
// BrnPhysics::Props;` but does NOT expose which queue this indexes. The manager's own
// DynamicPropByCache::Item is only 12 bytes (bool+float+EntityId) and is a plain array, NOT this
// 64-byte BaseEventQueue element -- so the element is a DISTINCT type, most plausibly a
// BrnPhysics::Props event (BrnPropEvents.h family) but NONE is confirmed at a 64-byte stride.
// A minimal placeholder is defined locally so this thin instantiation is a well-formed compilable
// unit. Consolidator: replace `PassbyPropByEvent` with the real DWARF-named 64-byte type (and drop
// this placeholder) once it is disassembled from UpdateDynamicPropBys.
namespace BrnSound { namespace Logic { namespace Passby {
    // PLACEHOLDER -- 64-byte opaque payload (X360-attested stride only; field layout unattested).
    struct alignas(16) PassbyPropByEvent { u8 macOpaquePayload[64]; };
}}}

template BrnSound::Logic::Passby::PassbyPropByEvent&
CgsModule::BaseEventQueue<BrnSound::Logic::Passby::PassbyPropByEvent>::GetEvent(s32);
