#include "GameSource/World/AI/Route/BrnRouteMapDebugComponent.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Math/BrnMathUtils.h"
#include "GameSource/World/AI/BrnRouteRequestManager.h"
#include "GameSource/World/AI/BrnAIDebugUtils.h"
#include "GameSource/World/AI/BrnAIBoundaryLine.h"
#include "GameSource/World/AI/BrnAICar.h"
#include <cmath>

#include "GameSource/World/AI/Route/BrnRouteMapModule.h"                          // BrnAI::RouteMapModule
#include "GameSource/World/AI/Route/BrnRoute.h"                                   // BrnAI::Route
#include "GameSource/World/AI/BrnAIModule.h"                                      // BrnAI::AIModule (GetMasterRoute / GetDefaultDistanceFunctionName)
#include "SharedClasses/AI/AISectionsResourceType.h"                             // BrnAI::AISection / AISectionsData / Portal
#include "GameSource/World/AI/BrnAIPortal.h"                                      // BrnAI::Portal (GetPosition / GetLinkSectionIndex)
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h" // CgsDev::Debug2DImmediateRender
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h" // CgsDev::Debug3DImmediateRender
#include "GameShared/GameClasses/Development/CgsStrStream.h"                       // CgsDev::SimpleStrStream
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                           // CgsCore::SPrintf (DrawPortalIds)
#include "GameSource/World/AI/Route/BrnAStar.h"                                   // BrnAI::AStar / AStarNodePool (DrawAStarInMap)

// BrnAI::RouteMapDebugComponent -- the AI route/section-map debug overlay. Reconstructed from the
// X360 ARTIST build (function addresses cited per method) cross-checked against the DecFIGS DWARF
// (member names/virtual shape) and the recovered dependency headers
// (AISection/AISectionsData/Portal/Route, the Debug2D/3D immediate renderers, SimpleStrStream).
//
// The X360 source path is "gamesource\unity\../World/AI/Route/BrnRouteMapDebugComponent.cpp"; the
// asserts in this file reproduce that exact path string + line numbers so the assert text matches
// the target.
//
// VMX NOTE: the X360 build vectorises the per-segment work in the draw loops (normalise a 2D
// direction, lerp two world points, transform a world (x,z) into map space). Those are reconstructed
// here as ordinary scalar/vector C++ -- the geometry, every branch, every cull and every draw call
// is preserved; the literal VMX register shuffles are not (they are pure compiler codegen).

namespace BrnAI
{
namespace
{
    // World-space RenderWare primitives take the vendor RGBA record; preserve ARTIST's packed
    // immediate word verbatim (the debug renderer interprets the low byte as red).
    inline rw::RGBA ToWorldColour(u32 luPackedColour)
    {
        rw::RGBA lColour;
        lColour.m_rgba = luPackedColour;
        return lColour;
    }

    // Source path reproduced verbatim for the asserts (matches the X360 string literal).
    const char* const KPC_SOURCE_FILE =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/AI/Route/BrnRouteMapDebugComponent.cpp";

    // Section/colour-mode tuning constants (BrnRouteMapDebugComponent.cpp:34-55 in the DWARF). Only
    // the ones this TU's bodies actually load are pinned to their X360 literal values.
    const f32  KF_MAP_SCALE_DEFAULT   = 0.1f;   // flt_820C424C (Construct)
    const f32  KF_MAP_OFFSET_X_DEFAULT = 580.0f; // flt_820C5708
    const f32  KF_MAP_OFFSET_Y_DEFAULT = 450.0f; // flt_8203869C

    // High-detail map: above this on-screen section count the draw loop stops unless mbHighDetailMap.
    const s32  KI_LOW_DETAIL_SECTION_LIMIT = 2500;   // cmpwi r21, 0x9C4
    // A junction-line is only drawn when the section's portal separation exceeds this (the X360
    // computes the inverse length, then compares against 25 -- flt_820C4870).
    const f32  KF_MIN_PORTAL_SEPARATION = 25.0f;     // flt_820C4870

    // Packed RGBA8 colours used by the colour pickers (read straight off the X360 li/lis/ori
    // immediates). CgsDev's packed value uses the low byte for red, then green, blue, alpha.
    // ⭐ 2026-09-06 CONSTANT AUDIT: this one is NOT an immediate like the table below it --
    // it is a memory word, and it was recorded as white by assumption. GetSectionTypeColour's
    // fall-through arm reads it (`lwz r11, 0x404(r11)` @0x827681B8, r11 = 0x82F30000) and the
    // .data image word at dword_82F30404 is 0xFF00FFFF. The renderer's channel extraction makes
    // this yellow (R=255,G=255,B=0), not cyan. The same word feeds the section draw at 0x82775550.
    const RGBA K_NORMAL_COLOUR        = 0xFF00FFFFu;  // dword_82F30404 (.data image word; yellow)

    // Type-colour table (GetSectionTypeColour @0x82768120).
    const RGBA K_TYPE_JUNCTION_COLOUR = 0xFFFFFF00u;  // bit 0x04 -> -256
    const RGBA K_TYPE_BIT0_COLOUR     = 0xFF0000FFu;  // bit 0x01 -> -16776961
    const RGBA K_TYPE_BIT1_COLOUR     = 0xFFFF0000u;  // bit 0x02 -> -65536
    const RGBA K_TYPE_PORTAL_COLOUR   = 0xFFFF00FFu;  // bit 0x20 / 0x40 -> -65281
    const RGBA K_TYPE_BIT3_COLOUR     = 0xFF0080FFu;  // bit 0x08 -> -16744193

    // Speed-band table (GetSectionSpeedColour @0x827681C8), indexed by muSpeed.
    const RGBA K_SPEED_COLOUR[5] =
    {
        0xFF0000FFu,  // 0 -> -16776961
        0xFF0080FFu,  // 1 -> -16744193
        0xFF00FFFFu,  // 2 -> -16711681
        0xFF00FFC0u,  // 3 -> -16711744
        0xFF00FF00u,  // 4 -> -16711936
    };
    const RGBA K_SPEED_DEFAULT_COLOUR = 0xFFFFFFFFu;  // -1

    // X360 flt_82F303FC is the squared-distance threshold loaded by DrawPortalIds @0x82767F40
    // and DrawPortals @0x82775280. The image word is 0x47742400 == 62500.0f (a 250-unit radius).
    const f32  KF_PORTAL_CULL_DIST_SQ = 62500.0f;

    // ARTIST table @0x82F30240, indexed in RenderHUD @0x82794110..118.
    const char* const KAPC_ASTAR_DISTANCE_FUNCTION_NAMES[] = {
        "Euclidean", "EuclideanXBiased", "EuclideanYBiased", "Manhattan", "Diagonal"
    };
}
}

