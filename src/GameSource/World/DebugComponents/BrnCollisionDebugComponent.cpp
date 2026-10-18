#include "GameSource/World/DebugComponents/BrnCollisionDebugComponent.h"
#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.h"
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionDebugComponent.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameSource/AttribSys/Generated/classes/surface.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>

// BrnWorld::CollisionDebugComponent::GetTrafficColour @ 0x822A8C50.
//
// Maps a scalar (a traffic-density / angle value) onto an 8-step debug colour wheel and writes
// the packed colour word through the result pointer. The X360 scales the input by 4/pi
// (flt_820176A8 == 1.2732395) so a half-turn of angle spans the eight sectors, then walks a
// cascade of `fcmpu`/`bge` comparisons against the unit thresholds 1.0 .. 7.0 (the rodata
// floats flt_82001C98 / flt_82014984 / flt_82004270 / flt_82004EF4 / flt_820149B4 /
// flt_82014A54 / flt_820054D0), selecting one packed 0x20xxxxxx colour per sector:
//
//   scaled <  1.0 -> 0x200000FF   scaled <  5.0 -> 0x20009900
//   scaled <  2.0 -> 0x20000099   scaled <  6.0 -> 0x20FFFF00
//   scaled <  3.0 -> 0x2000FFFF   scaled <  7.0 -> 0x20FF0000
//   scaled <  4.0 -> 0x2000FF00   scaled >= 7.0 -> 0x20FF00FF
//
// Reproduced branch-for-branch / constant-for-constant from the asm. The X360 returns the
// colour through an out pointer (r3); this home returns it by value.

namespace BrnWorld
{
    const WorldEntityModule* CollisionDebugComponent::spWorldEntityModule = nullptr; // 82FAD2A0
    s32 CollisionDebugComponent::siDrawMode = E_DRAWMODE_SURFACES; // 82FAD30C
    bool CollisionDebugComponent::sbDrawTags = false;            // 82FAD310
    bool CollisionDebugComponent::sbDrawNormals = false;         // 82FAD311
    s32 CollisionDebugComponent::siSurfaceAlpha = 75;             // 82CDB59C

    // ARTIST 822F9F40. The extra null option terminates the native UI's string-list walk.
    void CollisionDebugComponent::Construct(const WorldEntityModule* lpWorldEntityModule)
    {
        sbDrawTags = false;
        CgsDev::DebugComponent::Construct();
        spWorldEntityModule = lpWorldEntityModule;
        CgsSceneManager::TriangleCollisionDebugComponent::RegisterDrawCollisionPolyCallback(
            &DrawCollisionPolyCallback, nullptr);
        const char* lapcNames[] = {"Surfaces", "Surface flags", "Collision zones",
                                  "Traffic colours", "AI Sections", "Is Drivable"};
        for (s32 li = 0; li < E_DRAWMODE_COUNT; ++li)
            maDrawModeList[li] = {li, lapcNames[li]};
        maDrawModeList[E_DRAWMODE_COUNT] = {0, nullptr};
    }

    // ARTIST 822A8B80.
    void CollisionDebugComponent::OnActivate()
    {
        RegisterVariable(&siDrawMode, "Draw mode");
        SetOptions(&siDrawMode, maDrawModeList);
        SetRange(&siDrawMode, 0, E_DRAWMODE_COUNT - 1);
        RegisterVariable(&sbDrawNormals, "Draw normals");
        RegisterVariable(&siSurfaceAlpha, "Surface alpha");
        SetRange(&siSurfaceAlpha, 0, 255);
        RegisterVariable(&sbDrawTags, "Draw tags");
    }

