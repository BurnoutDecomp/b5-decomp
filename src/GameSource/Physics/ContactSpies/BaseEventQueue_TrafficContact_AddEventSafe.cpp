#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                 // BaseEventQueue<T>::AddEventSafe (inline generic)
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"            // BrnPhysics::ContactSpy::TrafficContact (96-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::TrafficContact>::AddEventSafe @ 0x825A33F8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEventSafe body
// is already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. The X360
// body matches the generic store-for-store:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h, the "mpEvents != NULL" tripwire at :331);
//   * bounds-gated full check: `lwz r11,8(r31)` (miLength) vs `lwz r10,4(r31)` (miMaxLength),
//     `bge` => return 0 WITHOUT appending when miLength >= miMaxLength;
//   * otherwise copies the 96-byte element (ctr = 12 std/ld 64-bit block moves) to
//     mpEvents[miLength] at a 96-byte stride (`v4*3*32`, `slwi r11,r11,5`), bumps miLength
//     (`stw r11,8(r31)`) and returns 1.
// The 96-byte stride matches sizeof(TrafficContact) == 96 (BaseContact layout: EntityId@0 +
// EntityId@4 + CollisionTag@8, then five 16-byte Vector3s @16..80, alignas(16)).
template bool
CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::TrafficContact>::AddEventSafe(
    const BrnPhysics::ContactSpy::TrafficContact&);