// MaybeDrawText (X360 0x82824048) -- the shared "draw debug text iff on-screen" helper used by the
// HUD draw loops. External to this TU (its own TU); declared so the compile gate sees its shape.
// Signature recovered from the call sites: (display, text, x, y, scale, colour, centred).
int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCentred);

namespace BrnAI
{

// ====================================================================================================
// Construct  @0x82767E28
// ====================================================================================================
void RouteMapDebugComponent::Construct(RouteMapModule* lpRouteMapModule, AIModule* lpAIModule)
{
    // The X360 calls the base init first (BaseCollisionGenerator::Destruct is the de-virtualised
    // base two-phase reset that the inliner folded in); reproduce it as the base Construct.
    CgsDev::DebugComponent::Construct();

    CGS_ASSERT(lpRouteMapModule != NULL, "lpRouteMapModule != NULL");

    mpRouteMapModule = lpRouteMapModule;     // +0x0C
    mpAIModule       = lpAIModule;            // +0x14

    // Colour-mode dropdown: {0,"Plain"}, {1,"Type"}, {2,"Speed"}.
    maColourModeList[0].miValue = 0;          // +0x40
    maColourModeList[0].mpcName = "Plain";    // +0x44
    maColourModeList[1].miValue = 1;          // +0x48
    maColourModeList[1].mpcName = "Type";     // +0x4C
    maColourModeList[2].miValue = 2;          // +0x50
    maColourModeList[2].mpcName = "Speed";    // +0x54

    mfMapScale   = KF_MAP_SCALE_DEFAULT;      // +0x18
    mfMapOffsetX = KF_MAP_OFFSET_X_DEFAULT;   // +0x1C
    mfMapOffsetY = KF_MAP_OFFSET_Y_DEFAULT;   // +0x20

    mpAISectionsData = NULL;                  // +0x10 (resolved later in OnActivate)

    // Every toggle starts off (the X360 stb r11(=0) sweep over +0x2C..+0x3B).
    mbDrawAISectionMap            = false;    // +0x2C
    mbHighDetailMap              = false;     // +0x2D
    mbDrawCarsInMap              = false;     // +0x2E
    mbDrawCarRoutesInMap         = false;     // +0x2F
    mbDrawMasterRouteInMap       = false;     // +0x30
    mbDrawAStarInMap             = false;     // +0x31
    mbCarInfoInMap               = false;     // +0x32
    mbDrawDeadEndsInMap          = false;     // +0x33
    mbDrawEventBlockSectionsInMap = false;    // +0x34
    mbOnlyDrawOneCar             = false;     // +0x35
    mbOnlyDrawDataForPlayerSection = false;   // +0x36
    mbDrawPortals                = false;     // +0x37
    mbDrawPortalIds              = false;     // +0x38
    mbDrawPortalLines            = false;     // +0x39
    mbDrawHNGAsPlanes            = false;     // +0x3A
    mbShowSectionInfo            = false;     // +0x3B

    miCarIndex       = 0;                      // +0x24
    miMapColourMode  = 0;                      // +0x28
    miAISectionIndex = 0;                      // +0x3C
}

// ====================================================================================================
// GetName  @0x82767F30
// ====================================================================================================
const char* RouteMapDebugComponent::GetName() const
{
    return "Route Map";
}

// ARTIST vtable @0x820CDFC4, slot 4 ->0x82767678, ICF-folded with
// AIDebugComponent::GetPath. The three instructions return the literal "AI".
const char* RouteMapDebugComponent::GetPath() const
{
    return "AI";
}

// ARTIST inlined in RouteMapModule::Destruct @0x827750E8..0x82775124.
void RouteMapDebugComponent::Destruct()
{
    CGS_ASSERT(mpRouteMapModule != nullptr, "mpRouteMapModule != NULL");
    mpRouteMapModule = nullptr;
    CgsDev::DebugComponent::Destruct();
}

// ====================================================================================================
// GetSectionTypeColour  @0x82768120
// ----------------------------------------------------------------------------------------------------
// Colour a section by its mx8Flags (+0x17). The bit precedence is read straight off the asm's
// rlwinm/cmplwi chain: 0x04 first, then 0x01, 0x02, (0x40 -> portal colour), 0x08, then 0x20 ->
// portal colour, else the normal colour.
// ====================================================================================================
RGBA RouteMapDebugComponent::GetSectionTypeColour(const AISection* lpSection) const
{
    const u8 lu8Flags = lpSection->mx8Flags;

    if (lu8Flags & 0x04)
        return K_TYPE_JUNCTION_COLOUR;
    if (lu8Flags & 0x01)
        return K_TYPE_BIT0_COLOUR;
    if (lu8Flags & 0x02)
        return K_TYPE_BIT1_COLOUR;
    if (lu8Flags & 0x40)
        return K_TYPE_PORTAL_COLOUR;
    if (lu8Flags & 0x08)
        return K_TYPE_BIT3_COLOUR;
    if (lu8Flags & 0x20)
        return K_TYPE_PORTAL_COLOUR;

    return K_NORMAL_COLOUR;
}

// ====================================================================================================
// GetSectionSpeedColour  @0x827681C8
// ====================================================================================================
RGBA RouteMapDebugComponent::GetSectionSpeedColour(const AISection* lpSection) const
{
    CGS_ASSERT(lpSection->muSpeed < E_SECTION_SPEED_COUNT, "muSpeed < E_SECTION_SPEED_COUNT");

    const u8 lu8Speed = lpSection->muSpeed;
    if (lu8Speed < E_SECTION_SPEED_COUNT)
        return K_SPEED_COLOUR[lu8Speed];

    return K_SPEED_DEFAULT_COLOUR;
}

// ====================================================================================================
// GetSectionColour  @0x82775780
// ----------------------------------------------------------------------------------------------------
// Dispatch on the active colour mode (miMapColourMode == liColourMode): 1 -> by type, 2 -> by speed,
// anything else -> the normal colour.
// ====================================================================================================
RGBA RouteMapDebugComponent::GetSectionColour(s32 liColourMode, const AISection* lpSection) const
{
    if (liColourMode == 1)
        return GetSectionTypeColour(lpSection);
    if (liColourMode == 2)
        return GetSectionSpeedColour(lpSection);

    return K_NORMAL_COLOUR;
}

// ====================================================================================================
// Advance100SectionsCallback  @0x82768380
// ----------------------------------------------------------------------------------------------------
// Debug-menu "+100 Sections" action: lpUserData is the component; bump miAISectionIndex (+0x3C) by
// 100. (The X360 reads/writes guest +0x3C directly on the user-data pointer.)
// ====================================================================================================
void RouteMapDebugComponent::Advance100SectionsCallback(void* lpUserData)
{
    RouteMapDebugComponent* lpComponent = static_cast<RouteMapDebugComponent*>(lpUserData);
    lpComponent->miAISectionIndex += 100;
}

// ====================================================================================================
// Back100SectionsCallback  @0x82768390 (ARTIST export hole)
// ----------------------------------------------------------------------------------------------------
// The callback subtracts 100 directly from +0x3C; there is no lower-bound clamp in the image.
// ====================================================================================================
void RouteMapDebugComponent::Back100SectionsCallback(void* lpUserData)
{
    RouteMapDebugComponent* lpComponent = static_cast<RouteMapDebugComponent*>(lpUserData);
    lpComponent->miAISectionIndex -= 100;
}

// ====================================================================================================
// RenderWorld  @0x8278C7D0
// ----------------------------------------------------------------------------------------------------
// 3D overlays, each gated on its toggle: portals, portal-ids, portal-geometry, HNG-as-planes.
// ====================================================================================================
void RouteMapDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay)
{
    CGS_ASSERT(mpRouteMapModule != NULL, "mpRouteMapModule != NULL");

    if (mbDrawPortals)
        DrawPortals(lpDisplay);
    if (mbDrawPortalIds)
        DrawPortalIds(lpDisplay);
    if (mbDrawPortalLines)
        DrawAllPortalGeometry(lpDisplay);
    if (mbDrawHNGAsPlanes)
        DrawHNGAsPlanes(lpDisplay);
}

