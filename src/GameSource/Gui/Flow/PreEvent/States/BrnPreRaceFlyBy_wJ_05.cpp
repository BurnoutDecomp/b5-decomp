// ===================================================================================
// BrnGui::PreRaceFlyByState -- wave-J partfile 05: the two VMX workers.
//   FindEventDirection  @0x824B4EC8  (DWARF BrnPreRaceFlyBy.cpp:1596)
//   CalculateZoomFactor @0x824BE8F0  (DWARF BrnPreRaceFlyBy.cpp:1763)
//
// Both bodies were reconstructed from the raw X360 assembly
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x824B4EC8.json and /0x824BE8F0.json, field
// `assembly`), arbitrated over the Hex-Rays pseudocode -- which for these two is mostly
// unreadable `__asm` soup and, for FindEventDirection, is actively WRONG (it renders the
// degree conversion as the constant 57.29578 instead of angle * 57.29578).
//
// (CalculateZoomFactor was parked while GuiCache::GetEventID / GetLandmarkInfoFromID and
// RaceEventData's checkpoint accessors were missing; all four have landed since, and the
// necessary-and-sufficient set was proven with a shadow-include compile probe --
// scratchpad/waveJ/probe_g05/, built with the gate's own flags: clean.)
//
// ALL VMX CONSTANTS BELOW WERE DUMPED FROM THE IMAGE with headless IDA
// (scratchpad/waveJ/dump_g05_consts.py -> scratchpad/waveJ/g05_consts.txt), not inferred:
//   flt_82067490 = 0.41421398520469666  (tan 22.5 degrees -- the half-sector offset)
//   flt_820037C8 = -1.0     flt_82001CC0 = 0.0     flt_82004928 = 360.0
//   flt_820652A8 = 57.295780181884766   (radians -> degrees)
//   flt_8206748C = 0.02222222276031971  (1 / 45)
//   unk_820652B0 = {6.2831854820251465, ...}  (2*pi; the lvlx + vspltw takes lane 0)
//   unk_82181510 = {0.0, 1.0, 0.0, 0.0}       (the +Y axis, the cross-product sign probe)
// ===================================================================================

#include "GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.h"

#include <cmath>                                            // std::acos / std::sqrt / std::floor
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"                      // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"              // GuiEventUpdateSatNav::SatNavIconInfo
#include "GameSource/Gui/BrnGuiShared.h"                     // BrnGui::ECompassPoints (E_COMPASS_POINTS_COUNT)
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"
#include "GameSource/GameState/BrnGameStateTypes.h"          // BrnGameState::LandmarkIndex (complete: passed by value)
#include "SharedClasses/Progression/BrnRaceEventData.h"      // BrnProgression::RaceEventData (+ CheckpointData)
#include "GameSource/Gui/BrnGuiWorldDataController.h"        // GetEventInfoFromEventId
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"            // BrnGui::MapTransform

namespace GSM = BrnGameState::GameStateModuleIO;

namespace BrnGui
{

namespace
{
    // The reference direction FindEventDirection measures every event bearing against.
    // The X360 stack-builds it lane by lane from three rodata literals
    // (flt_82067490 / flt_82001CC0 / flt_820037C8) -- there is no rodata vector and no
    // class static for it, so it is reproduced here as a file-local constant.
    // 0.41421399 == tan(22.5 degrees): rotating the -Z map "north" by half a 45-degree
    // sector is what makes the later floor(degrees / 45) land each bearing in the middle
    // of its compass sector instead of on the boundary.
    // FLAG: the CONSTANT NAME is ours (consumer-derived); the three VALUES are image reads.
    const f32 KF_SECTOR_REFERENCE_X = 0.41421399f;
    const f32 KF_SECTOR_REFERENCE_Y = 0.0f;
    const f32 KF_SECTOR_REFERENCE_Z = -1.0f;

