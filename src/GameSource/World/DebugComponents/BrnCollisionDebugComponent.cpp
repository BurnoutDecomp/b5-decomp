#include "GameSource/World/DebugComponents/BrnCollisionDebugComponent.h"

#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionDebugComponent.h"
#include "SharedClasses/World/BrnCollisionTag.h"
#include "GameSource/AttribSys/Generated/classes/surface.h"

#include <math.h>

namespace BrnWorld
{
namespace
{
    s32 siDrawMode = 0;                 // dword_82FAD30C
    bool sbDrawTags = false;            // byte_82FAD310
    bool sbDrawNormals = false;         // byte_82FAD311
    s32 siSurfaceAlpha = 75;            // dword_82CDB59C
    const WorldEntityModule* spWorldEntityModule = 0; // dword_82FAD2A0

    const u32 K_ZONE_COLOURS[16] =
    {
        0x208080FFu, 0x2000FFFFu, 0x2080FF80u, 0x20FF0000u,
        0x20400080u, 0x20808040u, 0x20FFFF00u, 0x200080FFu,
        0x20FF0080u, 0x20C0C0C0u, 0x20008000u, 0x200000FFu,
        0x20800040u, 0x20000000u, 0x20FFFFFFu, 0x20C08000u,
    };

    Vector3 Add(Vector3 lA, Vector3 lB)
    {
        return Vector3{lA.x + lB.x, lA.y + lB.y, lA.z + lB.z, 0.0f};
    }