// ====================================================================================================
// RenderHUD  @0x827940A0
// ----------------------------------------------------------------------------------------------------
// The 2D minimap. Everything is gated behind mbDrawAISectionMap (the master toggle): if it is off,
// nothing in the HUD draws. The default-distance-function readout + master route are drawn when
// mbDrawMasterRouteInMap is set.
// ====================================================================================================
void RouteMapDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay)
{
    if (!mbDrawAISectionMap)
        return;

    DrawAISectionMap(lpDisplay);

    if (mbDrawCarsInMap)
        DrawCarsInMap(lpDisplay);

    if (mbDrawMasterRouteInMap)
    {
        // "Default distance function: <name>" -- the name table is indexed by the AI module's
        // current default distance-function enum. The label is rendered top-left, then the master
        // route itself uses the original packed value 0xFF0000FF (red with this renderer's packing).
        CgsDev::SimpleStrStream lStream;
        const char* lpcDistanceFunction = KAPC_ASTAR_DISTANCE_FUNCTION_NAMES[
            mpAIModule->mRouteRequestManager.meDefaultAStarDistanceFunction];
        lStream << "Default distance function: " << lpcDistanceFunction;
        MaybeDrawText(lpDisplay, lStream.GetBuffer(), 50.0f, 100.0f, 16.0f, 0xFFFFFFFFu, false);

        DrawRouteInMap(lpDisplay, mpAIModule->GetMasterRoute(), 0xFF0000FFu /* -16776961 */);
    }

    if (mbDrawAStarInMap)
        DrawAStarInMap(lpDisplay);
    if (mbDrawEventBlockSectionsInMap)
        DrawEventBlockSectionsInMap(lpDisplay);
    if (mbDrawDeadEndsInMap)
        DrawDeadEndsInMap(lpDisplay);
    if (mbShowSectionInfo)
        DrawAISectionInfo(lpDisplay, static_cast<u16>(miAISectionIndex));
}

// ====================================================================================================
// OnActivate  @0x8277FE50
// ----------------------------------------------------------------------------------------------------
// Register every toggle/slider/dropdown/action with the debug menu. The X360 resolves
// mpAISectionsData from the route-map module's memory resource first (so the section-index slider's
// max can be the live section count), then registers the variables in the asm's exact order.
// ====================================================================================================
void RouteMapDebugComponent::OnActivate()
{
    mpAISectionsData = mpRouteMapModule->GetAISectionsData();

    RegisterVariable(&mbDrawAISectionMap, "Draw AI section map");
    RegisterVariable(&mbHighDetailMap, "High detail map");

    RegisterVariable(&miMapColourMode, "Map colour mode");
    SetRange(&miMapColourMode, 0, 2);
    SetOptions(&miMapColourMode, maColourModeList);

    RegisterVariable(&mbDrawCarsInMap, "Draw cars in map");
    RegisterVariable(&mbDrawMasterRouteInMap, "Draw master route in map");
    RegisterVariable(&mbDrawCarRoutesInMap, "Draw car routes in map");
    RegisterVariable(&mbCarInfoInMap, "Draw car info in map");
    RegisterVariable(&mbDrawAStarInMap, "Draw A* in map");
    RegisterVariable(&mbDrawEventBlockSectionsInMap, "Draw event block sections");
    RegisterVariable(&mbDrawDeadEndsInMap, "Draw dead ends in map");
    RegisterVariable(&mbOnlyDrawOneCar, "Only draw one car");

    RegisterVariable(&miCarIndex, "Car Index");
    RegisterVariable(&mfMapScale, "Map scale");
    RegisterVariable(&mfMapOffsetX, "Map offset X");
    RegisterVariable(&mfMapOffsetY, "Map offset Y");
    SetRange(&miCarIndex, 0, 34);

    RegisterVariable(&mbDrawPortals, "Draw Portals");
    RegisterVariable(&mbDrawPortalIds, "Draw Portal IDs");
    RegisterVariable(&mbDrawPortalLines, "Draw Portal Geometry");
    RegisterVariable(&mbDrawHNGAsPlanes, "Draw HNG as planes");
    RegisterVariable(&mbOnlyDrawDataForPlayerSection, "Player section data only");

    SetRange(&mfMapScale, 0.05f, 1.0f);
    SetStep(&mfMapScale, 0.02f);
    SetStep(&mfMapOffsetX, 10.0f);
    SetStep(&mfMapOffsetY, 10.0f);

    RegisterVariable(&mbShowSectionInfo, "Show AI section info");
    RegisterVariable(&miAISectionIndex, "Section Index");
    RegisterFunction(&RouteMapDebugComponent::Advance100SectionsCallback, this, "+100 Sections");
    RegisterFunction(&RouteMapDebugComponent::Back100SectionsCallback, this, "-100 Sections");
    // X360: clrlwi r6, numSections, 16 -- the section-count max is taken as the low 16 bits.
    SetRange(&miAISectionIndex, 0, static_cast<s32>(mpAISectionsData->muNumSections & 0xFFFFu));
}