    // rodata scalars the bearing math loads (all image reads; names ours).
    const f32 KF_TWO_PI              = 6.2831853f;   // unk_820652B0 lane 0
    const f32 KF_RADIANS_TO_DEGREES  = 57.29578f;    // flt_820652A8
    const f32 KF_DEGREES_PER_TURN    = 360.0f;       // flt_82004928
    const f32 KF_ONE_OVER_SECTOR_DEG = 0.022222223f; // flt_8206748C == 1/45

    // flt_82F27384 -- the 16:9 base aspect the shared map-zoom solver is called
    // against. IMAGE READ, not inferred: headless IDA dumped it as 1.7777777910232544
    // (scratchpad/waveJ/dump_g05_consts.py -> scratchpad/waveJ/g05_consts.txt).
    const f32 KF_MAP_BASE_ASPECT_RATIO = 1.7777778f;

    // ---- the three 3-lane vector ops the X360 emits as VMX128 ----
    //
    // The console does all of this in registers: vmsum3fp128 for the dot products,
    // vrsqrtefp + one Newton-Raphson step (vmulfp/vnmsubfp/vmaddfp) for the reciprocal
    // square root, and a vpermwi128 0x63 (the yzx lane rotate) pair for the cross
    // product. None of that has a portable PC equivalent and the refinement step only
    // exists to recover precision the estimate instruction throws away, so -- exactly as
    // rw/math/vpu/types.h and BrnMapUtils.h already state for this codebase -- these are
    // reconstructed at the SEMANTIC level with scalar math, not transliterated.
    // (Reconstruction-local helpers: the X360 has no out-of-line calls for them.)

    f32 Dot3(const Vector3& lv3A, const Vector3& lv3B)
    {
        return (lv3A.x * lv3B.x) + (lv3A.y * lv3B.y) + (lv3A.z * lv3B.z);
    }

    Vector3 Cross3(const Vector3& lv3A, const Vector3& lv3B)
    {
        const Vector3 lv3Result = { (lv3A.y * lv3B.z) - (lv3A.z * lv3B.y),
                                    (lv3A.z * lv3B.x) - (lv3A.x * lv3B.z),
                                    (lv3A.x * lv3B.y) - (lv3A.y * lv3B.x),
                                    0.0f };
        return lv3Result;
    }

    Vector3 Normalise3(const Vector3& lv3In)
    {
        const f32 lfInverseLength = 1.0f / std::sqrt(Dot3(lv3In, lv3In));
        const Vector3 lv3Result = { lv3In.x * lfInverseLength,
                                    lv3In.y * lfInverseLength,
                                    lv3In.z * lfInverseLength,
                                    0.0f };
        return lv3Result;
    }
}

// -------------------------------------------------------------------------------------
// FindEventDirection @ 0x824B4EC8 (DWARF cpp:1596) -- which of the eight compass sectors
// the event's destination landmark lies in, as seen from the current world camera.
//
// Notes taken from the asm rather than the pseudocode:
//  * The two asserts are cpp:1601 (`li r5, 0x641`) and cpp:1604 (`li r5, 0x644`); the
//    second fires when the game mode is none of {0, 5, 8} -- the three the assert string
//    names (OFFLINE_RACE / BURNING_ROUTE / MARKED_MAN).
//  * `lvx128 v127, r4, 0x4AE0` is the committed GuiCache::GetWorldCameraPosition() far
//    member. The console offset 0x4AE0 is NOT reproduced -- the accessor is called by
//    name so the host's own layout applies.
//  * `lhz r4, var_A0(r1)` after the sret call reads the FIRST halfword of the returned
//    BrnGameState::LandmarkIndex (big-endian, hence Hex-Rays' `HIWORD`); on the host the
//    value is simply passed by value into GetLandmarkInfoFromIndex.
//  * Operation ORDER is the asm's: normalise both -> dot -> clamp -> acos -> cross-product
//    sign probe -> radians-to-degrees -> wrap -> floor. (The pseudocode's `v20 = 57.29578`
//    is a Hex-Rays artefact of the splat-and-store round trip; the asm multiplies the
//    ANGLE by 57.29578 at 0x824B50A8 and reloads lane 0 at 0x824B50B4.)
//  * CLAMP POLARITY: `vmaxfp v0, v0, -1` then `vminfp v1, v0, 1` are select-style ops
//    (max(a,b) == a > b ? a : b), so a NaN dot falls through to the OTHER operand. The
//    ternaries below reproduce that exactly; `if (x < -1.0f) x = -1.0f;` would NOT.
//  * NaN POLARITY on the two wrap loops: the console guards them with `bge` (0x824B50C0)
//    and `ble` (0x824B50D4), both of which are TAKEN when unordered, i.e. a NaN skips
//    both loops. `while (deg < 0)` and `while (deg > 360)` are the matching ordered
//    predicates -- both false for NaN -- so the plain C++ shape is already correct here.
//  * The sign probe is `vcmpgtfp. v0, {0,0,0,0}, crossY`, an ORDERED greater-than: NaN
//    leaves the angle alone. `crossY < 0.0f` matches.
    ECompassPoints PreRaceFlyByState::FindEventDirection()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1601

