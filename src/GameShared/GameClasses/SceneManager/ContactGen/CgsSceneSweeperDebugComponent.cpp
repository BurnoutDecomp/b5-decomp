#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DBA60
//   (CgsSceneManager::SceneSweeperDebugComponent::GetName)  ->  return "Scene sweeper";

namespace CgsSceneManager
{
    struct SceneSweeperDebugComponent
    {
        const char* GetName();
    };

    const char* SceneSweeperDebugComponent::GetName()
    {
        return "Scene sweeper";
    }
}
