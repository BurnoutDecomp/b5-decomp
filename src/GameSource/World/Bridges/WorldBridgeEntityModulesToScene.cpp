#include "GameSource/World/Bridges/WorldBridgeEntityModulesToScene.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"        // BrnPhysics::Props::PropInputInterface (Append)

// WorldModule entity-modules -> scene/physics prepare-phase bridges, reconstructed store-for-store
// from BURNOUT_X360_ARTIST.XEX.
//
// The null tripwires are NON-gating (the X360 falls through after firing the assert). The
// X360-baked d:\p4 file/line pairs are intentionally not reproduced -- CGS_ASSERT stamps
// __FILE__/__LINE__; the X360 source line is noted in a trailing comment. Every function
// tail-forwards the merge call's result, but these bridges are logically void (the returned
// register is an artifact of the tail branch).
//
// FLAG cross-home casts: the source module's prepare output interface and the destination
// buffer's input-interface member model the SAME X360 payload but each is exposed as opaque
// *Storage by its own home; the merge is driven through the named aggregate (PropInputInterface
// / InSceneUpdateInterface), reinterpret_cast from the getter result -- the documented adapter
// used by the sibling bridges (WorldBridgePhysicsToScene.cpp / WorldBridgeEntityModulesToEntityModules.cpp).

namespace WorldModule
{

// @ 0x827AB410
void BridgePropModuleToPhysicsModule_Prepare(
    void* lpWorldModule,
    BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
    const BrnWorld::PropEntityIO::OutputBuffer_Prepare* lpPropOutputBuffer_Prepare)
{
    (void)lpWorldModule;

    // The X360 checks only the prop output buffer, firing the same tripwire twice (the
    // compiler CSE'd two identical consecutive null-asserts into a single branch on a3).
    CGS_ASSERT(lpPropOutputBuffer_Prepare != 0, "lpPropOutputBuffer_Prepare != NULL");   // :76
    CGS_ASSERT(lpPropOutputBuffer_Prepare != 0, "lpPropOutputBuffer_Prepare != NULL");   // :77

    // asm order: fetch the prop output's prop-input interface first, then the physics
    // prop-manager's, then merge source into destination.
    const BrnPhysics::Props::PropInputInterface& lrPropSource =
        reinterpret_cast<const BrnPhysics::Props::PropInputInterface&>(
            *lpPropOutputBuffer_Prepare->GetPropInputInterface());
    BrnPhysics::Props::PropInputInterface* lpPropManagerInputInterface =
        reinterpret_cast<BrnPhysics::Props::PropInputInterface*>(
            lpPhysicsModuleInputBuffer->GetPropManagerInputInterface());
    lpPropManagerInputInterface->Append(lrPropSource);
}

// @ 0x827AB388
void BridgePropModuleToSceneModule_Prepare(
    void* lpWorldModule,
    CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
    const BrnWorld::PropEntityIO::OutputBuffer_Prepare* lpPropOutputBuffer_Prepare)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpSceneInputBuffer != 0, "lpSceneInputBuffer != NULL");                   // :58
    CGS_ASSERT(lpPropOutputBuffer_Prepare != 0, "lpPropOutputBuffer_Prepare != NULL");   // :59

    typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface InSceneUpdateInterface;

    // asm order: fetch the prop output's scene-input interface first, then the scene manager's
    // in-scene-update aggregate, then merge source into destination.
    const InSceneUpdateInterface& lrSceneSource =
        reinterpret_cast<const InSceneUpdateInterface&>(
            *lpPropOutputBuffer_Prepare->GetSceneInputInterface());
    lpSceneInputBuffer->GetInSceneUpdateInterface()->Append(lrSceneSource);
}

// @ 0x827AB300
void BridgeTrafficModuleToSceneModule_Prepare(
    void* lpWorldModule,
    CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_Prepare* lpTrafficOutputBuffer_Prepare)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpSceneInputBuffer != 0, "lpSceneInputBuffer != NULL");                       // :40
    CGS_ASSERT(lpTrafficOutputBuffer_Prepare != 0, "lpTrafficOutputBuffer_Prepare != NULL"); // :41

    typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface InSceneUpdateInterface;

    // asm order: fetch the traffic output's scene-input interface first, then the scene manager's
    // in-scene-update aggregate, then merge source into destination.
    const InSceneUpdateInterface& lrSceneSource =
        reinterpret_cast<const InSceneUpdateInterface&>(
            *lpTrafficOutputBuffer_Prepare->GetSceneInputInterface());
    lpSceneInputBuffer->GetInSceneUpdateInterface()->Append(lrSceneSource);
}

}   // namespace WorldModule