    u32 CollisionDebugComponent::GetTrafficColour(f32 lfValue)
    {
        // flt_820176A8 == 4/pi (the pseudocode-decoded 1.2732395f).
        const f32 lfScaled = lfValue * 1.2732395f;

        if (lfScaled < 1.0f) return 0x200000FFu;
        if (lfScaled < 2.0f) return 0x20000099u;
        if (lfScaled < 3.0f) return 0x2000FFFFu;
        if (lfScaled < 4.0f) return 0x2000FF00u;
        if (lfScaled < 5.0f) return 0x20009900u;
        if (lfScaled < 6.0f) return 0x20FFFF00u;
        if (lfScaled < 7.0f) return 0x20FF0000u;
        return 0x20FF00FFu;
    }

    // ARTIST 822A8D40: the overlapping driveable/fatal/superfatal flags have priority.
    u32 CollisionDebugComponent::GetFlagsRenderColour(CollisionTag lTag)
    {
        const u16 luMaterial = lTag.GetMaterialTag();
        const bool lbDriveable = (luMaterial & KU_COLLISION_FLAG_DRIVEABLE) != 0;
        const bool lbFatal = (luMaterial & KU_COLLISION_FLAG_FATAL) != 0;
        if (lbDriveable && !lbFatal) return 0x80ffffffu;
        if (!lbDriveable && !lbFatal) return 0x80000000u;
        if (luMaterial & KU_COLLISION_FLAG_SUPERFATAL) return 0xff00ffffu;
        if (!lbDriveable && lbFatal) return 0x800000ffu;
        return 0;
    }

    // ARTIST 822C3FB8. Truncate each 255*RGB component; packed debug colours are AABBGGRR.
    u32 CollisionDebugComponent::GetSurfaceRenderColour(u8 luSurfaceId)
    {
        Attrib::Gen::surface lSurface;
        spWorldEntityModule->GetSurface(luSurfaceId, &lSurface);
        const auto& lColour = lSurface.DebugRenderColor();
        CGS_ASSERT(lColour[0] >= 0.f && lColour[0] <= 1.f, "lColour.GetX() >= 0.0f && lColour.GetX() <= 1.0f");
        CGS_ASSERT(lColour[1] >= 0.f && lColour[1] <= 1.f, "lColour.GetY() >= 0.0f && lColour.GetY() <= 1.0f");
        CGS_ASSERT(lColour[2] >= 0.f && lColour[2] <= 1.f, "lColour.GetZ() >= 0.0f && lColour.GetZ() <= 1.0f");
        return (static_cast<u32>(siSurfaceAlpha) << 24) |
            (static_cast<u32>(lColour[2] * 255.f) << 16) |
            (static_cast<u32>(lColour[1] * 255.f) << 8) | static_cast<u32>(lColour[0] * 255.f);
    }

    // ARTIST 822D8C48; zone palette read from 82CDB660 in the decrypted ARTIST image.
    u32 CollisionDebugComponent::GetPolyColour(CollisionTag lTag, s32 liPolySoupIndex)
    {
        static const u32 KAU_ZONE_COLOURS[16] = {
            0x208080ff,0x2000ffff,0x2080ff80,0x20ff0000,0x20400080,0x20808040,0x20ffff00,0x200080ff,
            0x20ff0080,0x20c0c0c0,0x20008000,0x200000ff,0x20800040,0x20000000,0x20ffffff,0x20c08000};
        CGS_ASSERT(siDrawMode >= 0 && siDrawMode < E_DRAWMODE_COUNT,
                   "leDrawMode >= 0 && leDrawMode < E_DRAWMODE_COUNT");
        switch (siDrawMode)
        {
        case E_DRAWMODE_SURFACES:
            return lTag.GetSurfaceId() ? GetSurfaceRenderColour(lTag.GetSurfaceId()) : 0;
        case E_DRAWMODE_FLAGS: return GetFlagsRenderColour(lTag);
        case E_DRAWMODE_ZONES: return KAU_ZONE_COLOURS[liPolySoupIndex % 16];
        case E_DRAWMODE_TRAFFIC:
        {
            f32 lfAngle = 0.f;
            const TrafficDirection leTraffic = lTag.GetTrafficInfo(&lfAngle);
            if (leTraffic == E_TRAFFIC_DIRECTION_VALID) return GetTrafficColour(lfAngle);
            return leTraffic == E_TRAFFIC_DIRECTION_UNKNOWN ? 0x20ffffffu : 0x20000000u;
        }
        case E_DRAWMODE_AISECTION:
        {
            const u16 luSection = lTag.GetAISectionIndex();
            if (luSection == 0x7fff) return 0x200000ffu;
            const u32 KAU_SECTION_COLOURS[] = {0x20006400u,0x20009600u,0x2000c800u,0x2000ff00u};
            return KAU_SECTION_COLOURS[luSection % 4];
        }
        case E_DRAWMODE_DRIVABLE: return lTag.IsDrivable() ? 0x20006400u : 0x20000064u;
        default: CGS_ASSERT(false, "\nInvalid collision draw mode"); return 0;
        }
    }