// ====================================================================================================
// DrawX  @0x827682A8
// ----------------------------------------------------------------------------------------------------
// Two crossing segments forming an 'X' centred on lv2Centre, each arm of half-extent lfSize.
// ====================================================================================================
void RouteMapDebugComponent::DrawX(CgsDev::Debug2DImmediateRender* lpDisplay,
                                   Vector2 lv2Centre, f32 lfSize, RGBA lColour) const
{
    // Arm 1: top-left (-s,-s) to bottom-right (+s,+s).
    Vector2 lv2A; lv2A.x = lv2Centre.x - lfSize; lv2A.y = lv2Centre.y - lfSize; lv2A.z = 0.0f; lv2A.w = 0.0f;
    Vector2 lv2B; lv2B.x = lv2Centre.x + lfSize; lv2B.y = lv2Centre.y + lfSize; lv2B.z = 0.0f; lv2B.w = 0.0f;
    lpDisplay->DrawLine(lv2A, lv2B, lColour);

    // Arm 2: bottom-left (-s,+s) to top-right (+s,-s).
    Vector2 lv2C; lv2C.x = lv2Centre.x - lfSize; lv2C.y = lv2Centre.y + lfSize; lv2C.z = 0.0f; lv2C.w = 0.0f;
    Vector2 lv2D; lv2D.x = lv2Centre.x + lfSize; lv2D.y = lv2Centre.y - lfSize; lv2D.z = 0.0f; lv2D.w = 0.0f;
    lpDisplay->DrawLine(lv2C, lv2D, lColour);
}

// ====================================================================================================
// DrawPortalIds  @0x82767F40
// ----------------------------------------------------------------------------------------------------
// Draw each section's index as 3D text at its first portal, distance-culled against the camera. The
// camera world position comes from the renderer; cull is on squared distance vs KF_PORTAL_CULL_DIST_SQ.
// ====================================================================================================
void RouteMapDebugComponent::DrawPortalIds(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpRouteMapModule != NULL, "mpRouteMapModule != NULL");

    const Vector3& lrCamera = lpDisplay->GetCameraPosition();

    const u32 luNumSections = mpAISectionsData->muNumSections;
    for (u32 luSection = 0; luSection < luNumSections; ++luSection)
    {
        CGS_ASSERT(luSection < mpAISectionsData->muNumSections, "luSectionIndex < muNumSections");
        const AISection* lpSection = mpAISectionsData->GetAISection(luSection);

        if (lpSection->mu8NumPortals == 0)
            continue;

        const Vector3 lv3Portal = lpSection->GetPortal(0)->GetPosition();

        const f32 lfDX = lv3Portal.x - lrCamera.x;
        const f32 lfDY = lv3Portal.y - lrCamera.y;
        const f32 lfDZ = lv3Portal.z - lrCamera.z;
        const f32 lfDistSq = lfDX * lfDX + lfDY * lfDY + lfDZ * lfDZ;   // vmsum3fp128

        if (lfDistSq > KF_PORTAL_CULL_DIST_SQ)   // vcmpgtfp. -> skip when farther than the cull distance
            continue;

        char acText[20];
        CgsCore::SPrintf(acText, 20, "%d", luSection);
        lpDisplay->DrawText(lv3Portal, acText, 16.0f);
    }
}

// ====================================================================================================
// ToMapCoords
// ----------------------------------------------------------------------------------------------------
// World to minimap: scale then offset. A 3D point maps its ground-plane (x, z).
// ====================================================================================================
Vector2 RouteMapDebugComponent::ToMapCoords(Vector2 lv2World) const
{
    Vector2 lv2Map;
    lv2Map.x = std::fma(lv2World.x, mfMapScale, mfMapOffsetX);
    lv2Map.y = std::fma(lv2World.y, mfMapScale, mfMapOffsetY);
    lv2Map.z = 0.0f;
    lv2Map.w = 0.0f;
    return lv2Map;
}

Vector2 RouteMapDebugComponent::ToMapCoords(Vector3 lv3World) const
{
    Vector2 lv2Map;
    lv2Map.x = std::fma(lv3World.x, mfMapScale, mfMapOffsetX);
    lv2Map.y = std::fma(lv3World.z, mfMapScale, mfMapOffsetY);
    lv2Map.z = 0.0f;
    lv2Map.w = 0.0f;
    return lv2Map;
}

// ====================================================================================================
// DrawRouteInMap
// ----------------------------------------------------------------------------------------------------
// One minimap segment between each consecutive pair of route nodes.
// ====================================================================================================
void RouteMapDebugComponent::DrawRouteInMap(CgsDev::Debug2DImmediateRender* lpDisplay, const Route* lpRoute,
                                            RGBA lColour) const
{
    for (s32 liNode = 0; liNode < lpRoute->GetNodeCount() - 1; )
    {
        const RouteNode* lpFrom = lpRoute->GetNode(liNode);
        Vector2 lv2From;
        lv2From.x = lpFrom->GetX();
        lv2From.y = lpFrom->GetY();
        lv2From.z = 0.0f;
        lv2From.w = 0.0f;

        ++liNode;
        const RouteNode* lpTo = lpRoute->GetNode(liNode);
        Vector2 lv2To;
        lv2To.x = lpTo->GetX();
        lv2To.y = lpTo->GetY();
        lv2To.z = 0.0f;
        lv2To.w = 0.0f;

        lpDisplay->DrawLine(ToMapCoords(lv2From), ToMapCoords(lv2To), lColour);
    }
}

