// Bodies for the streaming-PVS debug component, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   PVSDebugComponent::Construct               @ 0x827B2108   (bodied here)
//   PVSDebugComponent::RenderHUD               @ 0x827CEAD8   (bodied here)
//   PVSDebugComponent::RenderCollisionZones    @ 0x827C7378   (bodied here)
//   PVSDebugComponent::OnActivate              @ 0x827B2178   (declared; deferred - see note)
//   PVSDebugComponent::RenderPVS               @ 0x827C6E58   (declared; deferred - see note)
//   PVSDebugComponent::RenderPvsCentrePosition @ 0x827BFB08   (declared; deferred - see note)
//
// DEFERRED FUNCTIONS. Three of this component's six functions reach through types that are not
// yet reconstructed in b5-decomp and cannot be forked locally without violating the no-raw-offset
// rule:
//   * OnActivate registers the PVS-module score-tuning globals (flt_82CDB58C.. in another TU's
//     rodata) and a WorldEntityModule "Restrict PVS to 1 zone" flag reached at module+0x2610;
//     BrnWorld::WorldEntityModule has no reconstructed class home with that accessor yet.
//   * RenderPVS walks the loaded ZoneList obtained from mpWorldEntityModule (module+0x23D8) and a
//     PVS-module-internal compiled-zone format (48-byte records, NOT CgsSceneManager::Zone), then
//     queries the graphics streamer (module+0x4990) via InternalBaseStreamer::DebugGetAssetStatus.
//     Those module/PVS internals are not reconstructed.
//   * RenderPvsCentrePosition calls an as-yet-unrecovered CgsDev::Debug2DImmediateRender marker
//     primitive (X360 sub_8281C3E0) that is not declared on the committed render header.
// They are declared in the header (RenderHUD/RenderCollisionZones call them through those
// declarations, so the per-TU `cl /c` gate is satisfied) and their bodies land when the
// WorldEntityModule / PVS-module / render-marker homes exist.

#include "GameSource/World/DebugComponents/BrnPVSDebugComponent.h"

#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h" // Debug2DImmediateRender (DrawCircle / DrawText / CalcTextWidth)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                      // CgsCore::SPrintf

#include <cmath>   // sqrtf (the X360's vmsum3fp + vrsqrtefp Newton sequence == a 3D length)

namespace BrnWorld
{
    // ------------------------------------------------------------------------------------------
    // File-scope debug state. These are the X360 .bss/.data toggles this TU owns (a debug
    // component keeps its menu-bound flags as file-scope statics so the menu can point at a stable
    // address). The collision-zone draw radius defaults to the X360 .data initial 750.0 (mid-range
    // of the registered 100..1500 SetRange in OnActivate).
    // ------------------------------------------------------------------------------------------
    namespace
    {
        bool _mbShowPVS            = false;   // byte_8300E116 - "Show PVS"
        bool _mbShowCollisionZones = false;   // byte_8300E117 - "Show collision zones"
        s32  _miColourMode         = 0;       // dword_8300E11C - "Colours" (0 = by player-zone, 1 = by streaming status)
        f32  _mrDrawCollisionRadius = 750.0f; // flt_82F307F4 - "Draw collision zone radius"

        // KI_BOUNDING_SPHERE_SEGMENTS (DWARF :48) - circle tessellation for the collision-zone discs.
        const s32 KI_BOUNDING_SPHERE_SEGMENTS = 32;

        // Bounding-sphere label size (DWARF :47, KR_BOUNDING_SPHERE_TEXT_SIZE). flt_820CA5A8 == 20.0
        // is the X360 scale passed to CalcTextWidth / the label draw.
        const f32 KF_BOUNDING_SPHERE_TEXT_SIZE = 20.0f;

        // INFERRED DATA TABLE: the X360 colours each collision-zone disc by zone-number modulo 8 from
        // an 8-entry .rdata RGBA wheel (dword_82F30DFC). The raw bytes are not in the available X360
        // exports, so the wheel below is a plausible 8-hue debug palette (packed 0xAARRGGBB, fully
        // opaque). FLAGGED: the per-hue values are a reconstruction guess; the INDEXING
        // (zoneNumber % 8) and the use as the DrawCircle colour are confirmed from the asm.
        const CgsDev::RGBA KAU_ZONE_COLOUR_WHEEL[8] =
        {
            0xFFFF0000u, // red
            0xFF00FF00u, // green
            0xFF0000FFu, // blue
            0xFFFFFF00u, // yellow
            0xFFFF00FFu, // magenta
            0xFF00FFFFu, // cyan
            0xFFFF8000u, // orange
            0xFFFFFFFFu, // white
        };

        // INFERRED projection constants. The X360 maps the world XZ plane to the debug overlay with a
        // SINGLE fixed scale (flt_82F30E30, @0x827C74FC) that it applies to BOTH the world->screen
        // projection of the zone centre AND the zone-disc radius (asm: sphereRadius * flt_82F30E30).
        // It centres the overlay on the virtual screen and halves the label width with a single 0.5
        // half-factor (flt_82001DA0, @0x827C73B0 into f30) used in BOTH places. Only the 0.05 scale
        // magnitude is inferred (FLAGGED); the 0.5 is exact and the control flow (range cull,
        // projection shape, draw order) is confirmed from the asm.
        const f32 KF_WORLD_TO_SCREEN_SCALE = 0.05f;  // flt_82F30E30 (world units AND sphere radius -> overlay pixels)
        const f32 KF_OVERLAY_HALF          = 0.5f;   // flt_82001DA0 (screen-centre origin + label-width halving)
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
}
