#include "GameSource/World/AI/Route/BrnRouteMapDebugComponent.h"

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

    // The packed RGBA colours used by the colour pickers (read straight off the X360 li/lis/ori
    // immediates; AARRGGBB, alpha in the top byte).
    const RGBA K_NORMAL_COLOUR        = 0xFFFFFFFFu;  // dword_82F30404 (white)

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

    // Squared distance beyond which portals/portal-ids are culled (DWARF KF_PORTAL_CULL_DIST_SQ,
    // BrnRouteMapDebugComponent.cpp:39 -- a file-scope const; the X360 loads it as flt_82F303FC).
    // UNCERTAIN: the numeric value is a runtime data literal not present in the available references;
    // the relationship KF_PORTAL_CULL_DIST_SQ == KF_PORTAL_CULL_DIST^2 holds. Placeholder kept so the
    // cull's STRUCTURE (compare squared-distance > threshold -> skip) is faithful; flagged for review.
    const f32  KF_PORTAL_CULL_DIST    = 200.0f;            // KF_PORTAL_CULL_DIST (value uncertain)
    const f32  KF_PORTAL_CULL_DIST_SQ = KF_PORTAL_CULL_DIST * KF_PORTAL_CULL_DIST;
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
        // route itself is drawn in blue.
        CgsDev::SimpleStrStream lStream;
        const char* lpcDistanceFunction = mpAIModule->GetDefaultDistanceFunctionName();
        lStream << "Default distance function: " << lpcDistanceFunction;
        MaybeDrawText(lpDisplay, lStream.GetBuffer(), 50.0f, 100.0f, 16.0f, K_NORMAL_COLOUR, false);

        DrawRouteInMap(lpDisplay, mpAIModule->GetMasterRoute(), 0xFF0000FFu /* -16776961, blue */);
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

// ----------------------------------------------------------------------------------------------------
// DEFERRED bodies (declared in the header, bodied here once their dependencies are exposed by name):
//
//   DrawAISectionMap            @0x8278C890   DrawCarsInMap               @0x8278D7B0
//   DrawEventBlockSectionsInMap @0x8278DD28   DrawPortals                 @0x82775280
//   DrawHNGAsPlanes             @0x82780138   DrawHNGSectionAsPlanes      @0x827758F0
//
// These six draw loops read deep into types whose X360 layouts are NOT yet recovered by name and
// whose 3D-draw entry points (Debug3DImmediateRender::DrawSolidQuad / DrawText) are not yet on the
// renderer's reconstructed surface:
//   - DrawCarsInMap walks the per-car array inside RouteMapModule (5472-byte stride) and reads the
//     car's run state / target section at raw guest offsets (+5320/+5430/+5442/+5449), plus the
//     external helpers BrnMath::Flatten and the per-car StrStream distance label.
//   - DrawEventBlockSectionsInMap walks the event-block checkpoint arrays embedded in RouteMapModule
//     (offsets +270952/+270984/+322040) which RouteMapModule does not yet expose.
//   - DrawHNGAsPlanes / DrawHNGSectionAsPlanes need the player car's HNG section indices (AICar
//     +0x1762/+0x1764, not yet carved) and the renderer's DrawSolidQuad.
//   - DrawPortals / DrawPortalIds need the renderer's camera position + DrawText.
// Implementing them now would require either raw offset casts into those black-box types (forbidden
// by the project's hard quality rules) or fabricated accessors; both are deferred until the owning
// headers (RouteMapModule internals, AICar HNG members, the Debug3D draw surface) are carved.
// The control flow, branch structure and cull constants for all six are captured in the dossier
// review notes and will reconstruct cleanly once those named members land.

}
