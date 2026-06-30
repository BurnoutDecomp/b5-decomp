#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody (16-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody>::Append
//   @ X360 0x825A3988 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody>").
//
// The generic Append body is already inline in CgsBaseEventQueue.h; this is the thin explicit
// instantiation. The X360 body matches the generic store-for-store.
//
// Append (@0x825A3988) -- merges lSource onto the tail (asserts are non-gating tripwires):
//   * mpEvents != NULL (CgsBaseEventQueue.h:413, `lwz r11,0(r31)`; bne skips);
//   * no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow", `lwz r11,8(r30);
//     lwz r10,8(r31); lwz r9,4(r31); add r11,r11,r10; cmpw r11,r9`, ble skips);
//   * lSource.mpEvents != NULL (CgsBaseEventQueue.h:486 tripwire, `lwz r11,0(r30)`);
//   * XMemCpy(mpEvents + 16*miLength, lSource.mpEvents, 16*lSource.miLength) at a 16-byte stride
//     (`lwz r11,8(r31); slwi r11,r11,4` == miLength*16 dest offset, `add r3,r11,r10` dest;
//     `slwi r5,r29,4` == lSource.miLength*16 == XMemCpy Size; `lwz r4,0(r30)` == src);
//   * bumps miLength by lSource.miLength (`stw r11,8(r31)`); returns 1.
//
// The 16-byte stride is X360-attested off this very function (the slwi-by-4 on both the count and
// the dest offset) and cross-confirmed by the sibling AddEvent @0x825E3ED8 (slwi r11,r11,4 plus a
// pair of 64-bit element stores == 16 bytes). sizeof(InRemoveRigidBody) == 16 (see
// CgsPhysicsSimulationIO_Events.h). The AddEvent counterpart for this type (@0x825E3ED8) is the
// other function of this TU and is instantiated separately.
template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody>::Append(
    const CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody>&);