// ====================================================================================================
// DrawAStarInMap
// ----------------------------------------------------------------------------------------------------
// The route-map module's A* search state: every live node as a point (green while open), a text line
// with the open count, the per-bucket and total closed counts, the cost weight, iteration count,
// distance heuristic and the block-section ids, and an X over each block section.
// The heuristic test checks Euclidean on its own before the else-if chain over the others, so a
// Euclidean search prints "EuclideanUnknown".
// ====================================================================================================
void RouteMapDebugComponent::DrawAStarInMap(CgsDev::Debug2DImmediateRender* lpDisplay) const
{
    static const f32  KF_NODE_POINT_SIZE    = 2.0f;
    static const RGBA KU_OPEN_NODE_COLOUR   = 0xFF00FF00u;
    static const RGBA KU_CLOSED_NODE_COLOUR = 0xFF0000C8u;
    static const f32  KF_BLOCK_X_SIZE       = 5.0f;
    static const RGBA KU_BLOCK_X_COLOUR     = 0xFF0000FFu;

    AStar&         lrAStar     = mpRouteMapModule->mAStar;
    AStarNodePool& lrNodePool  = lrAStar.mAStarNodePool;
    s32            liNodeTotal = 0;

    CgsDev::SimpleStrStream lStream;
    lStream << "Open nodes: " << static_cast<s32>(lrNodePool.muOpenNodeCount) << ", Closed nodes: ( ";

    for (u32 luPartition = 0; luPartition < AStarNodePool::KU_PARTITION_COUNT; ++luPartition)
    {
        const u32 luFirstNode = luPartition * AStarNodePool::KU_MAX_PARTITION_NODES;
        liNodeTotal += lrNodePool.mauNodeCount[luPartition];
        lStream << static_cast<s32>(lrNodePool.mauNodeCount[luPartition]) << " ";

        for (s32 liNode = 0; liNode < lrNodePool.mauNodeCount[luPartition]; ++liNode)
        {
            const AStarNode* lpNode = lrNodePool.GetNode(static_cast<u16>(luFirstNode + liNode));
            const AStarVector2 lPosition = lpNode->GetPosition();

            Vector2 lv2Position;
            lv2Position.x = lPosition.X();
            lv2Position.y = lPosition.Y();
            lv2Position.z = 0.0f;
            lv2Position.w = 0.0f;

            DrawPoint(lpDisplay, ToMapCoords(lv2Position), KF_NODE_POINT_SIZE,
                      lpNode->IsOpen() ? KU_OPEN_NODE_COLOUR : KU_CLOSED_NODE_COLOUR);
        }
    }

    lStream << ")" << liNodeTotal;
    lStream << ", Cost weight: " << lrAStar.mfCostWeight;
    lStream << ", Iteration count: " << lrAStar.miIterationCount;
    lStream << ", Distance function: ";

    if (lrAStar.mpDistanceFunction == &AStar::EuclideanDistance)
    {
        lStream << "Euclidean";
    }

    if (lrAStar.mpDistanceFunction == &AStar::EuclideanDistanceXBiased)
    {
        lStream << "EuclideanXBiased";
    }
    else if (lrAStar.mpDistanceFunction == &AStar::EuclideanDistanceYBiased)
    {
        lStream << "EuclideanYBiased";
    }
    else if (lrAStar.mpDistanceFunction == &AStar::DiagonalDistance)
    {
        lStream << "Diagonal";
    }
    else
    {
        lStream << "Unknown";
    }

    lStream << ", Block sections : " << lrAStar.miBlockSectionCount << " (";

    for (s32 liBlock = 0; liBlock < lrAStar.miBlockSectionCount; ++liBlock)
    {
        const u32 luBlockSectionId = lrAStar.maBlockSectionIds[liBlock];
        CgsDev::StrStreamBase& lrStream = lStream;
        lrStream << luBlockSectionId << " ";

        for (s32 liSection = 0; liSection < static_cast<u16>(mpAISectionsData->muNumSections); ++liSection)
        {
            if (mpAISectionsData->GetAISection(liSection)->mId == luBlockSectionId)
            {
                const Vector3 lv3Middle = mpAISectionsData->GetAISection(liSection)->GetMiddle();
                DrawX(lpDisplay, ToMapCoords(lv3Middle), KF_BLOCK_X_SIZE, KU_BLOCK_X_COLOUR);
                break;
            }
        }
    }

    lStream << ")";
    MaybeDrawText(lpDisplay, lStream.GetBuffer(), 50.0f, 70.0f, 16.0f, 0xFFFFFFFFu, false);
}

// ====================================================================================================
// DrawAISectionInfo
// ----------------------------------------------------------------------------------------------------
// A circle on the minimap at the chosen section's middle and a line with its index, id, the link
// section of each portal and its world middle.
// ====================================================================================================
void RouteMapDebugComponent::DrawAISectionInfo(CgsDev::Debug2DImmediateRender* lpDisplay, u16 luSectionIndex)
{
    const AISection* lpSection = mpAISectionsData->GetAISection(luSectionIndex);

    CgsDev::SimpleStrStream lStream;
    lpDisplay->DrawCircle(ToMapCoords(lpSection->GetMiddle()), 2.0f, 5, 0xFFFFFFFFu);

    lStream << "Index: " << static_cast<s32>(luSectionIndex) << ", ID: " << lpSection->mId << ", Links: ";

    for (s32 liPortal = 0; liPortal < lpSection->mu8NumPortals; ++liPortal)
    {
        const Portal* lpPortal = lpSection->GetPortal(static_cast<u8>(liPortal));
        lStream << static_cast<s32>(lpPortal->GetLinkSectionIndex()) << " ";
    }

    lStream << ", Position: ";
    const Vector3 lv3Middle = lpSection->GetMiddle();
    lStream.AppendFormat("(%f, %f, %f)", lv3Middle.x, lv3Middle.y, lv3Middle.z);

    MaybeDrawText(lpDisplay, lStream.GetBuffer(), 50.0f, 100.0f, 16.0f, 0xFFFFFFFFu, false);
}


// ARTIST 0x8278C890; DWARF BrnRouteMapDebugComponent.cpp:336.
void RouteMapDebugComponent::DrawAISectionMap(CgsDev::Debug2DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpRouteMapModule != NULL, "mpRouteMapModule != NULL");
    CgsDev::SimpleStrStream lStream;
    s32 liLineCount = 0;
    for (u16 luSection = 0; luSection < mpAISectionsData->muNumSections; ++luSection)
    {
        const AISection* lpSection = mpAISectionsData->GetAISection(luSection);
        if (liLineCount >= 2500 && !mbHighDetailMap)
            break;
        if (lpSection->mu8NumPortals <= 1)
            continue;

        const Vector3 lFirstPortal = lpSection->GetPortal(0)->GetPosition();
        const Vector3 lSecondPortal = lpSection->GetPortal(1)->GetPosition();
        const f32 lfDX = lSecondPortal.x - lFirstPortal.x;
        const f32 lfDY = lSecondPortal.y - lFirstPortal.y;
        const f32 lfDZ = lSecondPortal.z - lFirstPortal.z;
        const f32 lfLength = std::sqrt(lfDX * lfDX + lfDY * lfDY + lfDZ * lfDZ);
        if (!mbHighDetailMap && (lpSection->miSpanIndex == -1 || (lpSection->mx8Flags & 0xC5) != 0
                                || lfLength <= 25.0f))
            continue;
        // Original fcmpu/ble rejects <=25; unordered length takes the draw arm.

        if (lpSection->mu8NumPortals == 2)
        {
            Vector2 lStart = ToMapCoords(lpSection->GetPortal(0)->GetPosition());
            Vector2 lEnd = ToMapCoords(lpSection->GetPortal(1)->GetPosition());
            if (static_cast<u32>(luSection) < mpAISectionsData->muNumSections - 1)
            {
                const u32 luNextSection = static_cast<u32>(luSection) + 1;
                const AISection* lpNextSection = mpAISectionsData->GetAISection(luNextSection);
                if (lpNextSection->mu8NumPortals == 2)
                {
                    const Vector2 lCurrent0 = ToMapCoords(lpSection->GetPortal(0)->GetPosition());
                    const Vector2 lCurrent1 = ToMapCoords(lpSection->GetPortal(1)->GetPosition());
                    const Vector2 lNext0 = ToMapCoords(lpNextSection->GetPortal(0)->GetPosition());
                    const Vector2 lNext1 = ToMapCoords(lpNextSection->GetPortal(1)->GetPosition());

                    // These are four independent original comparisons. Multiple
                    // matches increment the u16 section cursor multiple times.
                    if (lCurrent1.x == lNext0.x && lCurrent1.y == lNext0.y)
                    {
                        lStart = lCurrent0;
                        lEnd = lNext1;
                        ++luSection;
                    }
                    if (lCurrent0.x == lNext1.x && lCurrent0.y == lNext1.y)
                    {
                        lStart = lCurrent1;
                        lEnd = lNext0;
                        ++luSection;
                    }
                    if (lCurrent0.x == lNext0.x && lCurrent0.y == lNext0.y)
                    {
                        lStart = lCurrent1;
                        lEnd = lNext1;
                        ++luSection;
                    }
                    if (lCurrent1.x == lNext1.x && lCurrent1.y == lNext1.y)
                    {
                        lStart = lCurrent0;
                        lEnd = lNext0;
                        ++luSection;
                    }
                }
            }
            lpDisplay->DrawLine(lStart, lEnd, GetSectionColour(miMapColourMode, lpSection));
            ++liLineCount;
        }
        else if (mbHighDetailMap)
        {
            const Vector2 lMiddle = ToMapCoords(lpSection->GetMiddle());
            for (s32 liPortal = 0; liPortal < lpSection->mu8NumPortals; ++liPortal)
            {
                const Portal* lpPortal = lpSection->GetPortal(static_cast<u8>(liPortal));
                const RGBA lColour = GetSectionColour(miMapColourMode, lpSection);
                const Vector2 lPortal = ToMapCoords(lpPortal->GetPosition());
                lpDisplay->DrawLine(lMiddle, lPortal, lColour);
                ++liLineCount;
            }
        }
        else
        {
            CGS_ASSERT(lpSection->mu8NumPortals <= 4, "lpAISection->GetPortalCount() <= 4");
            Vector2 laPortalPositions[4];
            s32 liPositionCount = 0;
            for (s32 liPortal = 0; liPortal < lpSection->mu8NumPortals; ++liPortal)
            {
                const Portal* lpPortal = lpSection->GetPortal(static_cast<u8>(liPortal));
                const AISection* lpLinkedSection = mpAISectionsData->GetAISection(lpPortal->GetLinkSectionIndex());
                if ((lpLinkedSection->mx8Flags & 0x01) == 0)
                    laPortalPositions[liPositionCount++] = ToMapCoords(lpPortal->GetPosition());
            }
            if (liPositionCount >= 2)
            {
                lpDisplay->DrawLine(laPortalPositions[0], laPortalPositions[1],
                                    GetSectionColour(miMapColourMode, lpSection));
                ++liLineCount;
            }
        }
    }
    lStream << "Section count: " << static_cast<s32>(mpAISectionsData->muNumSections)
            << ", Line count: " << liLineCount;
    MaybeDrawText(lpDisplay, lStream.GetBuffer(), 50.0f, 50.0f, 16.0f, 0xFFFFFFFFu, false);
}

