#pragma once

// CgsSceneManager::VolumeInstanceId — a packed 64-bit handle (entity id + reserved
// bits + volume index) identifying a collision volume instance. Reconstructed from
// the DecFIGS DWARF; minimal recon (the storage word — accessors are inlined / own TUs).
#include "types.hpp"

namespace CgsSceneManager
{
    struct VolumeInstanceId
    {
        u64 muId;
    };
}