    Vector3 Scale(Vector3 lValue, f32 lfScale)
    {
        return Vector3{lValue.x * lfScale, lValue.y * lfScale,
                       lValue.z * lfScale, 0.0f};
    }
}

// ARTIST 0x822F9F40. BaseCollisionGenerator::Destruct, called at entry by the
// original body, is the empty function at 0x8284CB38. The remaining stores are the
// mode-list entries and file-static callback state.
void CollisionDebugComponent::Construct(WorldEntityModule* lpWorldEntityModule)
{
    sbDrawTags = false;
    spWorldEntityModule = lpWorldEntityModule;

    maDrawModeList[0].miValue = E_DRAWMODE_SURFACES;
    maDrawModeList[0].mpcName = "Surfaces";
    maDrawModeList[1].miValue = E_DRAWMODE_FLAGS;
    maDrawModeList[1].mpcName = "Surface flags";
    maDrawModeList[2].miValue = E_DRAWMODE_ZONES;
    maDrawModeList[2].mpcName = "Collision zones";
    maDrawModeList[3].miValue = E_DRAWMODE_TRAFFIC;
    maDrawModeList[3].mpcName = "Traffic colours";
    maDrawModeList[4].miValue = E_DRAWMODE_AISECTION;
    maDrawModeList[4].mpcName = "AI Sections";
    maDrawModeList[5].miValue = E_DRAWMODE_DRIVABLE;
    maDrawModeList[5].mpcName = "Is Drivable";

    CgsSceneManager::TriangleCollisionDebugComponent::RegisterDrawCollisionPolyCallback(
        &CollisionDebugComponent::DrawCollisionPolyCallback, 0);
}

// ARTIST 0x822A8B80. Four variables in exact original order, including the dropdown's
// option table and both signed integer ranges.
void CollisionDebugComponent::OnActivate()
{
    RegisterVariable(&siDrawMode, "Draw mode");
    SetOptions(&siDrawMode, maDrawModeList);
    SetRange(&siDrawMode, 0, 5);

    RegisterVariable(&sbDrawNormals, "Draw normals");

    RegisterVariable(&siSurfaceAlpha, "Surface alpha");
    SetRange(&siSurfaceAlpha, 0, 255);

    RegisterVariable(&sbDrawTags, "Draw tags");
}

u32 CollisionDebugComponent::GetTrafficColour(f32 lfValue)
{
    // flt_820176A8 == 4/pi; the eight threshold branches are the ARTIST body at 0x822A8C50.
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

u32 CollisionDebugComponent::GetSurfaceRenderColour(u8 luSurfaceId)
{
    Attrib::Gen::surface lSurface;
    spWorldEntityModule->GetSurface(luSurfaceId, &lSurface);
    const f32 (&laColour)[4] = lSurface.DebugRenderColor();

    CGS_ASSERT(laColour[0] >= 0.0f && laColour[0] <= 1.0f,
               "lColour.GetX() >= 0.0f && lColour.GetX() <= 1.0f");
    CGS_ASSERT(laColour[1] >= 0.0f && laColour[1] <= 1.0f,
               "lColour.GetY() >= 0.0f && lColour.GetY() <= 1.0f");
    CGS_ASSERT(laColour[2] >= 0.0f && laColour[2] <= 1.0f,
               "lColour.GetZ() >= 0.0f && lColour.GetZ() <= 1.0f");

    const u32 luRed = static_cast<u8>(laColour[0] * 255.0f);
    const u32 luGreen = static_cast<u8>(laColour[1] * 255.0f);
    const u32 luBlue = static_cast<u8>(laColour[2] * 255.0f);
    return (static_cast<u32>(siSurfaceAlpha) << 24) |
           (luRed << 16) | (luGreen << 8) | luBlue;
}

u32 CollisionDebugComponent::GetFlagsRenderColour(CollisionTag lCollisionTag)
{
    const u16 luMaterialTag = lCollisionTag.mu16MaterialTag;
    const bool lbDrivable = (luMaterialTag & KU_COLLISION_FLAG_DRIVEABLE) != 0;
    const bool lbFatal = (luMaterialTag & KU_COLLISION_FLAG_FATAL) != 0;
    const bool lbSuperFatal = (luMaterialTag & KU_COLLISION_FLAG_SUPERFATAL) != 0;

    if (lbDrivable && !lbFatal)
    {
        return 0x80FFFFFFu;
    }
    if (lbSuperFatal)
    {
        return 0xFF00FFFFu;
    }
    if (!lbDrivable && lbFatal)
    {
        return 0x800000FFu;
    }
    if (!lbDrivable && !lbFatal)
    {
        return 0x80000000u;
    }
    return 0;
}

u32 CollisionDebugComponent::GetPolyColour(CollisionTag lCollisionTag,
                                            s32 liPolyIndex)
{
    CGS_ASSERT(siDrawMode >= E_DRAWMODE_SURFACES &&
                   siDrawMode < E_DRAWMODE_COUNT,
               "leDrawMode >= 0 && leDrawMode < E_DRAWMODE_COUNT");

    const u16 luMaterialTag = lCollisionTag.mu16MaterialTag;
    const u16 luGroupTag = lCollisionTag.mu16GroupTag;
    switch (siDrawMode)
    {
    case E_DRAWMODE_SURFACES:
    {
        const u8 luSurfaceId =
            static_cast<u8>((luMaterialTag & KU_COLLISION_MASK_SURFACE_ID) >> 4);
        return (luSurfaceId != 0) ? GetSurfaceRenderColour(luSurfaceId) : 0;
    }
    case E_DRAWMODE_FLAGS:
        return GetFlagsRenderColour(lCollisionTag);
    case E_DRAWMODE_ZONES:
        return K_ZONE_COLOURS[liPolyIndex % 16];
    case E_DRAWMODE_TRAFFIC:
    {
        f32 lfTrafficAngle = 0.0f;
        const TrafficDirection leTraffic =
            lCollisionTag.GetTrafficInfo(&lfTrafficAngle);
        if (leTraffic == E_TRAFFIC_DIRECTION_UNKNOWN)
        {
            return 0x20FFFFFFu;
        }
        if (leTraffic == E_TRAFFIC_DIRECTION_NO_LANES)
        {
            return 0x20000000u;
        }
        return GetTrafficColour(lfTrafficAngle);
    }
    case E_DRAWMODE_AISECTION:
    {
        const u16 luSectionIndex = luGroupTag & 0x7FFFu;
        if (luSectionIndex == KI_INVALID_SECTION_INDEX)
        {
            return 0x200000FFu;
        }
        switch (luSectionIndex % 4)
        {
        case 0: return 0x20006400u;
        case 1: return 0x20009600u;
        case 2: return 0x2000C800u;
        default: return 0x2000FF00u;
        }
    }
    case E_DRAWMODE_DRIVABLE:
        return (luMaterialTag & KU_COLLISION_FLAG_DRIVEABLE) != 0
                   ? 0x20006400u
                   : 0x20000064u;
    default:
        CGS_ASSERT(false, "\nInvalid collision draw mode");
        return 0;
    }
}

// ARTIST 0x822EE728. This callback receives up to four polygon vertices. Each is
// offset by the supplied normal*0.05 before drawing, matching the X360 VMX madds.
void CollisionDebugComponent::DrawCollisionPolyCallback(
    CgsDev::Debug3DImmediateRender* lpDisplay,
    void* lpUserData,
    const Vector3* lpaPolyPoints,
    s32 liNumPolyPoints,
    Vector3 lNormal,
    u32 luCollisionTag,
    s32 liPolyIndex)
{
    (void)lpUserData;
    CGS_ASSERT(lpDisplay != 0, "lpDisplay != NULL");
    CGS_ASSERT(lpaPolyPoints != 0, "lpaPolyPoints != NULL");
    CGS_ASSERT(liNumPolyPoints >= 2, "liNumPolyPoints >= 2");

    Vector3 laPoints[4];
    for (s32 liPoint = 0; liPoint < 4; ++liPoint)
    {
        const Vector3 lOffset = Scale(lNormal, 0.05f);
        laPoints[liPoint] = Add(lpaPolyPoints[liPoint], lOffset);
    }

    CollisionTag lCollisionTag;
    lCollisionTag.Construct(static_cast<u16>(luCollisionTag),
                            static_cast<u16>(luCollisionTag >> 16));
    const u32 luColour = GetPolyColour(lCollisionTag, liPolyIndex);

    Vector3 lPolyCentre;
    if (liNumPolyPoints == 4)
    {
        if (siSurfaceAlpha != 0)
        {
            lpDisplay->DrawSolidQuad(laPoints[0], laPoints[1], laPoints[2],
                                     laPoints[3], luColour);
        }
        lpDisplay->DrawQuad(laPoints[0], laPoints[1], laPoints[2], laPoints[3],
                            0xFFFFFFFFu);
        lPolyCentre = Scale(Add(Add(laPoints[0], laPoints[1]),
                                Add(laPoints[2], laPoints[3])), 0.25f);
    }
    else if (liNumPolyPoints == 3)
    {
        if (siSurfaceAlpha != 0)
        {
            lpDisplay->DrawSolidTriangle(laPoints[0], laPoints[1], laPoints[2],
                                         luColour);
        }
        lpDisplay->DrawHollowTriangle(laPoints[0], laPoints[1], laPoints[2],
                                      0xFFFFFFFFu);
        lPolyCentre = Scale(Add(Add(laPoints[0], laPoints[1]), laPoints[2]),
                            1.0f / 3.0f);
    }
    else
    {
        CGS_ASSERT(false,
                   "Trying to render a polygon that isn't a triangle or quad");
    }

    if (sbDrawNormals)
    {
        lpDisplay->DrawArrow(lPolyCentre, Add(lPolyCentre, lNormal), 0xFFFFFFFFu);
    }

    if (sbDrawTags)
    {
        const Vector3 lTagPosition =
            Scale(Add(Add(laPoints[0], laPoints[1]), laPoints[2]), 0.333f);
        const Vector3 lCameraPosition = lpDisplay->GetCameraPosition();
        const f32 lfDX = lTagPosition.x - lCameraPosition.x;
        const f32 lfDY = lTagPosition.y - lCameraPosition.y;
        const f32 lfDZ = lTagPosition.z - lCameraPosition.z;
        if (sqrtf(lfDX * lfDX + lfDY * lfDY + lfDZ * lfDZ) < 20.0f)
        {
            CgsDev::SimpleStrStream lStream;
            lStream << "( " << static_cast<s32>(liPolyIndex & 0x7FFF) << ")";
            lpDisplay->DrawText(lTagPosition, lStream.GetBuffer(), 10.0f);
        }
    }
}
}
