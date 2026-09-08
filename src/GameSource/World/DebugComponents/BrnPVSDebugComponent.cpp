// Bodies for the streaming-PVS debug component, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   PVSDebugComponent::Construct               @ 0x827B2108   (bodied here)
//   PVSDebugComponent::RenderHUD               @ 0x827CEAD8   (bodied here)
//   PVSDebugComponent::RenderCollisionZones    @ 0x827C7378   (bodied here)
//   PVSDebugComponent::OnActivate              @ 0x827B2178
//   PVSDebugComponent::RenderPVS               @ 0x827C6E58
//   PVSDebugComponent::RenderPvsCentrePosition @ 0x827BFB08

#include "GameSource/World/DebugComponents/BrnPVSDebugComponent.h"

#include "GameSource/Game/BrnGameModule.hpp"                                                   // BrnGame::sbShowStreamStallMessage
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h" // Debug2DImmediateRender (DrawCircle / DrawText / CalcTextWidth)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                      // CgsCore::SPrintf
#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.h"
#include "GameSource/World/EntityModules/WorldEntityModule/PVSModule/BrnPVSModule.h"
#include "GameSource/Resource/SharedIO/BrnAssetIds.h"

#include <cmath>   // sqrtf (the X360's vmsum3fp + vrsqrtefp Newton sequence == a 3D length)

namespace BrnWorld
{
    // ------------------------------------------------------------------------------------------
    // File-scope debug state. These are the X360 .bss/.data toggles this TU owns (a debug
    // component keeps its menu-bound flags as file-scope statics so the menu can point at a stable
    // address).
    //
    // ⭐ 2026-09-06 CONSTANT AUDIT: the collision-zone draw radius was 750.0 with the rationale
    // "the X360 .data initial 750.0 (mid-range of the registered 100..1500 SetRange)". The range
    // is right and 750 is indeed its midpoint -- but the midpoint of a range is not a datum. The
    // .data image word at flt_82F307F4 is 0x43480000 == 200.0, and that is the shipped default:
    // OnActivate registers the variable at 0x827B222C (`addi r30, r11, 0x7f4`, r11 = 0x82F30000)
    // then SetRange(100.0 flt_820049E0, 1500.0 flt_820266C4) / SetStep(20.0 flt_820CA5A8); the
    // draw reads the same word at 0x827C7474 (`lfs f0, 0x7f4(r22)`). 200 is inside the range, so
    // nothing ever contradicted 750 -- the overlay just drew a zone disc 3.75x too wide. Second
    // lying instrument in this file: KF_WORLD_TO_SCREEN_SCALE was 0.05 vs the image's 0.2 (a3f05287).
    // ------------------------------------------------------------------------------------------
    namespace
    {
        bool _mbShowPVS            = false;   // byte_8300E116 - "Show PVS"
        bool _mbShowCollisionZones = false;   // byte_8300E117 - "Show collision zones"
        s32  _miColourMode         = 0;       // dword_8300E11C - "Colours" (0 = by player-zone, 1 = by streaming status)
        f32  _mrDrawCollisionRadius = 200.0f; // flt_82F307F4 - "Draw collision zone radius"

        // KI_BOUNDING_SPHERE_SEGMENTS (DWARF :48) - circle tessellation for the collision-zone discs.
        const s32 KI_BOUNDING_SPHERE_SEGMENTS = 32;

        // Bounding-sphere label size (DWARF :47, KR_BOUNDING_SPHERE_TEXT_SIZE). flt_820CA5A8 == 20.0
        // is the X360 scale passed to CalcTextWidth / the label draw.
        const f32 KF_BOUNDING_SPHERE_TEXT_SIZE = 20.0f;

        const CgsDev::RGBA KAU_ZONE_COLOUR_WHEEL[8] =
        {
            0xFF0000FFu,
            0xFF00FF00u,
            0xFFFF0000u,
            0xFFFF00FFu,
            0xFF00FFFFu,
            0xFFFFFF00u,
            0xFFFFFFFFu,
            0xFF0088FFu,
        };

