#pragma once

// CgsSceneManager::OverlappingIntervalPair — a broadphase overlap between two object
// intervals (their indices). Reconstructed from the DecFIGS DWARF.
#include "types.hpp"

namespace CgsSceneManager
{
    struct OverlappingIntervalPair
    {
        u16 muObjectIndexA;
        u16 muObjectIndexB;
    };
}
