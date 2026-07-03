#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"          // BaseEventQueue<T>::Append (inline generic)
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"      // BrnPhysics::Props::UpdatePropEvent (112-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Props::UpdatePropEvent>::Append
//   @ X360 0x825E61F0 (dossier id "class:BrnPhysics::Props::UpdatePropEvent>").
//
// The generic Append body is already inline in CgsBaseEventQueue.h; this is the thin explicit
// instantiation. The X360 body matches the generic store-for-store.
//
// Append (@0x825E61F0) -- merges lSource onto the tail (asserts are non-gating tripwires):
//   * mpEvents != NULL (CgsBaseEventQueue.h:413, `lwz r11,0(r31)`; bne skips);
//   * no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow", `lwz r11,8(r30);
//     lwz r10,8(r31); lwz r9,4(r31); add r11,r11,r10; cmpw r11,r9`, ble skips);
//   * lSource.mpEvents != NULL (CgsBaseEventQueue.h:486 tripwire, `lwz r11,0(r30)`);
//   * XMemCpy(mpEvents + 112*miLength, lSource.mpEvents, 112*lSource.miLength) at a 112-byte stride
//     (`lwz r11,8(r31); mulli r11,r11,0x70` == miLength*112 dest offset, `add r3,r11,r10` dest;
//     `mulli r5,r29,0x70` == lSource.miLength*112 == XMemCpy Size; `lwz r4,0(r30)` == src);
//   * bumps miLength by lSource.miLength (`add r11,r11,r10`/`stw r11,8(r31)`); returns 1.
//
// The 112-byte stride is X360-attested off this very function (`mulli ...,0x70` on both the count
// and the dest offset) and cross-confirmed by the sibling AddEvent @0x825E5DF8 (mulli 0x70 plus the
// six 16-byte vector element stores + tail scalars == 112 bytes). sizeof(UpdatePropEvent) == 112
// (see BrnPropEvents.h). The AddEvent counterpart for this type (@0x825E5DF8) is the other function
// of this TU and is instantiated separately.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Props::UpdatePropEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Props::UpdatePropEvent>&);
