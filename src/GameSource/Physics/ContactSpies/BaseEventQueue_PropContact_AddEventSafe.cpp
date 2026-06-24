#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                 // BaseEventQueue<T>::AddEventSafe (inline generic)
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"            // BrnPhysics::ContactSpy::PropContact (112-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::PropContact>::AddEventSafe @ 0x825A3570
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEventSafe body is
// already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. The X360 body
// matches the generic store-for-store:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:331 "mpEvents != NULL" tripwire,
//     `lwz r11,0(r31)`; bne skips the assert);
//   * bounds-gated full check: `lwz r11,8(r31)` (miLength) vs `lwz r10,4(r31)` (miMaxLength),
//     `bge` => return 0 WITHOUT appending when miLength >= miMaxLength;
//   * otherwise copies the 112-byte element (ctr = 14 std/ld 64-bit block moves == 112 bytes) to
//     mpEvents[miLength] at a 112-byte stride (`mulli r11,r11,0x70` == miLength*112), bumps
//     miLength (`stw r11,8(r31)`) and returns 1.
// The 112-byte stride matches sizeof(PropContact) == 112 (BaseContact(96) + u16 muType@96 +
// u8 muState@98 + u8 muFlags@99 + u8 muBeganMoving@100, alignas(16) => 112).
// Called by sub_825A5340.
template bool
CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::PropContact>::AddEventSafe(
    const BrnPhysics::ContactSpy::PropContact&);
