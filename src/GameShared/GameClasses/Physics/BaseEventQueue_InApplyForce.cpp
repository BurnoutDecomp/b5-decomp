#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::AddEvent / AddEventSafe (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::InApplyForce (32-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InApplyForce>::{AddEvent,AddEventSafe}
//   AddEvent @ X360 0x825E3CC8, AddEventSafe @ X360 0x825E3E20
//   (dossier id "class:CgsPhysics::PhysicsSimulationIO::InApplyForce>", funcs: 2).
//
// The generic AddEvent / AddEventSafe bodies are already inline in CgsBaseEventQueue.h; these are
// the thin explicit instantiations. Both X360 bodies match the generic store-for-store.
//
// AddEvent (@0x825E3CC8) -- appends UNCONDITIONALLY (asserts are non-gating tripwires):
//   * mpEvents != NULL  (CgsBaseEventQueue.h:312, `lwz r11,0(r29)`; bne skips);
//   * miLength < miMaxLength (CgsBaseEventQueue.h:313 "Reached Max length", `lwz r11,8(r29)` vs
//     `lwz r10,4(r29)`, blt skips) -- tripwire only; the copy below always runs;
//   * copies the 32-byte element to mpEvents[miLength] at a 32-byte stride (`slwi r11,r11,5`
//     == miLength*32, `add r11,r11,r10` == + mpEvents) as four 64-bit block moves
//     (ld/std x4 == 32 bytes); bumps miLength (`stw r11,8(r29)`); returns 1.
//
// AddEventSafe (@0x825E3E20) -- bounds-gated append:
//   * mpEvents != NULL (CgsBaseEventQueue.h:331, `lwz r11,0(r31)`; bne skips);
//   * `lwz r11,8(r31)` (miLength) vs `lwz r10,4(r31)` (miMaxLength), `bge` => return 0 WITHOUT
//     appending when miLength >= miMaxLength;
//   * otherwise copies the same 32-byte element at the same 32-byte stride
//     (`slwi r11,r11,5`; four ld/std), bumps miLength and returns 1.
//
// The 32-byte stride matches sizeof(InApplyForce) == 32 (X360-attested off this very pair, see
// CgsPhysicsSimulationIO_Events.h). This is the AddEvent/AddEventSafe pair of the
// EventQueue<InApplyForce,250> instance whose Construct lives in EventQueue_InApplyForce_250.cpp.
// AddEvent is called from BrnPhysics::Props::PropManager::ReadUpdatedBodies; AddEventSafe from
// BrnPhysics::Props::PropManager::ApplyAntiHerdingForce.
template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InApplyForce>::AddEvent(
    const CgsPhysics::PhysicsSimulationIO::InApplyForce&);

template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InApplyForce>::AddEventSafe(
    const CgsPhysics::PhysicsSimulationIO::InApplyForce&);