        // INFERRED projection constants. The X360 maps the world XZ plane to the debug overlay with a
        // SINGLE fixed scale (flt_82F30E30, @0x827C74FC) that it applies to BOTH the world->screen
        // projection of the zone centre AND the zone-disc radius (asm: sphereRadius * flt_82F30E30).
        // It centres the overlay on the virtual screen and halves the label width with a single 0.5
        // half-factor (flt_82001DA0, @0x827C73B0 into f30) used in BOTH places.
        // ⭐ THE FLAG IS RETIRED 2026-09-06 (driving-path 1:1 constant audit). The scale magnitude
        // was never inferred-only: flt_82F30E30 is plain initialised image data that reads
        // 0x3E4CCCCD == 0.2, and nothing in the CRT init bank writes it, so the load at
        // 0x827C74FC (`lfs f0, 0xE30(r24)`, r24 = 0x82F30000) takes exactly that. The inferred
        // 0.05 was four times too small, i.e. this overlay drew the whole PVS zone map at a
        // quarter scale.
        const f32 KF_WORLD_TO_SCREEN_SCALE = 0.200000003f;  // flt_82F30E30 (world units AND sphere radius -> overlay pixels)
        const f32 KF_OVERLAY_HALF          = 0.5f;   // flt_82001DA0 (screen-centre origin + label-width halving)

        const char* const KAPC_ASSET_STATUS_NAMES[8] =
        {
            "UNKNOWN",
            "NOT IN LIST",
            "PENDING LOAD",
            "LOADING",
            "READY",
            "PENDING ABORT LOAD",
            "UNLOADING",
            "PENDING UNLOAD",
        };
    }

    // @ 0x827B2108. Bind the world-entity module this component debugs and clear the collision-zone
    // table. The X360's leading bl is CgsDev::DebugComponent::Construct() (an empty COMDAT-folded
    // body @ 0x8284CB38, shared with BaseCollisionGenerator::Destruct - the decompiler attributes
    // the fold to the latter). The table walk zeroes all 256 records (a zeroed 16-byte sphere + a
    // zeroed zone-number word, 0x20 stride); the count, the can-render flag and the wireframe flag
    // are cleared. The `int result`/`return result` in the pseudocode is the register-passed `this`
    // artifact on a void function and is dropped.
    void PVSDebugComponent::Construct(WorldEntityModule* lpWorldEntityModule)
    {
        DebugComponent::Construct();

        mpWorldEntityModule = lpWorldEntityModule;

        mbCanRender        = false;   // +0x2030
        mbDrawPVSWireFrame = false;   // +0x2044

        for (s32 liIndex = 0; liIndex < KI_MAX_NUM_COLLISION_ZONES; ++liIndex)
        {
            maCollisionZones[liIndex].Construct();
        }

        miNumCollisionZones = 0;      // +0x2010
    }

    // @ 0x827B2178. Register the display modes and the live PVS/streaming
    // tunings used by the world update.
    void PVSDebugComponent::OnActivate()
    {
        _miColourMode = 1;
        maColourModeOptions[0].miValue = 1;
        maColourModeOptions[0].mpcName = "Streaming";
        maColourModeOptions[1].miValue = 0;
        maColourModeOptions[1].mpcName = nullptr;

        RegisterVariable(&_mbShowPVS, "Show PVS");
        RegisterVariable(&mbDrawPVSWireFrame, "Show PVS wire frame");
        RegisterVariable(&_miColourMode, "Colours");
        RegisterVariable(&_mbShowCollisionZones, "Show collision zones");
        SetOptions(&_miColourMode, maColourModeOptions);

        RegisterVariable(&_mrDrawCollisionRadius, "Draw collision zone radius");
        SetRange(&_mrDrawCollisionRadius, 100.0f, 1500.0f);
        SetStep(&_mrDrawCollisionRadius, 20.0f);

        RegisterVariable(&mpWorldEntityModule->mPVSModule.mbDebugRestrictZoneLists,
                         "Restrict PVS to 1 zone");

        RegisterVariable(&mfCentreZoneBaseScore, "Centre zone base score");
        SetRange(&mfCentreZoneBaseScore, 0.0f, 200.0f);
        SetStep(&mfCentreZoneBaseScore, 0.5f);

        RegisterVariable(&mfImmediateZoneBaseScore, "Immediate zone base score");
        SetRange(&mfImmediateZoneBaseScore, 0.0f, 200.0f);
        SetStep(&mfImmediateZoneBaseScore, 0.5f);

        RegisterVariable(&mfSecondaryZoneBaseScore, "Secondary zone base score");
        SetRange(&mfSecondaryZoneBaseScore, 0.0f, 200.0f);
        SetStep(&mfSecondaryZoneBaseScore, 0.5f);

        RegisterVariable(&mfDirectionalScoreMultiplier, "Score Multiplier");
        SetRange(&mfDirectionalScoreMultiplier, 0.0f, 10.0f);
        SetStep(&mfDirectionalScoreMultiplier, 0.1f);

        RegisterVariable(&_mbAllowStreamStalling, "Allow Stream Stalling");
        RegisterVariable(&BrnGame::sbShowStreamStallMessage, "Show stall message");
    }

