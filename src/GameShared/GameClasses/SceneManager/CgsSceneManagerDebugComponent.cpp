#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DD260
//   (CgsSceneManager::SceneManagerDebugComponent::GetName)  ->  return "Scene manager";

namespace CgsSceneManager
{
    struct SceneManagerDebugComponent
    {
        const char* GetName();
    };

    const char* SceneManagerDebugComponent::GetName()
    {
        return "Scene manager";
    }
}
