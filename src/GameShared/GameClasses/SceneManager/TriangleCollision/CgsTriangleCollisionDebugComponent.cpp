// CgsTriangleCollisionDebugComponent.cpp
//
// The triangle-collision debug component's recovered boot-trace function,
//   CgsSceneManager::TriangleCollisionDebugComponent::GetName @ 0x827DD828 -> "Collision",
// is a leaf that returns a literal and is homed inline on the class in the header (mirroring
// the committed PerfMon debug-name hooks). This translation unit exists to anchor that header
// against the canonical class declaration; the heavy component bodies (the per-test collision
// queries + Debug3DImmediate render path) are separate engine-gated TUs.
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionDebugComponent.h"