// ARTIST 0x8278D7B0; DWARF BrnRouteMapDebugComponent.cpp:582.
void RouteMapDebugComponent::DrawCarsInMap(CgsDev::Debug2DImmediateRender* lpDisplay) const
{
    for (s32 liCar = 0; liCar < KI_MAX_OUT_OF_RANGE_RACE_CARS; ++liCar)
    {
        const AICar* lpCar = &mpAIModule->maAICars[liCar];
        if ((mbOnlyDrawOneCar && miCarIndex != liCar) || lpCar->meCarState == E_AI_CAR_STATE_INACTIVE)
            continue;

        const Vector2 lPosition = ToMapCoords(lpCar->GetPosition());
        const Vector2 lFlatDirection = BrnMath::Flatten(lpCar->GetDirection());
        const f32 lfInverseLength = 1.0f / std::sqrt(
            lFlatDirection.x * lFlatDirection.x + lFlatDirection.y * lFlatDirection.y);
        // Original normalization has no zero-length fallback.
        const Vector2 lDirection = {lFlatDirection.x * lfInverseLength,
                                   lFlatDirection.y * lfInverseLength, 0.0f, 0.0f};
        const Vector2 lRight = {lDirection.y, -lDirection.x, 0.0f, 0.0f};

        RGBA lColour = 0xFF000000u;
        if (lpCar->mbIsPlayer)
            lColour = 0xC8FF8000u;
        else if (lpCar->meCarState == E_AI_CAR_STATE_OUT_OF_RANGE)
            lColour = 0xC80000FFu;
        else if (lpCar->meCarState == E_AI_CAR_STATE_IN_RANGE)
            lColour = lpCar->mbIsCrashing ? 0xC8202020u : 0xC800FF00u;

        const Vector2 lBack = {lPosition.x - lDirection.x * 4.0f,
                              lPosition.y - lDirection.y * 4.0f, 0.0f, 0.0f};
        const Vector2 laArrow[4] =
        {
            {std::fma(lDirection.x, 10.0f, lPosition.x),
             std::fma(lDirection.y, 10.0f, lPosition.y), 0.0f, 0.0f},
            {lBack.x - lRight.x * 5.0f, lBack.y - lRight.y * 5.0f, 0.0f, 0.0f},
            lPosition,
            {std::fma(lRight.x, 5.0f, lBack.x), std::fma(lRight.y, 5.0f, lBack.y), 0.0f, 0.0f},
        };
        lpDisplay->DrawSolidConvexPolygon(laArrow, 4, lColour);
        lpDisplay->DrawWirePolygon(laArrow, 4, 0xFF000000u);

        if (mbCarInfoInMap)
        {
            CgsDev::SimpleStrStream lStream;
            lStream << static_cast<s32>(lpCar->mfDistanceToCheckpoint) << "m";
            MaybeDrawText(lpDisplay, lStream.GetBuffer(), lPosition.x, lPosition.y - 10.0f,
                          10.0f, 0xFFFFFFFFu, false);
            const u16 luDestination = lpCar->GetDestinationSectionIndex();
            if (luDestination != AICar::KI_INVALID_SECTION_INDEX)
            {
                lpDisplay->DrawLine(lPosition,
                    ToMapCoords(mpAISectionsData->GetAISection(luDestination)->GetMiddle()), 0xFFFFFFFFu);
            }
        }

        if (mbDrawCarRoutesInMap)
        {
            const u16 luDestination = lpCar->GetDestinationSectionIndex();
            if (luDestination != AICar::KI_INVALID_SECTION_INDEX)
            {
                lpDisplay->DrawCircle(ToMapCoords(mpAISectionsData->GetAISection(luDestination)->GetMiddle()),
                                      5.0f, 6, 0xFFFFFFFFu);
            }
            const Route* lpRoute = lpCar->GetRoute();
            if (lpRoute->GetStatus() != Route::E_STATUS_UNINITIALISED && lpRoute->GetNodeCount() > 0)
                DrawRouteInMap(lpDisplay, lpRoute, lpCar->mbIsPlayer ? 0xFF8000FFu : 0xFFFF00FFu);
        }
    }
}