        CGS_ASSERT((mpGuiCache->GetGameMode() == GSM::E_MODE_OFFLINE_RACE)
                       || (mpGuiCache->GetGameMode() == GSM::E_MODE_BURNING_ROUTE)
                       || (mpGuiCache->GetGameMode() == GSM::E_MODE_MARKED_MAN),
                   "(mpGuiCache->GetGameMode() == GsmIO::E_MODE_OFFLINE_RACE) || "
                   "(mpGuiCache->GetGameMode() == GsmIO::E_MODE_BURNING_ROUTE) || "
                   "(mpGuiCache->GetGameMode() == GsmIO::E_MODE_MARKED_MAN)");   // cpp:1604

        const Vector4& lv4CameraPosition = mpGuiCache->GetWorldCameraPosition();

        const Vector3 lv3Reference = { KF_SECTOR_REFERENCE_X,
                                       KF_SECTOR_REFERENCE_Y,
                                       KF_SECTOR_REFERENCE_Z,
                                       0.0f };

        GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
        mpGuiCache->GetLandmarkInfoFromIndex(mpGuiCache->GetEventDestinationLandmarkIndex(),
                                             &lLandmarkInfo);

        // The icon record's leading 16-byte lane is the landmark's world position -- the
        // `lvx128 v13, r0, <info>` at 0x824B4FC0.
        const Vector4& lv4LandmarkPosition = lLandmarkInfo.GetPositionLane();
        const Vector3 lv3ToLandmark = { lv4LandmarkPosition.x - lv4CameraPosition.x,
                                        lv4LandmarkPosition.y - lv4CameraPosition.y,
                                        lv4LandmarkPosition.z - lv4CameraPosition.z,
                                        0.0f };

        const Vector3 lv3ReferenceDirection = Normalise3(lv3Reference);
        const Vector3 lv3LandmarkDirection  = Normalise3(lv3ToLandmark);

        f32 lfCosAngle = Dot3(lv3ReferenceDirection, lv3LandmarkDirection);
        lfCosAngle = (lfCosAngle > -1.0f) ? lfCosAngle : -1.0f;   // vmaxfp
        lfCosAngle = (lfCosAngle <  1.0f) ? lfCosAngle :  1.0f;   // vminfp

        f32 lfAngle = std::acos(lfCosAngle);                      // bl XMVectorACos

        // acos only ever returns [0, pi], so the half-turn the bearing actually lies in is
        // recovered from the sign of the cross product's Y component -- the console dots
        // the cross with unk_82181510 == the +Y axis and tests it against zero.
        const Vector3 lv3Cross = Cross3(lv3ReferenceDirection, lv3LandmarkDirection);
        if (lv3Cross.y < 0.0f)
            lfAngle = KF_TWO_PI - lfAngle;

