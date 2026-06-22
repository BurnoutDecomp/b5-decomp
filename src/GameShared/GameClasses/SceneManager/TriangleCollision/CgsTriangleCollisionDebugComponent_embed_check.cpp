// Embed check for CgsTriangleCollisionDebugComponent.h: exercises the one ledger
// function homed in this pass.
//   CgsSceneManager::TriangleCollisionDebugComponent::GetName @ 0x827DD828 -> "Collision"
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionDebugComponent.h"

namespace
{
    const char* ExerciseTriCollisionDebugName()
    {
        CgsSceneManager::TriangleCollisionDebugComponent lComponent;
        return lComponent.GetName();
    }
}

extern const char* CgsTriangleCollisionDebugComponent_embed_check_anchor();
const char* CgsTriangleCollisionDebugComponent_embed_check_anchor()
{
    return ExerciseTriCollisionDebugName();
}
