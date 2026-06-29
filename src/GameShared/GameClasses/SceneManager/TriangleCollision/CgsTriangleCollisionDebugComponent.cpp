// CgsTriangleCollisionDebugComponent.cpp
//
// The triangle-collision debug component's recovered boot-trace function,
//   CgsSceneManager::TriangleCollisionDebugComponent::GetName @ 0x827DD828 -> "Collision",
// is a leaf that returns a literal and is homed inline on the class in the header (mirroring
// the committed PerfMon debug-name hooks). This translation unit exists to anchor that header
// against the canonical class declaration; the heavy component bodies (the per-test collision
// queries + Debug3DImmediate render path) are separate engine-gated TUs.
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionDebugComponent.h"

// Definitions for the file-static draw-collision-poly callback registry that backs the
// recovered accessor GetDrawCollisionPolyCallback @ 0x828AA458 (EXECUTED in the boot-trace
// milestone). On X360 these are unk_8307A880 (the slot array) and dword_830848A0 (the live
// count, siNumDrawPolyCallbacks); both live in BSS and start zeroed.
namespace CgsSceneManager
{
    DrawCollisionPolyCallback TriangleCollisionDebugComponent::siDrawPolyCallbacks[KI_MAX_NUM_COLLISION_POLY_CALLBACKS] = {};
    s32 TriangleCollisionDebugComponent::siNumDrawPolyCallbacks = 0;
}
