#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                    // CgsModule::BaseEventQueue<T>::AddEvent / Append (inline generic)
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"             // BrnPhysics::Props::RemovePhysicalPropEvent (8-byte element)

// =============================================================================
// CgsModule::BaseEventQueue<BrnPhysics::Props::RemovePhysicalPropEvent>::AddEvent @ 0x822C8620
// CgsModule::BaseEventQueue<BrnPhysics::Props::RemovePhysicalPropEvent>::Append  @ 0x827A8030
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent /
// Append bodies are already inline in CgsBaseEventQueue.h (assert machinery included);
// these are the thin explicit instantiations. Mirrors the committed sibling
// BaseEventQueue_InEventClearCullingTable_AddEventAppend.cpp (both members, one TU).
//
// AddEvent (0x822C8620) asserts mpEvents != NULL (CgsBaseEventQueue.h:312) and
// miLength < miMaxLength (:313, whose de-inlined StrStream
// "CgsModule::BaseEventQueue<class BrnPhysics::Props::RemovePhysicalPropEvent>::AddEvent\n
// Reached Max length " collapses to the generic CGS_ASSERT) -- both NON-gating tripwires --
// then appends the element UNCONDITIONALLY and bumps miLength. Element stride is 8 bytes:
// the asm does `slwi r11,miLength,3; add mpEvents; stw *a2 @+0; stw a2[1] @+4` -- two 4-byte
// stores copying the whole { PropEntityID mEntityId; s32 miPhysicalIndex; } record (sizeof == 8).
//
// Append (0x827A8030) asserts mpEvents != NULL (:413), no overflow (:414 "Base event queue
// overflow"), and lSource owns a buffer (:486 via GetQueueStartPointer) -- all NON-gating --
// then XMemCpy(dest = mpEvents + 8*miLength, src = lSource.mpEvents, count = 8*lSource.miLength)
// (`slwi ...,3` stride 8 on both count and dest offset), advances miLength by lSource.miLength,
// returns 1. Modelled by the generic std::memcpy.
//
// sizeof(BrnPhysics::Props::RemovePhysicalPropEvent) == 8, matching the 8-byte stride in both
// bodies and the two-word stores in AddEvent. Home struct lives in BrnPropEvents.h (default
// alignment -- NOT alignas(16), which would pad sizeof to 16 and break the slwi,3 stride).
// AddEvent is called from PropInputInterface::RemovePropInstance; Append from
// PropInputInterface::Append and WorldModule::BridgeEntityModulesToPhysicsModule_PrePhysics.
// DWARF (CgsBaseEventQueue.h:3597/3609) confirms both member signatures on
// BaseEventQueue<BrnPhysics::Props::RemovePhysicalPropEvent>; EventQueue<...,300> subclass
// homed at CgsEventQueue.h:1865.
// =============================================================================
template bool
CgsModule::BaseEventQueue<BrnPhysics::Props::RemovePhysicalPropEvent>::AddEvent(
    const BrnPhysics::Props::RemovePhysicalPropEvent&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Props::RemovePhysicalPropEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Props::RemovePhysicalPropEvent>&);
