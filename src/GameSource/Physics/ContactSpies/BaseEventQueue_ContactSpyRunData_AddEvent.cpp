#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                 // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameSource/Physics/ContactSpies/BrnContactSpyRunList.h"            // BrnPhysics::ContactSpy::ContactSpyRunData (80-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::ContactSpyRunData>::AddEvent @ X360 0x8259E738
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent body is
// already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. The X360 body
// matches the generic store-for-store (this is the UNCONDITIONAL AddEvent, not AddEventSafe):
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:312, `lwz r11,0(r29)`; bne skips the assert);
//   * non-gating overflow tripwire: `lwz r11,8(r29)` (miLength) vs `lwz r10,4(r29)` (miMaxLength),
//     `blt` skips; on the >= path it builds the de-inlined StrStream "Reached Max length "
//     message and fires the assert (CgsBaseEventQueue.h:313) but does NOT return -- it falls
//     through and appends anyway;
//   * copies the 80-byte element (ctr = 10 ld/std 64-bit block moves == 80 bytes) to
//     mpEvents[miLength] at an 80-byte stride (`slwi r7,r11,2; add r11,r11,r7; slwi r11,r11,4`
//     == miLength*5*16 == miLength*80), bumps miLength (`stw r11,8(r29)`) and returns 1.
// The 80-byte stride matches sizeof(ContactSpyRunData) == 80 (EntityId@0, three Vector3
// @0x10/0x20/0x30, s32 miStartIndex@0x40, s32 miRunLength@0x44, alignas(16) => 80). Called by
// ContactSpyQueue<T,N>::SortAndCreateRunList (RaceCar/Traffic/PhysicalCarPart/Prop specialisations).
static_assert(sizeof(BrnPhysics::ContactSpy::ContactSpyRunData) == 80,
              "ContactSpyRunData stride 80");

template bool
CgsModule::BaseEventQueue<BrnPhysics::ContactSpy::ContactSpyRunData>::AddEvent(
    const BrnPhysics::ContactSpy::ContactSpyRunData&);