    // @ 0x827CEAD8. The HUD pass: always draw the PVS overlay, then draw the collision-zone overlay
    // only while the "Show collision zones" toggle is set. (The toggle byte_8300E117 is the
    // file-scope _mbShowCollisionZones.) The pseudocode's `return RenderCollisionZones(...)` /
    // `return result` are void tail-call / this-artifacts and are dropped.
    void PVSDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        RenderPVS(lpRender);

        if (_mbShowCollisionZones)
        {
            RenderCollisionZones(lpRender);
        }
    }

    // @ 0x827C6E58. Project every loaded PVS polygon around the current PVS
    // centre, colour it either by response membership or streaming state, and
    // optionally draw the wire/label pass.
    void PVSDebugComponent::RenderPVS(CgsDev::Debug2DImmediateRender* lpRender)
    {
        if (!mbCanRender || !_mbShowPVS)
            return;

        PVSModule& lrPVSModule = mpWorldEntityModule->mPVSModule;
        if (!lrPVSModule.mZoneList.HasMemoryResource())
            return;

        const CgsSceneManager::ZoneList* lpZoneList = lrPVSModule.mZoneList.operator->();
        const CgsSceneManager::Zone* lpZones = lpZoneList->GetZones();
        const u32 luZoneCount = lpZoneList->GetTotalZones();
        const Vector2 lScreenSize = lpRender->GetVirtualScreenSize();
        const Vector2 lScreenCentre = {
            lScreenSize.x * KF_OVERLAY_HALF,
            lScreenSize.y * KF_OVERLAY_HALF,
            0.0f,
            0.0f
        };

        for (u32 luZoneIndex = 0; luZoneIndex < luZoneCount; ++luZoneIndex)
        {
            const CgsSceneManager::Zone& lrZone = lpZones[luZoneIndex];
            const s32 liPointCount = lrZone.GetNumPoints();
            if (liPointCount <= 0 || liPointCount > 32)
                continue;

            Vector2 laPoints[32];
            Vector2 lMinimum = { 100000000.0f, 100000000.0f, 0.0f, 0.0f };
            for (s32 liPoint = 0; liPoint < liPointCount; ++liPoint)
            {
                const Vector2 lWorldPoint = lrZone.GetPoint(static_cast<s16>(liPoint));
                laPoints[liPoint] = {
                    (lWorldPoint.x - mPvsCentrePosition.x) * KF_WORLD_TO_SCREEN_SCALE + lScreenCentre.x,
                    (lWorldPoint.y - mPvsCentrePosition.z) * KF_WORLD_TO_SCREEN_SCALE + lScreenCentre.y,
                    0.0f,
                    0.0f
                };
                if (laPoints[liPoint].x < lMinimum.x) lMinimum.x = laPoints[liPoint].x;
                if (laPoints[liPoint].y < lMinimum.y) lMinimum.y = laPoints[liPoint].y;
            }

            s32 liPvsIndex = -1;
            const s32 liPvsCount = mpWorldEntityModule->mPlayerZoneResponse.GetNumZones();
            for (s32 liIndex = 0; liIndex < liPvsCount; ++liIndex)
            {
                if (lrZone.GetId() == mpWorldEntityModule->mPlayerZoneResponse.GetZoneId(liIndex))
                {
                    liPvsIndex = liIndex;
                    break;
                }
            }

            const InternalBaseStreamer::EAssetStatus leStatus =
                mpWorldEntityModule->mWorldGraphicsStreamer.DebugGetAssetStatus(
                    BrnResource::MakeTrackUnitId(static_cast<u32>(lrZone.GetId())));

            CgsDev::RGBA lFillColour = 0;
            if (_miColourMode == 0)
            {
                if (liPvsIndex == 0)
                    lFillColour = 0x1400FF00u;
                else if (liPvsIndex > 0)
                    lFillColour = 0x14FF0000u;
            }
            else
            {
                switch (leStatus)
                {
                    case InternalBaseStreamer::E_AS_PENDING_LOAD:       lFillColour = 0x140000FFu; break;
                    case InternalBaseStreamer::E_AS_LOADING:            lFillColour = 0x1400FFFFu; break;
                    case InternalBaseStreamer::E_AS_READY:
                    case InternalBaseStreamer::E_AS_UNLOADING:          lFillColour = 0x14FF0000u; break;
                    case InternalBaseStreamer::E_AS_PENDING_ABORT_LOAD: lFillColour = 0x14FF00FFu; break;
                    case InternalBaseStreamer::E_AS_PENDING_UNLOAD:     lFillColour = 0x14FFFF00u; break;
                    default:                                             lFillColour = 0; break;
                }
            }

            if ((lFillColour >> 24) != 0)
                lpRender->DrawSolidConvexPolygon(laPoints, static_cast<u32>(liPointCount), lFillColour);

            if (mbDrawPVSWireFrame)
            {
                lpRender->DrawWirePolygon(laPoints, static_cast<u32>(liPointCount), 0xFFFFFFFFu);

                if (lMinimum.x >= 0.0f && lMinimum.x <= 1000.0f &&
                    lMinimum.y >= 0.0f && lMinimum.y <= 1000.0f)
                {
                    char lacLabel[256];
                    const s32 liStatus = static_cast<s32>(leStatus);
                    const char* lpcStatus =
                        (liStatus >= 0 && liStatus < 8) ? KAPC_ASSET_STATUS_NAMES[liStatus] : "UNKNOWN";
                    CgsCore::SPrintf(lacLabel, sizeof(lacLabel), "%d: %s",
                                     static_cast<s32>(static_cast<u32>(lrZone.GetId())), lpcStatus);
                    lpRender->DrawText(lacLabel, lMinimum.x, lMinimum.y, 15.0f, 0xFFFFFFFFu);
                }
            }
        }
    }

    // @ 0x827C7378. Draw every registered collision zone as a labelled bounding-sphere disc on the
    // top-down debug overlay, then draw the PVS-centre marker.
    //
    // The X360 hand-vectorises the per-zone math; reconstructed by behaviour here. For each of the
    // miNumCollisionZones records it:
    //   1. forms delta = zoneSphereCentre - mPvsCentrePosition and its length (the asm's
    //      vmsum3fp/vrsqrtefp Newton sequence == a length() of the 3D delta);
    //   2. culls the zone unless _mrDrawCollisionRadius > length (only zones within the draw radius
    //      of the PVS centre are shown - flt_82F307F4 is _mrDrawCollisionRadius);
    //   3. projects the zone centre to the overlay (worldXZ scaled by KF_WORLD_TO_SCREEN_SCALE,
    //      offset by the screenSize*0.5 overlay origin) and draws a KI_BOUNDING_SPHERE_SEGMENTS
    //      circle of screen radius = zoneRadius * KF_WORLD_TO_SCREEN_SCALE (the same scale),
    //      coloured by zoneNumber % 8 from the colour wheel;
    //   4. prints the zone number ("%d") and draws it centred on the disc (the X360 nudges the label
    //      left by half its CalcTextWidth and up by half KF_BOUNDING_SPHERE_TEXT_SIZE).
    // The X360 `MaybeDrawText(render, text, .., x, y, scale, .., colour, ..)` is
    // Debug2DImmediateRender::DrawText(text, x, y, scale, RGBA); the extra register args are
    // decompiler artifacts (confirmed against the sibling RoadRules/Trigger reconstructions).
    void PVSDebugComponent::RenderCollisionZones(CgsDev::Debug2DImmediateRender* lpRender)
    {
        // Overlay origin: centre the map on the virtual screen (screenSize * 0.5). The X360 reads the
        // render's virtual screen size at render+0x34/+0x38 (GetVirtualScreenSize) and multiplies by
        // flt_82001DA0 == 0.5 -- vmulfp128 v123, screenSize, splat(0.5) @0x827C7424.
        const Vector2 lScreenSize   = lpRender->GetVirtualScreenSize();
        const f32     lfScreenOffX  = lScreenSize.x * KF_OVERLAY_HALF;
        const f32     lfScreenOffY  = lScreenSize.y * KF_OVERLAY_HALF;

        for (s32 liIndex = 0; liIndex < miNumCollisionZones; ++liIndex)
        {
            const CollisionZone& lrZone     = maCollisionZones[liIndex];
            const Vector3        lZoneCentre = lrZone.GetSpherePosition();

            // Distance from the PVS centre to this zone (3-component length of the delta).
            const f32 lfDeltaX   = lZoneCentre.x - mPvsCentrePosition.x;
            const f32 lfDeltaY   = lZoneCentre.y - mPvsCentrePosition.y;
            const f32 lfDeltaZ   = lZoneCentre.z - mPvsCentrePosition.z;
            const f32 lfDistance = sqrtf(lfDeltaX * lfDeltaX + lfDeltaY * lfDeltaY + lfDeltaZ * lfDeltaZ);

            // Only draw zones inside the configured draw radius of the PVS centre.
            if (_mrDrawCollisionRadius <= lfDistance)
            {
                continue;
            }

            // Project the zone centre (world XZ) to the overlay and draw the bounding-sphere disc.
            const Vector2 lScreenPos =
            {
                (lZoneCentre.x - mPvsCentrePosition.x) * KF_WORLD_TO_SCREEN_SCALE + lfScreenOffX,
                (lZoneCentre.z - mPvsCentrePosition.z) * KF_WORLD_TO_SCREEN_SCALE + lfScreenOffY,
                0.0f,
                0.0f,
            };

            const s32          liZoneNumber  = lrZone.GetZoneNumber();
            const f32          lfScreenRadius = lrZone.GetSphereRadius() * KF_WORLD_TO_SCREEN_SCALE;
            const CgsDev::RGBA lColour        = KAU_ZONE_COLOUR_WHEEL[liZoneNumber % 8];

            lpRender->DrawCircle(lScreenPos, lfScreenRadius, KI_BOUNDING_SPHERE_SEGMENTS, lColour);

            // Label the disc with the zone number, centred on the disc.
            char lacLabel[8];
            CgsCore::SPrintf(lacLabel, sizeof(lacLabel), "%d", liZoneNumber);

            const f32     lfLabelWidth = lpRender->CalcTextWidth(lacLabel, KF_BOUNDING_SPHERE_TEXT_SIZE);
            const Vector2 lLabelPos =
            {
                lScreenPos.x - lfLabelWidth * KF_OVERLAY_HALF,
                lScreenPos.y - KF_BOUNDING_SPHERE_TEXT_SIZE * 0.5f,
                0.0f,
                0.0f,
            };

            lpRender->DrawText(lacLabel, lLabelPos.x, lLabelPos.y, KF_BOUNDING_SPHERE_TEXT_SIZE, lColour);
        }

        RenderPvsCentrePosition(lpRender);
    }

    // @ 0x827BFB08. The original marker is a 10-by-20 white rectangle centred
    // on the virtual screen.
    void PVSDebugComponent::RenderPvsCentrePosition(CgsDev::Debug2DImmediateRender* lpRender)
    {
        const Vector2 lScreenSize = lpRender->GetVirtualScreenSize();
        const Vector2 lMinimum = {
            lScreenSize.x * 0.5f - 5.0f,
            lScreenSize.y * 0.5f - 10.0f,
            0.0f,
            0.0f
        };
        const Vector2 lMaximum = {
            lScreenSize.x * 0.5f + 5.0f,
            lScreenSize.y * 0.5f + 10.0f,
            0.0f,
            0.0f
        };
        lpRender->DrawBox(lMinimum, lMaximum, 0xFFFFFFFFu);
    }
}