        f32 lfDegrees = lfAngle * KF_RADIANS_TO_DEGREES;
        while (lfDegrees < 0.0f)
            lfDegrees += KF_DEGREES_PER_TURN;
        while (lfDegrees > KF_DEGREES_PER_TURN)
            lfDegrees -= KF_DEGREES_PER_TURN;

        // 360 degrees / 8 compass points == one 45-degree sector per point.
        const s32 liEventDirection =
            static_cast<s32>(std::floor(lfDegrees * KF_ONE_OVER_SECTOR_DEG));

        // cpp:1648 -- `cmpwi cr6, r31, 8` / `blt`. The 8 is ECompassPoints'
        // E_COMPASS_POINTS_COUNT, which BrnGuiShared.h now defines, so the enumerator is
        // named rather than the measured literal spelled out.
        CGS_ASSERT(liEventDirection < E_COMPASS_POINTS_COUNT,
                   "E_COMPASS_POINTS_COUNT > leEventDirection");

        return static_cast<ECompassPoints>(liEventDirection);
    }

// -------------------------------------------------------------------------------------
// CalculateZoomFactor @ 0x824BE8F0 (DWARF cpp:1763) -- fit the whole event (its start
// position plus every checkpoint) into the pre-race map view: accumulate the {x, z}
// bounding rectangle of those world positions, centre the map on it, and hand the
// rectangle to the shared map-zoom solver.
//
// Notes taken from the asm rather than the pseudocode (the pseudocode is `__asm` soup and
// drops all four arguments of the tail call):
//  * `bl sub_824F8AF0` @0x824BE914 is the committed GuiCache::GetProfileEventDisplayInfo
//    (r3 = the cache, r4 = the event id) -- see BrnGuiCache.h.
//  * The "mpWorldDataController" assert at 0x824BE92C carries the file string
//    GameSource/Gui/BrnGuiCache... and line 0x914 == 2324, i.e. it belongs to
//    GuiCache::GetWorldDataController(), which the X360 INLINES here (two direct
//    `lwz r11, 0x4064(r30)` loads). It is NOT reproduced at this call site: calling the
//    committed accessor by name brings its own assert with it.
//  * The two asserts this function does own are cpp:1781 (`li r5, 0x6F5`, "lpEventStart")
//    and cpp:1782 (`li r5, 0x6F6`, "lpRaceEventData"). Note the ORDER: the console fetches
//    BOTH records first and only then null-checks them, so a null display record does not
//    short-circuit the event-info lookup.
//  * The checkpoint bounds assert inside the loop is BrnRaceEventData.h:953 (`li r5,
//    0x3B9`) -- RaceEventData::GetCheckpointData's own assert, inlined by the console
//    alongside the raw `*(base + i * 0x28)` load. De-inlined here to the named accessor,
//    which carries that assert.
//  * CONSOLE OFFSETS NOT REPRODUCED: +0x9E5C (event id), +0x4064 (world-data controller),
//    +0x18/+0x1C (checkpoint array/count), the 0x28 checkpoint stride, and the
//    `stvx128 v0, r26, 0xFF0` centre store (0x9A0 mMainMapComponent + 0x650) are all
//    reached by name on the host, so the host's own layout applies.
//  * The `vperm` with mask unk_82CDA450 = {00 01 02 03 | 18 19 1A 1B | 00 01 02 03 |
//    00 01 02 03} (image read) takes a world position to the 2D map plane as
//    {x, z, x, x}; only lanes 0 and 1 are ever read back, so the reconstruction keeps a
//    plain 2-lane Vector2 {x = world X, y = world Z}.
//  * SELECT POLARITY: `vminfp v1, v12, v0` / `vmaxfp v2, v11, v0` are select-style ops
//    (min(a,b) == a < b ? a : b) with the RUNNING bound as operand a, so a NaN world
//    coordinate replaces the running bound rather than being rejected. The ternaries
//    below reproduce that exactly; std::min/std::max would not.
//  * `vcmpgtfp. v0, height, width` @0x824BEAD0 is an ORDERED greater-than, and the branch
//    at 0x824BEB0C takes the LONG rect when the bit is clear -- so `height > width`
//    (false for NaN -> LONG) is the matching C++ predicate.
//  * The `vrefp` + two Newton-Raphson steps on the splatted 2.0 (flt_82065670, image read)
//    is just a reciprocal: the centre is (min + max) * 0.5. The refinement is precision
//    recovery for the estimate instruction and is deliberately NOT transliterated (same
//    policy as rw/math/vpu/types.h and BrnMapUtils.h).
//  * The two display rects hold IDENTICAL values ({638.0f, 349.79999f}, recovered from
//    their runtime initialiser stubs), so the branch is value-neutral; it is kept because
//    the binary has it. Which of the two addresses is LONG and which is TALL is an
//    INFERENCE from initialiser order == declaration order (see BrnPreRaceFlyBy.h) --
//    0x82FB4AA0 is taken on the taller-than-wide arm, which is why it is read as TALL.
// -------------------------------------------------------------------------------------
    f32 PreRaceFlyByState::CalculateZoomFactor()
    {
        const u32 luEventId = mpGuiCache->GetEventID();

        const SatNavEventDisplayInfo* const lpEventStart =
            mpGuiCache->GetProfileEventDisplayInfo(luEventId);

        const BrnProgression::RaceEventData* const lpRaceEventData =
            mpGuiCache->GetWorldDataController()->GetEventInfoFromEventId(luEventId);

        CGS_ASSERT(lpEventStart != 0, "lpEventStart");           // cpp:1781
        CGS_ASSERT(lpRaceEventData != 0, "lpRaceEventData");     // cpp:1782

        // Seed the bounding rectangle with the event's start position, flattened onto the
        // map plane ({x, z}).
        Vector2 lv2Min = { lpEventStart->mv3Position.x, lpEventStart->mv3Position.z, 0.0f, 0.0f };
        Vector2 lv2Max = lv2Min;

        for (s32 liCheckpointIndex = 0;
             liCheckpointIndex < lpRaceEventData->GetCheckpointCount();
             ++liCheckpointIndex)
        {
            GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
            mpGuiCache->GetLandmarkInfoFromID(
                lpRaceEventData->GetCheckpointData(liCheckpointIndex)->GetLandmarkId(),
                &lLandmarkInfo);

            const Vector4& lv4Position = lLandmarkInfo.GetPositionLane();
            const Vector2 lv2Point = { lv4Position.x, lv4Position.z, 0.0f, 0.0f };

            lv2Min.x = (lv2Min.x < lv2Point.x) ? lv2Min.x : lv2Point.x;   // vminfp
            lv2Min.y = (lv2Min.y < lv2Point.y) ? lv2Min.y : lv2Point.y;
            lv2Max.x = (lv2Max.x > lv2Point.x) ? lv2Max.x : lv2Point.x;   // vmaxfp
            lv2Max.y = (lv2Max.y > lv2Point.y) ? lv2Max.y : lv2Point.y;
        }

        const f32 lfWidth  = lv2Max.x - lv2Min.x;
        const f32 lfHeight = lv2Max.y - lv2Min.y;

        const Vector2 lv2Centre = { (lv2Max.x + lv2Min.x) * 0.5f,
                                    (lv2Max.y + lv2Min.y) * 0.5f,
                                    0.0f, 0.0f };
        mMainMapComponent.SetDesiredWorldCentre(lv2Centre);

        const Vector2& lv2DisplayRect = (lfHeight > lfWidth) ? K_PRERACE_TALL_DISPLAY_RECT
                                                             : K_PRERACE_LONG_DISPLAY_RECT;

        return MapTransform::CalculateZoomFactor(lv2Min, lv2Max, lv2DisplayRect,
                                                 KF_MAP_BASE_ASPECT_RATIO);
    }
}
