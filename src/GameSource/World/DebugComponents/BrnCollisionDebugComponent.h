#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct @ 0x822F9F40, OnActivate @ 0x822A8B80, GetPolyColour @ 0x822D8C48,
//   GetSurfaceRenderColour @ 0x822C3FB8, GetFlagsRenderColour @ 0x822A8D40,
//   GetTrafficColour @ 0x822A8C50, DrawCollisionPolyCallback @ 0x822EE728.
// The component registers the polygon callback with the shared triangle-collision registry.
// Its menu name/path are "Collision draw mode" / "World".

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

namespace BrnWorld
{
    class WorldEntityModule;
    struct CollisionTag;

    class CollisionDebugComponent : public CgsDev::DebugComponent
    {
    public:
        enum EDrawMode
        {
            E_DRAWMODE_SURFACES = 0,
            E_DRAWMODE_FLAGS = 1,
            E_DRAWMODE_ZONES = 2,
            E_DRAWMODE_TRAFFIC = 3,
            E_DRAWMODE_AISECTION = 4,
            E_DRAWMODE_DRIVABLE = 5,
            E_DRAWMODE_COUNT = 6,
        };

        // @0x822F9F40. Initializes the option list, records the module for the static
        // surface-colour helper, and registers the polygon callback.
        void Construct(const WorldEntityModule* lpWorldEntityModule);

        const char* GetName() const  { return "Collision draw mode"; }
        const char* GetPath() const  { return "World"; }

        // Static callback registered with the shared triangle-collision component by
        // Construct. The callback type and argument order are from DecFIGS, with ARTIST
        // assembly confirming the actual X360 register use.
        static void DrawCollisionPolyCallback(
            CgsDev::Debug3DImmediateRender* lpDisplay,
            void* lpUserData,
            const Vector3* lpaPolyPoints,
            s32 liNumPolyPoints,
            Vector3 lNormal,
            u32 luCollisionTag,
            s32 liPolyIndex);

    protected:
        virtual void OnActivate() ;

    private:
        static u32 GetPolyColour(CollisionTag lCollisionTag, s32 liPolyIndex);
        static u32 GetTrafficColour(f32 lfValue);
        static u32 GetSurfaceRenderColour(u8 luSurfaceId);
        static u32 GetFlagsRenderColour(CollisionTag lCollisionTag);

        CgsDev::DebugUI::StringList maDrawModeList[6];
    };
}
