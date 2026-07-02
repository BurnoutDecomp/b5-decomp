#include "GameSource/World/Bridges/WorldBridgePhysicsToScene.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT


// @ 0x827ABA40 -- append the physics module's staged scene-update events (read-locked
// getter @0x8279F838, the +179424 scene sub-interface) into the scene manager's
// update input buffer (write-locked getter @0x825BD8C0).
// Both null tripwires are NON-gating (the X360 falls through after firing); the
// X360 tail returns the forwarded call's result as a register artifact -- the
// logical return type is void.

namespace WorldModule
{
// @ 0x827ABA40
void BridgePhysicsSceneUpdateToScene(
    void* lpWorldModule,
    CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer_Update,
    const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpSceneInputBuffer_Update != 0, "lpSceneInputBuffer_Update != NULL");           // :103
    CGS_ASSERT(lpPhysicsModuleOutputBuffer != 0, "lpPhysicsModuleOutputBuffer != NULL");       // :104

    // FLAG: the physics buffer slice models its scene sub-interface as opaque storage
    // (its header's convention); the real type is the scene manager's
    // InSceneUpdateInterface -- adopt the typed member when the buffer's own TU lands.
    typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface InSceneUpdateInterface;
    lpSceneInputBuffer_Update->GetInSceneUpdateInterface()->Append(
        *reinterpret_cast<const InSceneUpdateInterface*>(
            lpPhysicsModuleOutputBuffer->GetSceneInputInterface()));
}
}
