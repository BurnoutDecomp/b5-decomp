#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"          // BaseEventQueue<T>::AddEventSafe (inline generic)
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"       // BrnPhysics::ContactSpy::DiscardedContact (64-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::DiscardedContact>::AddEventSafe @ 0x825A3628
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEventSafe body is
// already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. The X360 body
// matches the generic store-for-store:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h "mpEvents != NULL" tripwire,
//     `lwz r11,0(r31)`; bne skips the assert);
//   * bounds-gated full check: `lwz r11,8(r31)` (miLength) vs `lwz r10,4(r31)` (miMaxLength),
//     `bge` => return 0 WITHOUT appending when miLength >= miMaxLength;
//   * otherwise copies the 64-byte element (ctr = 8 ld/std 64-bit block moves == 64 bytes) to
//     mpEvents[miLength] at a 64-byte stride (`slwi r10,r11,6` == miLength*64), bumps miLength
//     (`stw r11,8(r31)`) and returns 1.
// The 64-byte stride matches sizeof(DiscardedContact) == 64 (EntityId@0 + EntityId@4 +
// f32 mfClosingVelocity@8, then three 16-byte Vector3s mNormal@16/mPointOnA@32/mPointOnB@48,
// alignas(16) => 64). Called by BrnPhysics::ContactSpy::ContactSpyData::AddContact(const
// DiscardedContact&) via BridgeSimulationToOutput.
template bool
CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::DiscardedContact>::AddEventSafe(
    const BrnPhysics::ContactSpy::DiscardedContact&);
