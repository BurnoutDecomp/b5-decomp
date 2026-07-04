#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::InChangeRigidBodyInertia (80-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InChangeRigidBodyInertia>::Append
//   @ X360 0x825A40E8 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InChangeRigidBodyInertia>").
//
// The generic Append body is already inline in CgsBaseEventQueue.h; this is the thin explicit
// instantiation. The X360 body matches the generic store-for-store.
//
// Append (@0x825A40E8) -- merges lSource onto the tail (asserts are non-gating tripwires):
//   * mpEvents != NULL (CgsBaseEventQueue.h:413, `lwz r11,0(r31)`; bne skips);
//   * no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow", ble skips -- non-gating);
//   * lSource.mpEvents != NULL (CgsBaseEventQueue.h:486 tripwire, `lwz r11,0(r30)`);
//   * XMemCpy(mpEvents + 80*miLength, lSource.mpEvents, 80*lSource.miLength) at an 80-byte stride
//     (dest offset `slwi r8,r11,2; add` == miLength*5, `slwi r11,r11,4` == *16 == miLength*80;
//     count `slwi r9,r29,2; add` == count*5, `slwi r5,r9,4` == *16 == count*80). Hex-Rays:
//     XMemCpy(80 * a1[2] + *a1, *a2, 80 * v4);
//   * bumps miLength by lSource.miLength; returns 1.
//
// The 80-byte stride is X360-attested off this very function and pins
// sizeof(InChangeRigidBodyInertia) == 80 (see CgsPhysicsSimulationIO_Events.h, resized from 16
// to 80). Called from PhysicsSimulationIO::InputBuffer::AppendChangeRigidBodyInertiaQueue; the
// Construct instantiation for this element/capacity lives in EventQueue_InChangeRigidBodyInertia_200.cpp.
template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InChangeRigidBodyInertia>::Append(
    const CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InChangeRigidBodyInertia>&);
