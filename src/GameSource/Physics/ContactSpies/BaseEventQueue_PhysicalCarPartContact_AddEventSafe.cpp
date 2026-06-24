#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                 // BaseEventQueue<T>::AddEventSafe (inline generic)
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"            // BrnPhysics::ContactSpy::PhysicalCarPartContact (128-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::PhysicalCarPartContact>::AddEventSafe @ 0x825A34B8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEventSafe body is
// already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. The X360 body
// matches the generic store-for-store:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:331 "mpEvents != NULL" tripwire,
//     `lwz r11,0(r31)`; bne skips the assert);
//   * bounds-gated full check: `lwz r11,8(r31)` (miLength) vs `lwz r10,4(r31)` (miMaxLength),
//     `bge` => return 0 WITHOUT appending when miLength >= miMaxLength;
//   * otherwise copies the 128-byte element (ctr = 16 std/ld 64-bit block moves == 128 bytes) to
//     mpEvents[miLength] at a 128-byte stride (`slwi r10,r11,7` == miLength*128), bumps miLength
//     (`stw r11,8(r31)`) and returns 1.
// The 128-byte stride matches sizeof(PhysicalCarPartContact) == 128 (BaseContact(96) + Vector3
// mVelocity@96 + EBodyParts meType@112 + bool mbIsHinged@116, alignas(16) => 128).
// Called by sub_825A52E0.
template bool
CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::PhysicalCarPartContact>::AddEventSafe(
    const BrnPhysics::ContactSpy::PhysicalCarPartContact&);