    // ARTIST 822EE728: offset along the supplied normal, fill then white wireframe.
    void CollisionDebugComponent::DrawCollisionPolyCallback(CgsDev::Debug3DImmediateRender* lpDisplay,
        void*, const Vector3* lpaPolyPoints, s32 liNumPolyPoints, Vector3 lNormal,
        u32 luTag, s32 liPolySoupIndex)
    {
        CGS_ASSERT(lpDisplay != nullptr, "lpDisplay != NULL");
        CGS_ASSERT(lpaPolyPoints != nullptr, "lpaPolyPoints != NULL");
        CGS_ASSERT(liNumPolyPoints >= 2, "liNumPolyPoints >= 2");
        Vector3 laPoints[4];
        for (s32 li = 0; li < liNumPolyPoints && li < 4; ++li)
            laPoints[li] = lpaPolyPoints[li] + lNormal * .05f;
        CollisionTag lTag;
        // 822EE884/8A4 store high half at material +2, low half at group +0.
        lTag.Construct(static_cast<u16>(luTag), static_cast<u16>(luTag >> 16));
        const u32 luColour = GetPolyColour(lTag, liPolySoupIndex);
        const rw::RGBA lColour(static_cast<u8>(luColour),static_cast<u8>(luColour>>8),static_cast<u8>(luColour>>16),static_cast<u8>(luColour>>24));
        const rw::RGBA lWhite(255,255,255,255);
        Vector3 lCentre{0.f, 0.f, 0.f,0.f};
        if (liNumPolyPoints == 4)
        {
            if (siSurfaceAlpha)
                lpDisplay->DrawSolidQuad(laPoints[0], laPoints[1], laPoints[3], laPoints[2], lColour);
            lpDisplay->DrawQuad(laPoints[0], laPoints[1], laPoints[3], laPoints[2], lWhite);
            lCentre = (laPoints[0] + laPoints[1] + laPoints[2] + laPoints[3]) * .25f;
        }
        else if (liNumPolyPoints == 3)
        {
            if (siSurfaceAlpha) lpDisplay->DrawSolidTriangle(laPoints[0], laPoints[1], laPoints[2], lColour);
            lpDisplay->DrawHollowTriangle(laPoints[0], laPoints[1], laPoints[2], lWhite);
            lCentre = (laPoints[0] + laPoints[1] + laPoints[2]) * (1.f / 3.f);
        }
        else CGS_ASSERT(false, "Trying to render a polygon that isn't a triangle or quad");
        if (sbDrawNormals) lpDisplay->DrawArrow(lCentre, lCentre + lNormal, lWhite);
        if (sbDrawTags)
        {
            const Vector3 lPosition = (laPoints[0] + laPoints[1] + laPoints[2]) * .333f;
            const Vector3 lDelta = lPosition - lpDisplay->GetCameraPosition();
            if (lDelta.x*lDelta.x + lDelta.y*lDelta.y + lDelta.z*lDelta.z < 400.f)
            {
                char lacText[256];
                std::snprintf(lacText, sizeof(lacText), "( %u)", static_cast<u32>(lTag.GetAISectionIndex()));
                lpDisplay->DrawText(lPosition, lacText, 10.f);
            }
        }
    }
}
