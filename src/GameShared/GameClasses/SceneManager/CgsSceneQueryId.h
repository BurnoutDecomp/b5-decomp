#pragma once

// CgsSceneManager::SceneQueryId — a packed (owner, index) handle for an in-flight
// scene query. Reconstructed from the DecFIGS DWARF; minimal (the storage word).
#include "types.hpp"

namespace CgsSceneManager
{
    struct SceneQueryId
    {
        u32 mId;
    };
}