// ARTIST 0x8278DD28; DWARF BrnRouteMapDebugComponent.cpp:713.
void RouteMapDebugComponent::DrawEventBlockSectionsInMap(CgsDev::Debug2DImmediateRender* lpDisplay) const
{
    for (s32 liCheckpoint = 0; liCheckpoint < 16; ++liCheckpoint)
    {
        CgsDev::SimpleStrStream lStream;
        const Array<u32, 8u>& lrBlockIds = mpAIModule->mRouteRequestManager.mauBlockSectionIds[liCheckpoint];
        // GetLength retains the original unconstructed-array assert. The signed
        // comparison after that assert is also original; do not widen -1 to a loop.
        const s32 liCount = static_cast<s32>(lrBlockIds.GetLength());
        if (liCount <= 0)
            continue;

        lStream << "Checkpoint " << liCheckpoint << ", Block section count: " << liCount << " ( ";
        for (s32 liBlock = 0; liBlock < liCount; ++liBlock)
        {
            const u32 luSectionId = lrBlockIds.GetItem(static_cast<u32>(liBlock));
            static_cast<CgsDev::StrStreamBase&>(lStream) << luSectionId << " ";
            for (u32 luSection = 0; luSection < mpAISectionsData->muNumSections; ++luSection)
            {
                if (mpAISectionsData->GetAISection(luSection)->mId == luSectionId)
                {
                    DrawX(lpDisplay, ToMapCoords(mpAISectionsData->GetAISection(luSection)->GetMiddle()),
                          5.0f, 0xFF0000FFu);
                    break;
                }
            }
        }
        lStream << " )";
        MaybeDrawText(lpDisplay, lStream.GetBuffer(), 50.0f,
                      std::fma(static_cast<f32>(liCheckpoint), 20.0f, 100.0f),
                      16.0f, 0xFFFFFFFFu, false);
    }
}

// ARTIST 0x82775280; DWARF BrnRouteMapDebugComponent.cpp:869.
void RouteMapDebugComponent::DrawPortals(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpRouteMapModule != NULL, "mpRouteMapModule != NULL");
    const Vector3 lCamera = lpDisplay->GetCameraPosition();
    for (u16 luSection = 0; luSection < mpAISectionsData->muNumSections; ++luSection)
    {
        const AISection* lpSection = mpAISectionsData->GetAISection(luSection);
        const u8 luPortalCount = lpSection->mu8NumPortals;
        if (luPortalCount == 0)
            continue;

        const Vector3 lFirstPortal = lpSection->GetPortal(0)->GetPosition();
        const f32 lfDX = lFirstPortal.x - lCamera.x;
        const f32 lfDY = lFirstPortal.y - lCamera.y;
        const f32 lfDZ = lFirstPortal.z - lCamera.z;
        if (lfDX * lfDX + lfDY * lfDY + lfDZ * lfDZ > 62500.0f)
            continue;

        for (u8 luPortal = 0; luPortal < luPortalCount; ++luPortal)
        {
            const Portal* lpPortal = lpSection->GetPortal(luPortal);
            const Vector3 lPosition = lpPortal->GetPosition();
            const Vector3 lMin = {lPosition.x - 1.0f, lPosition.y - 1.0f, lPosition.z - 1.0f, 0.0f};
            const Vector3 lMax = {lPosition.x + 1.0f, lPosition.y + 1.0f, lPosition.z + 1.0f, 0.0f};
            // ARTIST's unnamed 0x8281E548 is the actual AA solid-box renderer.
            lpDisplay->DrawSolidBox(lMin, lMax, ToWorldColour(0xFF00FF00u));

            const AISection* lpLinkedSection = mpAISectionsData->GetAISection(lpPortal->GetLinkSectionIndex());
            const u8 luLinkedPortalCount = lpLinkedSection->mu8NumPortals;
            for (u8 luLinkedPortal = 0; luLinkedPortal < luLinkedPortalCount; ++luLinkedPortal)
            {
                const Vector3 lLinkedPosition = lpLinkedSection->GetPortal(luLinkedPortal)->GetPosition();
                const RGBA lColour = GetSectionTypeColour(lpLinkedSection);
                lpDisplay->DrawLine(lPosition, lLinkedPosition, ToWorldColour(lColour));
            }
        }
    }
}

// ARTIST 0x82780138; DWARF BrnRouteMapDebugComponent.cpp:978.
void RouteMapDebugComponent::DrawHNGAsPlanes(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    // The original directly indexes the active-player cursor, with no -1 guard.
    const AICar* lpPlayerCar = &mpAIModule->maAICars[mpAIModule->mePlayerActiveRaceCarIndex];
    const u16 luSection = lpPlayerCar->GetBestSectionIndex();
    if (luSection == AICar::KI_INVALID_SECTION_INDEX)
        return;

    const AISection* lpSection = mpAISectionsData->GetAISection(luSection);
    DrawHNGSectionAsPlanes(lpDisplay, luSection);
    if (mbOnlyDrawDataForPlayerSection)
        return;

    for (s32 liPortal = 0; liPortal < lpSection->mu8NumPortals; ++liPortal)
    {
        const u16 luNeighbour = lpSection->GetPortal(static_cast<u8>(liPortal))->GetLinkSectionIndex();
        const AISection* lpNeighbour = mpAISectionsData->GetAISection(luNeighbour);
        DrawHNGSectionAsPlanes(lpDisplay, luNeighbour);
        for (s32 liNextPortal = 0; liNextPortal < lpNeighbour->mu8NumPortals; ++liNextPortal)
        {
            DrawHNGSectionAsPlanes(lpDisplay,
                lpNeighbour->GetPortal(static_cast<u8>(liNextPortal))->GetLinkSectionIndex());
        }
    }
}

// ARTIST 0x827758F0; DWARF BrnRouteMapDebugComponent.cpp:1265.
void RouteMapDebugComponent::DrawHNGSectionAsPlanes(CgsDev::Debug3DImmediateRender* lpDisplay,
                                                   u16 luSectionIndex) const
{
    const AISection* lpSection = mpAISectionsData->GetAISection(luSectionIndex);
    for (s32 liLine = 0; liLine < lpSection->muNumNoGoLines; ++liLine)
    {
        const u16 luLine = static_cast<u16>(liLine);
        CGS_ASSERT(luLine < lpSection->muNumNoGoLines, "luHNGLineIndex < muNumNoGoLines");
        const BoundaryLine& lrLine = lpSection->mpaNoGoLines[luLine];
        const f32 lfMiddleY = lpSection->GetMiddle().y;
        const Vector3 lBottomStart = {lrLine.mfStartX, lfMiddleY - 20.0f, lrLine.mfStartY, 0.0f};
        const Vector3 lTopStart = {lrLine.mfStartX, lfMiddleY + 10.0f, lrLine.mfStartY, 0.0f};
        const Vector3 lTopEnd = {lrLine.mfEndX, lfMiddleY + 10.0f, lrLine.mfEndY, 0.0f};
        const Vector3 lBottomEnd = {lrLine.mfEndX, lfMiddleY - 20.0f, lrLine.mfEndY, 0.0f};
        lpDisplay->DrawSolidQuad(lBottomStart, lTopStart, lTopEnd, lBottomEnd,
                                 ToWorldColour(0x800040FFu));
        lpDisplay->DrawSolidQuad(lBottomEnd, lTopEnd, lTopStart, lBottomStart,
                                 ToWorldColour(0x804000FFu));
    }
}

