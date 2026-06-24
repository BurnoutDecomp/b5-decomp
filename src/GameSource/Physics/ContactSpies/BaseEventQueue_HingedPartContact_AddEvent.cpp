#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                 // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"            // BrnPhysics::ContactSpy::HingedPartContact (112-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::HingedPartContact>::AddEvent @ 0x825E50B8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent body is
// already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. The X360 body
// matches the generic store-for-store (this is the UNCONDITIONAL AddEvent, not the bounds-gated
// AddEventSafe):
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:312 "mpEvents != NULL" tripwire,
//     `lwz r11,0(r29)`; bne skips the assert);
//   * non-gating overflow tripwire: `lwz r11,8(r29)` (miLength) vs `lwz r10,4(r29)` (miMaxLength),
//     `blt` skips the assert; on the >= path it builds the "Reached Max length" StrStream message
//     (CgsBaseEventQueue.h:313) and fires the assert but does NOT return -- it falls through and
//     appends anyway;
//   * copies the 112-byte element (ctr = 14 std/ld 64-bit block moves == 112 bytes) to
//     mpEvents[miLength] at a 112-byte stride (`mulli r10,r10,0x70`), bumps miLength
//     (`stw r11,8(r29)`) and returns 1.
// The 112-byte stride matches sizeof(HingedPartContact) == 112 (BaseContact(96) + EBodyParts
// meType, alignas(16) => 112). Called by BrnPhysics::Deformation::PhysicalBodyPart::AddContactSpy.
template bool
CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::HingedPartContact>::AddEvent(
    const BrnPhysics::ContactSpy::HingedPartContact&);
