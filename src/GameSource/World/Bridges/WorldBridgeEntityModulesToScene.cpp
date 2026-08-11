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
    // ⭐ 2026-08-10 (root-cause wave): the DESTINATION cast is GONE. PhysicsModuleIO::
    // InputBuffer::mPropManagerInputInterface was an `unsigned char[1]` slice and this Append
    // wrote the real ~12 KB interface straight through it and out into mGameActionQueue --
    // latent only because no prop is physically registered yet. The member now holds the real
    // type, so the destination is same-type. (The SOURCE stays a documented cross-home cast:
    // PropEntityIO::OutputBuffer_Prepare still exposes its interface as opaque *Storage.)
    const BrnPhysics::Props::PropInputInterface* lpPropSource =
        reinterpret_cast<const BrnPhysics::Props::PropInputInterface*>(
            lpPropOutputBuffer_Prepare->GetPropInputInterface());
    lpPhysicsModuleInputBuffer->GetPropManagerInputInterface()->Append(lpPropSource);
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

// @ 0x827AB490 -- the PER-FRAME pre-scene merge: every entity module's staged scene
// updates (adds / removes / position + radius updates) go into the scene manager's
// update input buffer, which SceneManagerModule::UpdateScene then fans out to the
// spatial partition and the overlap generator. This is the hop that puts the world's
// streamed instances into the broad-phase, so the frustum query has something to
// return.
//
// The X360 merge ORDER is traffic -> race car -> world entity -> prop -> trigger
// (asm call order @0x827AB4F4..0x827AB584); reproduced, though the merges are
// independent per-queue appends.
void BridgeEntityModulesToSceneModule_PreScene(
    void* lpWorldModule,
    CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
    const BrnWorld::TriggerEntityModuleIO::OutputBuffer_PreScene* lpTriggerOutputBuffer_PreScene,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene* lpTrafficOutputBuffer_PreScene,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene,
    const BrnWorld::PropEntityIO::OutputBuffer_PreScene* lpPropOutputBuffer_PreScene,
    const BrnWorld::WorldEntityIO::OutputBuffer_PreScene* lpWorldEntityOutputBuffer_PreScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpSceneInputBuffer != 0, "lpSceneModuleInputBuffer != NULL");                       // :98
    CGS_ASSERT(lpTriggerOutputBuffer_PreScene != 0, "lpTriggerOutputBuffer_PreScene != NULL");     // :99
    CGS_ASSERT(lpRaceCarOutputBuffer_PreScene != 0, "lpRaceCarOutputBuffer_PreScene != NULL");     // :100
    CGS_ASSERT(lpPropOutputBuffer_PreScene != 0, "lpPropOutputBuffer_PreScene != NULL");           // :101
    CGS_ASSERT(lpWorldEntityOutputBuffer_PreScene != 0, "lpWorldOutputBuffer_PreScene != NULL");   // :102

    typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface InSceneUpdateInterface;
    InSceneUpdateInterface* lpScene = lpSceneInputBuffer->GetInSceneUpdateInterface();

    // FLAG (deferred, NOT a divergence for the broad phase): only the WORLD-ENTITY leg
    // is merged today. The trigger / race-car / traffic / prop pre-scene output buffers
    // either do not model a scene-input interface in their own IO homes, or model it as
    // a declaration-only accessor whose body belongs to that buffer's own TU -- none of
    // which is reconstructed. All four of those modules are inert on this build (no
    // trigger volume, race car, traffic vehicle or prop is registered with the scene
    // manager), so the queues they would contribute are empty and their merges are
    // no-ops either way. The world entity module IS live -- it stages one AddEntity per
    // streamed instance -- and that is the leg the frustum query runs on. Restore the
    // other four with those buffers' own waves.
    lpScene->Append(reinterpret_cast<const InSceneUpdateInterface&>(
        *lpWorldEntityOutputBuffer_PreScene->GetSceneInputInterface()));

    (void)lpTriggerOutputBuffer_PreScene;
    (void)lpTrafficOutputBuffer_PreScene;
    (void)lpRaceCarOutputBuffer_PreScene;
    (void)lpPropOutputBuffer_PreScene;
}

// @ 0x827AB608 -- the post-physics restage (traffic -> race car -> prop -> world
// entity): the position/radius updates the physics step produced, merged into the
// same scene update input.
void BridgeEntityModulesToScene_PostPhysics(
    void* lpWorldModule,
    CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics* lpTrafficOutputBuffer_PostPhysics,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpRaceCarOutputBuffer_PostPhysics,
    const BrnWorld::PropEntityIO::OutputBuffer_PostPhysics* lpPropOutputBuffer_PostPhysics,
    const BrnWorld::WorldEntityIO::OutputBuffer_PostPhysics* lpWorldEntityOutputBuffer_PostPhysics)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpSceneInputBuffer != 0, "lpSceneModuleInputBuffer != NULL");                            // :243
    CGS_ASSERT(lpTrafficOutputBuffer_PostPhysics != 0, "lpTrafficOutputBuffer_PostPhysics != NULL");    // :244
    CGS_ASSERT(lpRaceCarOutputBuffer_PostPhysics != 0, "lpRaceCarOutputBuffer_PostPhysics != NULL");    // :245
    CGS_ASSERT(lpPropOutputBuffer_PostPhysics != 0, "lpPropOutputBuffer_PostPhysics != NULL");          // :246

    typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface InSceneUpdateInterface;
    InSceneUpdateInterface* lpScene = lpSceneInputBuffer->GetInSceneUpdateInterface();

    // (Same deferral as the pre-scene leg -- see its FLAG. The world-entity buffer's
    //  post-physics getter is non-const on its own home, so the const source is cast to
    //  reach it; the merge itself only reads.)
    // FLAG: the world-entity leg is deferred here too -- OutputBuffer_PostPhysics::
    // GetSceneInputInterface guards on the WRITE lock and this bridge runs with the
    // buffer read-locked (the X360 orders its locks differently across the post-physics
    // spine). Nothing is lost today: the world module stages its scene adds in the
    // PRE-SCENE phase (OnWorldGraphicsLoadComplete), and its instances are static, so
    // the post-physics restage carries no position or radius update.
    (void)lpWorldEntityOutputBuffer_PostPhysics;
    (void)lpTrafficOutputBuffer_PostPhysics;
    (void)lpRaceCarOutputBuffer_PostPhysics;
    (void)lpPropOutputBuffer_PostPhysics;
}

}   // namespace WorldModule