// ARTIST export hole 0x827755C8, called directly at RenderWorld 0x8278C85C.
// DWARF BrnRouteMapDebugComponent.cpp:1022. This is an original separate method.
void RouteMapDebugComponent::DrawAllPortalGeometry(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    CGS_ASSERT(mpRouteMapModule != NULL, "mpRouteMapModule != NULL");
    const Vector3 lCamera = lpDisplay->GetCameraPosition();
    for (u16 luSection = 0; luSection < mpAISectionsData->muNumSections; ++luSection)
    {
        const AISection* lpSection = mpAISectionsData->GetAISection(luSection);
        const u8 luPortalCount = lpSection->mu8NumPortals;
        if (luPortalCount == 0)
            continue;

        const Vector3 lMiddle = lpSection->GetMiddle();
        const f32 lfDX = lMiddle.x - lCamera.x;
        const f32 lfDY = lMiddle.y - lCamera.y;
        const f32 lfDZ = lMiddle.z - lCamera.z;
        if (lfDX * lfDX + lfDY * lfDY + lfDZ * lfDZ > 62500.0f)
            continue;

        // Original quirk: this arm reads slot zero, independently of the active
        // player cursor used by DrawHNGAsPlanes. Do not repair that distinction.
        const bool lbIsPlayerSection = luSection == mpAIModule->maAICars[0].GetBestSectionIndex();
        if (mbOnlyDrawDataForPlayerSection && !lbIsPlayerSection)
            continue;

        for (u8 luPortal = 0; luPortal < luPortalCount; ++luPortal)
            BrnAIDebugUtils::DrawPortalGeometry(lpDisplay, lpSection->GetPortal(luPortal), lbIsPlayerSection);
    }
}

// ARTIST 0x827757F0. The export's Hex-Rays signature is register-soup; its VMX128
// stream loads the by-value centre from v1, expands lfSize, then calls the scalar
// DrawBox wrapper with the two min/max corners and forwards lColour unchanged.
void RouteMapDebugComponent::DrawPoint(CgsDev::Debug2DImmediateRender* lpDisplay,
                                      Vector2 lv2Centre, f32 lfSize, RGBA lColour) const
{
    const f32 lfMinX = lv2Centre.x - lfSize;
    const f32 lfMinY = lv2Centre.y - lfSize;
    const f32 lfMaxX = lv2Centre.x + lfSize;
    const f32 lfMaxY = lv2Centre.y + lfSize;
    lpDisplay->DrawBox(lfMinX, lfMinY, lfMaxX, lfMaxY, lColour);
}

// ARTIST export hole 0x8278D350, called directly from RenderHUD @0x827941B4.
// The target stream is decoded from the original image with tools/re/ppcdis.py;
// the X360 compares mx8Flags bits against DecFIGS' SHORTCUT (0x01) and NO_RESET
// (0x02) values, then highlights one-way portal links and duplicate destinations.
void RouteMapDebugComponent::DrawDeadEndsInMap(CgsDev::Debug2DImmediateRender* lpDisplay) const
{
    const u16 luNumSections = static_cast<u16>(mpAISectionsData->muNumSections);
    for (u32 luSectionIndex = 0; luSectionIndex < luNumSections; ++luSectionIndex)
    {
        const AISection* lpSection = mpAISectionsData->GetAISection(luSectionIndex);
        const u8 luPortalCount = lpSection->mu8NumPortals;
        bool lbDrawSection = false;
        RGBA lColour = 0xFFFFFFFFu;

        if (luPortalCount < 2)
        {
            // ARTIST tests NO_RESET before SHORTCUT and selects these three original colours.
            if ((lpSection->mx8Flags & 0x02u) != 0)
                lColour = 0xFFFF0000u;
            else if ((lpSection->mx8Flags & 0x01u) != 0)
                lColour = 0xFF0000FFu;
            else
                lColour = 0xFF00FF00u;

            lbDrawSection = true;
        }

        // A portal with no reciprocal portal in its linked section marks this section yellow.
        // Keep scanning after a match, as the original loop does.
        for (s32 liPortal = 0; liPortal < luPortalCount; ++liPortal)
        {
            const Portal* lpPortal = lpSection->GetPortal(static_cast<u8>(liPortal));
            const u16 luLinkSectionIndex = lpPortal->GetLinkSectionIndex();
            const AISection* lpLinkSection = mpAISectionsData->GetAISection(luLinkSectionIndex);
            bool lbBackPortalFound = false;

            for (s32 liBackPortal = 0; liBackPortal < lpLinkSection->mu8NumPortals; ++liBackPortal)
            {
                if (lpLinkSection->GetPortal(static_cast<u8>(liBackPortal))->GetLinkSectionIndex() ==
                    luSectionIndex)
                {
                    lbBackPortalFound = true;
                }
            }

            if (!lbBackPortalFound)
            {
                lbDrawSection = true;
                lColour = 0xFFFFFF00u;
            }
        }

        // Repeated links from distinct portals mark the section white. The console checks
        // every ordered pair (skipping i == j); retain that loop shape and its colour priority.
        for (s32 liPortal0 = 0; liPortal0 < luPortalCount; ++liPortal0)
        {
            for (s32 liPortal1 = 0; liPortal1 < luPortalCount; ++liPortal1)
            {
                if (liPortal0 == liPortal1)
                    continue;

                const u16 luLink0 = lpSection->GetPortal(static_cast<u8>(liPortal0))->GetLinkSectionIndex();
                const u16 luLink1 = lpSection->GetPortal(static_cast<u8>(liPortal1))->GetLinkSectionIndex();
                if (luLink0 == luLink1)
                {
                    lbDrawSection = true;
                    lColour = 0xFFFFFFFFu;
                }
            }
        }

        if (!lbDrawSection)
            continue;

        CgsDev::SimpleStrStream lStream;
        static_cast<CgsDev::StrStreamBase&>(lStream) << luSectionIndex << "-" << lpSection->mId;

        const Vector2 lv2MapPosition = ToMapCoords(lpSection->GetMiddle());
        DrawX(lpDisplay, lv2MapPosition, 5.0f, lColour);

        if (mbHighDetailMap)
            MaybeDrawText(lpDisplay, lStream.GetBuffer(), lv2MapPosition.x, lv2MapPosition.y,
                          12.0f, 0xFFFFFFFFu, false);
    }
}

}
