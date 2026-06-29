#pragma once

// ===========================================================================
// CgsSceneManager::TriangleCollisionManager
//   Home: GameShared/GameClasses/SceneManager/TriangleCollision/
//         CgsTriangleCollisionManager.{h,cpp}
//
// Owns the triangle-collision scene (a PolygonSoupListSpatialMap of the static
// world collision) plus its debug component, and answers the SceneManager's
// triangle line/sphere/volume tests. Embedded BY VALUE in
// CgsSceneManager::SceneManagerModule (mTriangleCollisionManager).
//
// In the X360 SceneManagerModule::Construct the embedded sub-objects are built
// in order: a PolygonSoupListSpatialMap (its own type) then a
// TriangleCollisionDebugComponent (CgsTriangleCollisionDebugComponent.h). The
// map storage is large and owned by those TUs; this OWNING header models the
// Prepare() surface the SceneManagerModule needs and reserves the bulk as a
// documented opaque buffer (byte offsets are not preserved on the x64 PC compile
// -- semantic-parity-by-name, per the project rule).
//
// X360 function this TU (CgsSceneManagerModule.cpp) calls:
//   TriangleCollisionManager::Prepare  @ 0x828B2FF0
// ===========================================================================

#include "types.hpp"

namespace CgsMemory { class LinearMalloc; }

namespace CgsSceneManager
{
    class TriangleCollisionManager
    {
    public:
        // @ 0x828B2FF0 -- prepare the collision scene from a linear allocator with
        // a fixed object budget (the X360 passes 512). Returns success.
        bool Prepare(CgsMemory::LinearMalloc* lpAllocator, s32 liMaxObjects);

    private:
        // Embedded sub-objects (PolygonSoupListSpatialMap + the debug component)
        // and per-frame scratch; owned by this manager's own TUs.
        u8 maEmbeddedState[528];
    };
}
