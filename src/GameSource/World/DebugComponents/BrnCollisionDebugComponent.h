#pragma once

#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::CollisionDebugComponent::GetName @ 0x827DB658  -> "Collision draw mode"
//   BrnWorld::CollisionDebugComponent::GetPath @ 0x827DD250  -> "World"
//
// Debug-UI identity hooks for the collision debug component. The X360 build (the
// spine) returns "Collision draw mode" for GetName (the Feb-2007 leak header read
// "Collision Tags" - a different build). Header-keyed TU; members inline.

namespace BrnWorld
{
    class CollisionDebugComponent
    {
    public:
        const char* GetName() const { return "Collision draw mode"; }
        const char* GetPath() const { return "World"; }
    };
}
