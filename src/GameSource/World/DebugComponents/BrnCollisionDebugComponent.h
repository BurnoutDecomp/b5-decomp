#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "SharedClasses/World/BrnCollisionTag.h"

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
        enum EDrawMode
        {
            E_DRAWMODE_SURFACES, E_DRAWMODE_FLAGS, E_DRAWMODE_ZONES,
            E_DRAWMODE_TRAFFIC, E_DRAWMODE_AISECTION, E_DRAWMODE_DRIVABLE,
            E_DRAWMODE_COUNT
        };

        void Construct(const WorldEntityModule* lpWorldEntityModule);

        const char* GetName() const { return "Collision draw mode"; }
        const char* GetPath() const { return "World"; }

        // 0x822A8C50 -- map a traffic-density/angle scalar to a packed 0xAARRGGBB-style debug
        // colour word (the X360 stores it through the result pointer; returned here by value).
        // Defined in BrnCollisionDebugComponent.cpp.
        static u32 GetTrafficColour(f32 lfValue);
        static u32 GetFlagsRenderColour(CollisionTag lTag);
        static u32 GetSurfaceRenderColour(u8 luSurfaceId);
        static u32 GetPolyColour(CollisionTag lTag, s32 liPolySoupIndex);
        static void DrawCollisionPolyCallback(CgsDev::Debug3DImmediateRender* lpDisplay,
            void* lpUserData, const Vector3* lpaPolyPoints, s32 liNumPolyPoints,
            Vector3 lNormal, u32 luTag, s32 liPolySoupIndex);

    private:
        void OnActivate() override;
        CgsDev::DebugUI::StringList maDrawModeList[E_DRAWMODE_COUNT + 1];
        static const WorldEntityModule* spWorldEntityModule;
        static s32 siDrawMode;
        static bool sbDrawNormals;
        static bool sbDrawTags;
        static s32 siSurfaceAlpha;
    };
}
