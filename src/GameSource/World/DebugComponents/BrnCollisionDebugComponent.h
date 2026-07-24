#pragma once

#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::CollisionDebugComponent::GetName          @ 0x827DB658  -> "Collision draw mode"
//   BrnWorld::CollisionDebugComponent::GetPath          @ 0x827DD250  -> "World"
//   BrnWorld::CollisionDebugComponent::GetTrafficColour @ 0x822A8C50  (defined in the .cpp)
//
// Debug-UI identity hooks for the collision debug component. The X360 build (the spine)
// returns "Collision draw mode" for GetName (a partial-leak header read the older string
// "Collision Tags" from a different build). Header-keyed TU; identity accessors inline.

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

namespace BrnWorld
{
    class WorldEntityModule;

    // ADDITIVE GROW (WorldEntityModule::Construct @0x82302398 mounts + registers this
    // component like its PVS sibling): derive the CgsDev::DebugComponent registration
    // base and carry the owning-module back-pointer.
    class CollisionDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct( WorldEntityModule* lpWorldEntityModule )
        {
            mpWorldEntityModule = lpWorldEntityModule;
        }

        const char* GetName() const { return "Collision draw mode"; }
        const char* GetPath() const { return "World"; }

        // 0x822A8C50 -- map a traffic-density/angle scalar to a packed 0xAARRGGBB-style debug
        // colour word (the X360 stores it through the result pointer; returned here by value).
        // Defined in BrnCollisionDebugComponent.cpp.
        u32 GetTrafficColour(f32 lfValue) const;

    private:
        WorldEntityModule* mpWorldEntityModule;
    };
}
