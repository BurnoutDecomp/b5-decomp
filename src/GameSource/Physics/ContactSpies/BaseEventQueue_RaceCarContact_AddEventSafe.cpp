#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                 // BaseEventQueue<T>::AddEventSafe (inline generic)
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"            // BrnPhysics::ContactSpy::RaceCarContact (96-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::RaceCarContact>::AddEventSafe @ 0x825A3338
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEventSafe body is
// already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. The X360 body
// matches the generic store-for-store:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:331 "mpEvents != NULL" tripwire,
//     `lwz r11,0(r31)`; bne skips the assert);
//   * bounds-gated full check: `lwz r11,8(r31)` (miLength) vs `lwz r10,4(r31)` (miMaxLength),
//     `bge` => return 0 WITHOUT appending when miLength >= miMaxLength;
//   * otherwise copies the 96-byte element (ctr = 12 std/ld 64-bit block moves == 96 bytes) to
//     mpEvents[miLength] at a 96-byte stride (`slwi r9,r11,1; add r11,r11,r9; slwi r11,r11,5`
//     == miLength*3*32 == miLength*96), bumps miLength (`stw r11,8(r31)`) and returns 1.
// The 96-byte stride matches sizeof(RaceCarContact) == 96 (RaceCarContact adds no members over
// BaseContact: EntityId@0 + EntityId@4 + CollisionTag@8, then five 16-byte Vector3s @16..80,
// alignas(16) => 96). Called by BrnPhysics::ContactSpy::ContactSpyData::AddContact.
template bool
CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::RaceCarContact>::AddEventSafe(
    const BrnPhysics::ContactSpy::RaceCarContact&);
