#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DD828
//   (CgsSceneManager::TriangleCollisionDebugComponent::GetName)  ->  return "Collision";

namespace CgsSceneManager
{
    struct TriangleCollisionDebugComponent
    {
        const char* GetName();
    };

    const char* TriangleCollisionDebugComponent::GetName()
    {
        return "Collision";
    }
}
