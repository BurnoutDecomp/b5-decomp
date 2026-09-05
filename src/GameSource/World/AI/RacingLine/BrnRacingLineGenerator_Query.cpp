// =================================================================================================
// RacingLine/BrnRacingLineGenerator_Query.cpp  (aiwave2 lane R2, 2026-09-05)
//
// Partfile of RacingLine/BrnRacingLineGenerator.cpp: the QUERY half of BrnAI::RacingLineGenerator
// -- the five members the AIDriver / SteeringFan call once the section cache exists, plus the four
// DWARF-declared helpers the console inlined into them.
//
//   BODIED (exported)
//     InitialiseRacingLine         @0x8278FB20   (AIDriver::InitialiseRacingLine @0x82792DF0)
//     GetPointFarAhead             @0x827900A0   (SteeringFan::CachePointAhead, AIDriver::
//                                                 FindPositionInFuture / FindFinalDriftDirection)
//     GetRoadPositionAsPercentage  @0x8278FFB0   (AIDriver::HardShoulderSpeed @0x827930B8)
//     SpreadHNGBackOneStep         @0x8278F680   (AIDriver::DoRoundRobinWork  @0x82796340)
//     RaceLineDefaultsToSlamPlayer @0x82777730   (AIDriver::GenerateRacingLine @0x8277C4E8)
//   BODIED (no export -- recovered from the inlined blocks inside the five above, written ONCE)
//     GetPerpendicularDistanceToCentreLine     (DWARF BrnRacingLineGenerator.h:102) -- inlined
//                                  twice, at 0x8278FFDC..0x8279004C and 0x827777D8..0x82777814
//     FinishedSpreadingBack                    (:363) -- inlined THREE times inside
//                                  SpreadHNGBackOneStep (0x8278F768, 0x8278F7A8, 0x8278F82C)
//     HasSpreadHardNoGoLinesFinished           (:368) -- the 32-entry scan @0x8278F7F4..0x8278F824
//     SpreadHardNoGoLinesThroughCurrentSection (:372) -- the miBackwardsStep == 0 arm
//                                  @0x8278F70C..0x8278F740
//   PARKED (see the park banner at the foot of this file)
//     RenderHardNoGoMap @0x82790278, DrawCentreLine (:203), RenderSectionDetails (:208),
//     FindTargetFixedDistanceAhead (:96), FindAnyTargetOnLineAhead (:248),
//     RaceDefaultsLineToAutoCentre (:107), RaceLineDefaultsToHoldingRoadPosition (:117),
//     MoveRaceLineToSlamPlayer (:122).
//
// (RESOLVED 2026-09-05: the class is now taken from BrnRacingLineGenerator.h; the note below is historical) THE CLASS WAS DECLARED TU-LOCALLY, exactly the idiom the sibling
// partfile RacingLine/BrnRacingLineGenerator_GetForwardPortalIndex.cpp already uses. The host
// RacingLine/BrnRacingLineGenerator.h (lane R1's file this wave) declares this lane's five entry
// points but NOT the section-cache helpers they call, and this lane owns no header -- so including
// it would not compile. Every declaration below is the DecFIGS DWARF prototype verbatim
// (references/DecFIGS/dwarfdump/.../BrnRacingLineGenerator.h) at the DWARF's OWN access, because
// MSVC mangles member access into the symbol name. DELETE-WHEN lane R1's header grow lands:
// replace this block with `#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"`.
//
// SOURCE FILE / LINE NUMBERS: every assert this TU reproduces cites
// "d:\p4\b5_main\burnout\main\code\gamesource\unity\../World/AI/RacingLine/BrnRacingLineGenerator.cpp"
// -- the console line number is quoted at each site. The X360 retail build DOES emit these
// (BeginAssert / FireAssert / EndAssert are in the listing), so they are reproduced; the
// StrStream-formatted asserts elsewhere in this class are compiled out and are not.
// =================================================================================================

#include <cmath>

#include "types.hpp"
#include "BrnCommonTypes.h"                                               // Vector2 / Vector3 (vpu)
#include "rw/math/vpu/vector3_operation.h"                            // rw::math::vpu::IsValid
#include "GameSource/World/AI/Route/BrnRacingLine.h"                      // RacingLine + SectionData
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"     // RacingLineGenerator (lane R1 header)
#include "GameSource/World/AI/BrnAIUtils.h"                                // BrnAI::DistancePointToLine
#include "GameSource/World/AI/Route/BrnRoute.h"                           // Route::GetNodeCount/GetStatus
#include "GameSource/World/AI/BrnAICar.h"                                 // AICar::GetPosition / GetRoute
#include "SharedClasses/AI/AISectionsResourceType.h"                      // AISectionsData / AISection
#include "GameSource/Math/BrnMathUtils.h"                                 // BrnMath::Flatten
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // AddMonitor/Start/Stop

namespace BrnAI
{
// (The TU-local RacingLineGenerator declaration that stood here was replaced by the real header
//  once lane R1 landed it -- conductor, 2026-09-05.)

// (RESOLVED 2026-09-05: declared in BrnAIUtils.h, bodied in BrnAIUtils.cpp) BrnAI::DistancePointToLine -- DWARF GameSource/World/AI/BrnAIUtils.h:7,
// X360 @0x8276DDB8 (its own assert cites "..\..\..\GameSource\World/AI/BrnAIUtils.cpp":117,
// "Bad maths! Point = ..., Start = ..., End = ..."). The host BrnAIUtils.h deliberately declares
// only the helpers already homed, and this lane owns no header, so it is declared here.
// It has NO BODY anywhere in the tree yet -- see this lane's report (`## risks`), which carries
// the fully decoded body for BrnAIUtils.cpp. DELETE-WHEN it is declared in BrnAIUtils.h.

namespace
{
    // AddMonitor page argument: `li r4, 7` at every one of the six call sites in
    // InitialiseRacingLine. CgsDev::PerfMonCpuPage only spells out the pages earlier waves
    // needed; 7 is inside the enum's range (E_PMP_MAX == 24), so the cast is well defined.
    // [FLAG header_request] add `E_PMP_7 = 7` to CgsPerfMonCpu.h and use it here instead.
    const CgsDev::PerfMonCpuPage KE_RACING_LINE_PERFMON_PAGE =
        static_cast<CgsDev::PerfMonCpuPage>(7);

    // AddMonitor CPU budgets, read from the image: flt_820C4150 == 10.0f for the top-level
    // "Generate Racing Line" monitor, flt_820C41F4 == 2.0f for the other five.
    const f32 KF_PERFMON_BUDGET_GENERATE = 10.0f;
    const f32 KF_PERFMON_BUDGET_STEP     = 2.0f;

    // `cmpwi r11, 0x10` in InitialiseRacingLine @0x8278FDC8: at most sixteen route nodes are ever
    // cached, i.e. KI_RACING_LINE_MAX_AVAILABLE_SECTIONS.
    const s32 KI_MAX_SECTIONS_TO_CACHE = KI_RACING_LINE_MAX_AVAILABLE_SECTIONS;

    // `cmpwi r11, 7` / `bgt` @0x8278FF84: InitialiseRacingLine stops setting up targets once it
    // has done eight non-junction sections in one call (later frames finish the rest).
    const s32 KI_MAX_NON_JUNCTION_SETUPS = 7;

    // `cmpwi r11, 0x20` @0x8278F814 -- one stretch-distance slot per hard-no-go map COLUMN
    // (DWARF BrnHardNoGoMap.h:30, KI_HNG_MAP_WIDTH == 32), matching
    // RacingLine::maStretchDistanceForHNG[32].
    const s32 KI_HNG_STRETCH_COUNT = 32;

    // `lfs f1, flt_820C4168` == 0.5f -- GetPointFarAhead measures every section's length across
    // the middle of the map (height interpolant 0.5).
    const f32 KF_SECTION_LENGTH_HEIGHT = 0.5f;
}

// The six PerfMon handles: ONE definition, here (see the header_request above).

// =================================================================================================
// GetPerpendicularDistanceToCentreLine -- DWARF BrnRacingLineGenerator.h:102. NO IDA export; the
// console inlines it whole into the two bodies below, and the two copies are identical:
//
//   0x8278FFDC / 0x827777DC  lbz 0xBD1(lpRacingLine)      -- mbCentreLineHereKnown
//   0x82790034 / 0x82777800  lvx 0xBE0 -> v2, 0xBF0 -> v3 -- mCentreHere, mCentreAhead
//   0x82790048 / 0x82777810  bl  sub_8276DDB8              -- DistancePointToLine(v1, v2, v3)
//   0x8279002C / 0x827777F8  the "not known" arm supplies the CALLER's own default: 0.0f in
//                            GetRoadPositionAsPercentage, mfRoadPlacement (lfs 0xB24) in
//                            RaceLineDefaultsToSlamPlayer -- which is exactly what the DWARF's
//                            third (float32_t) parameter is for.
// =================================================================================================
f32 RacingLineGenerator::GetPerpendicularDistanceToCentreLine(RacingLine* lpRacingLine,
                                                              Vector2 lPos2D,
                                                              f32 lfDefault)
{
    if (!lpRacingLine->mbCentreLineHereKnown)
        return lfDefault;

    return DistancePointToLine(lPos2D, lpRacingLine->mCentreHere, lpRacingLine->mCentreAhead);
}

// =================================================================================================
// InitialiseRacingLine @0x8278FB20 (168 instructions)
//
// Register map: r3/r25 = this, r4/r30 = lpRacingLine, r5/r23 = lpCar, r6/r22 = liSectionIndex,
// r7/r21 = lpSectionsData -- i.e. the DWARF (:75) order (RacingLine*, AICar*, int32_t,
// AISectionsData*). The asserts NAME the second argument `lpRoute` because AICar embeds its Route
// at offset 0 (BrnAICar.h: GetRoute() == reinterpret_cast<Route*>(this)), so `lwz 0x1408(r23)`
// (Route::meStatus) and `bl AICar::GetPosition(r4 = r23)` really are the same pointer. That is the
// one place the pseudocode misleads -- it renders a3 as a bare int serving both roles.
//
//   0x8278FB48..0x8278FBDC  the four asserts (:1196 / :1197 / :1201 / :1202)
//   0x8278FBE0              lCarPosition = lpCar->GetPosition() -- only .y is used, hoisted into
//                           f31 @0x8278FE40 BEFORE the second loop (the compiler then reuses that
//                           stack slot for a GetRouteCentre out-parameter)
//   0x8278FBEC..0x8278FDB4  the one-shot PerfMon registration and its five `>= 0` asserts
//   0x8278FDB8              stb 0, 0xBD0 -- SetInitialised(false)
//   0x8278FDC0..0x8278FDF4  first / last section in cache, from the route's node count
//   0x8278FDF8              CacheLocalSections(lpRacingLine, lpCar->GetRoute(), lpSectionsData,
//                                              lpCar)   (r5 and r7 are the SAME register, r23)
//   0x8278FE10..0x8278FE2C  SetUpHardNoGoMap over the whole cache
//   0x8278FE44..0x8278FF98  the target-setup pass
//   0x8278FF9C              stb 1, 0xBD0 -- SetInitialised(true)
// =================================================================================================
void RacingLineGenerator::InitialiseRacingLine(RacingLine* lpRacingLine, AICar* lpCar,
                                               s32 liSectionIndex, AISectionsData* lpSectionsData)
{
    CGS_ASSERT(lpRacingLine != 0, "lpRacingLine");      // :1196
    CGS_ASSERT(lpSectionsData != 0, "lpSectionsData");  // :1197

    const Route* lpRoute = lpCar->GetRoute();
    CGS_ASSERT(lpRoute != 0, "lpRoute");                                                // :1201
    CGS_ASSERT(lpRoute->GetStatus() != Route::E_STATUS_UNINITIALISED,
               "lpRoute->IsInitialised()");                                             // :1202

    const Vector3 lCarPosition = lpCar->GetPosition();

    if (miGenerateRacingLinePM == -1)
    {
        miGenerateRacingLinePM = CgsDev::PerfMonCpu::AddMonitor("Generate Racing Line",
                                                                KE_RACING_LINE_PERFMON_PAGE, false,
                                                                KF_PERFMON_BUDGET_GENERATE, true);
        miGenerateInSectionPM  = CgsDev::PerfMonCpu::AddMonitor("In Section",
                                                                KE_RACING_LINE_PERFMON_PAGE, false,
                                                                KF_PERFMON_BUDGET_STEP, true);
        miHNGMapGenerationPM1  = CgsDev::PerfMonCpu::AddMonitor("HNG Map dropping",
                                                                KE_RACING_LINE_PERFMON_PAGE, false,
                                                                KF_PERFMON_BUDGET_STEP, true);
        miHNGMapGenerationPM2  = CgsDev::PerfMonCpu::AddMonitor("HNG Map spread current",
                                                                KE_RACING_LINE_PERFMON_PAGE, false,
                                                                KF_PERFMON_BUDGET_STEP, true);
        miHNGMapGenerationPM3  = CgsDev::PerfMonCpu::AddMonitor("HNG Map spread previous",
                                                                KE_RACING_LINE_PERFMON_PAGE, false,
                                                                KF_PERFMON_BUDGET_STEP, true);
        miFarAheadPM           = CgsDev::PerfMonCpu::AddMonitor("Far ahead scan",
                                                                KE_RACING_LINE_PERFMON_PAGE, false,
                                                                KF_PERFMON_BUDGET_STEP, true);

        CGS_ASSERT(miGenerateRacingLinePM >= 0, "miGenerateRacingLinePM >= 0");   // :1216
        CGS_ASSERT(miGenerateInSectionPM  >= 0, "miGenerateInSectionPM >= 0");    // :1217
        CGS_ASSERT(miHNGMapGenerationPM1  >= 0, "miHNGMapGenerationPM1 >= 0");    // :1218
        CGS_ASSERT(miHNGMapGenerationPM2  >= 0, "miHNGMapGenerationPM2 >= 0");    // :1219
        CGS_ASSERT(miHNGMapGenerationPM3  >= 0, "miHNGMapGenerationPM3 >= 0");    // :1220
        // (the console emits FIVE of these, not six -- miFarAheadPM is not asserted.)
    }

    lpRacingLine->SetInitialised(false);                                          // stb 0, 0xBD0

    // lwz 0x1400(route) - liSectionIndex, clamped to sixteen: the cache holds at most
    // KI_RACING_LINE_MAX_AVAILABLE_SECTIONS nodes and never runs past the end of the route.
    s32 liSectionsToCache = lpRoute->GetNodeCount() - liSectionIndex;
    if (liSectionsToCache >= KI_MAX_SECTIONS_TO_CACHE)
        liSectionsToCache = KI_MAX_SECTIONS_TO_CACHE;

    lpRacingLine->mFirstSectionInCache = liSectionIndex;
    lpRacingLine->mLastSectionInCache  = liSectionIndex + liSectionsToCache - 1;

    // CacheLocalSections returns the section the cache actually starts at.
    lpRacingLine->mFirstSectionInCache =
        CacheLocalSections(lpRacingLine, lpRoute, lpSectionsData, lpCar);

    for (s32 liSection = lpRacingLine->mFirstSectionInCache;
         liSection <= lpRacingLine->mLastSectionInCache;
         ++liSection)
    {
        SetUpHardNoGoMap(lpRacingLine, liSection);
    }

    // f31 is loaded ONCE, before the loop, from the car position's height lane.
    const f32 lfCarHeight = lCarPosition.y;

    Vector2 lRouteCentreAhead;
    Vector2 lRouteCentreBehind;

    for (s32 liSection = lpRacingLine->mFirstSectionInCache;
         liSection <= lpRacingLine->mLastSectionInCache;
         ++liSection)
    {
        if (IsTargetUpToDate(lpRacingLine, liSection))
            continue;

        if (IsAJunction(lpRacingLine, liSection))
        {
            // A junction's exit is where the straight stretches either side of it cross.
            const s32 liStraightAhead  = LookForStraightSectionAhead(lpRacingLine, liSection);
            const s32 liStraightBehind = LookForStraightSectionBehind(lpRacingLine, liSection);

            GetRouteCentre(lpRacingLine, liStraightAhead,  lRouteCentreAhead);
            GetRouteCentre(lpRacingLine, liStraightBehind, lRouteCentreBehind);

            if (liSection == liStraightAhead || liSection == liStraightBehind)
            {
                // No straight to project from on one side -- fall back to the map's own exit.
                SetupSectionExit(lpRacingLine, liSection);
            }
            else
            {
                // v1 = the BEHIND centre (var_80), v2 = the AHEAD centre (var_90), f1 = the car's
                // height (0x8278FEFC..0x8278FF1C).
                CalculateIntersectionWithProjectedRoute(lpRacingLine, liSection,
                                                        lRouteCentreBehind, lRouteCentreAhead,
                                                        lfCarHeight);
            }

            SetTargetUpToDate(lpRacingLine, liSection);
            SetUpIncomingPortalTarget(lpRacingLine, liSection);
        }
        else
        {
            SetupSectionExit(lpRacingLine, liSection);
            SetTargetUpToDate(lpRacingLine, liSection);
            SetUpIncomingPortalTarget(lpRacingLine, liSection);

            // Budget: at most eight ordinary sections get their target set up in one call.
            if (liSection - lpRacingLine->mFirstSectionInCache > KI_MAX_NON_JUNCTION_SETUPS)
                break;
        }
    }

    lpRacingLine->SetInitialised(true);                                           // stb 1, 0xBD0
}

// =================================================================================================
// GetRoadPositionAsPercentage @0x8278FFB0 (59 instructions)
//
// r3/r30 = this, r4/r31 = lpRacingLine, r5 = lpCar. How far off the centre line the car is, as a
// fraction of the half road width -- AIDriver::HardShoulderSpeed ramps its 75 % speed limiter on
// it above 0.85.
//
//   0x8278FFDC  lbz 0xBD0 -- not initialised -> 0.0f (flt_82001CC0)
//   0x8278FFFC  AICar::GetPosition, then the two vrlimi128 lane packs (mask 8 shift 0 = .x,
//               mask 4 shift 1 = .z into .y) == BrnMath::Flatten: world (x, Z) -> 2D (x, y)
//   0x8279000C  the inlined GetPerpendicularDistanceToCentreLine, default 0.0f
//   0x8279005C  GetHalfRoadWidthHere(lpRacingLine, lCarPos2D)
//   0x82790060  fcmpu vs 0.0 -- an exactly-zero half width returns 0.0f, no divide
//   0x82790068  fdivs then fabs
// =================================================================================================
f32 RacingLineGenerator::GetRoadPositionAsPercentage(RacingLine* lpRacingLine, AICar* lpCar)
{
    if (!lpRacingLine->mbIsInitialised)
        return 0.0f;

    const Vector2 lCarPos2D = BrnMath::Flatten(lpCar->GetPosition());

    const f32 lfPerpendicularDistance =
        GetPerpendicularDistanceToCentreLine(lpRacingLine, lCarPos2D, 0.0f);

    const f32 lfHalfRoadWidth = GetHalfRoadWidthHere(lpRacingLine, lCarPos2D);
    if (lfHalfRoadWidth == 0.0f)
        return 0.0f;

    return std::fabs(lfPerpendicularDistance / lfHalfRoadWidth);
}

// =================================================================================================
// RaceLineDefaultsToSlamPlayer @0x82777730 (44 instructions)
//
// r3 = this, r4/r31 = lpRacingLine, v1/v127 = lSlamTarget3D. Point the racing line's default
// perpendicular offset at the slam target: AIDriver::GenerateRacingLine calls it while the
// aggression machine holds a valid target, then (in ATTACK_SLAM) copies the offset into
// mfRoadPlacement.
//
//   0x82777754..0x827777A8  the three-lane vspltw128 + vcmpeqfp. self-equality cascade ==
//                           rw::math::vpu::IsValid(Vector3); assert :3297 IS emitted in retail
//   0x827777E4..0x827777F0  the same two vrlimi128 lane packs as above == BrnMath::Flatten
//   0x827777DC..0x82777810  the inlined GetPerpendicularDistanceToCentreLine, whose "not known"
//                           default is mfRoadPlacement (lfs 0xB24)
//   0x82777818 / 0x8277781C stfs 0xB10 / stb 1, 0xB14
// =================================================================================================
void RacingLineGenerator::RaceLineDefaultsToSlamPlayer(RacingLine* lpRacingLine,
                                                       Vector3 lSlamTarget3D)
{
    CGS_ASSERT(rw::math::vpu::IsValid(lSlamTarget3D), "RwMath::IsValid( lSlamTarget3D )");  // :3297

    const Vector2 lSlamTarget2D = BrnMath::Flatten(lSlamTarget3D);

    lpRacingLine->mfDefaultPerpendicularOffset =
        GetPerpendicularDistanceToCentreLine(lpRacingLine, lSlamTarget2D,
                                             lpRacingLine->mfRoadPlacement);
    lpRacingLine->mbDefiniteDestination = true;
}

// =================================================================================================
// FinishedSpreadingBack -- DWARF BrnRacingLineGenerator.h:363. No export; inlined three times in
// SpreadHNGBackOneStep (0x8278F768..0x8278F780, 0x8278F7A8..0x8278F7C4, 0x8278F82C..0x8278F848),
// each copy the identical store triple: miSectionToSpread + 1, miBackwardsStep = -1,
// miHNGLineStart = 0 -- move the spread cursor on to the next section, from scratch.
// =================================================================================================
void RacingLineGenerator::FinishedSpreadingBack(RacingLine* lpRacingLine)
{
    ++lpRacingLine->miSectionToSpread;
    lpRacingLine->miBackwardsStep = -1;
    lpRacingLine->miHNGLineStart  = 0;
}

// =================================================================================================
// HasSpreadHardNoGoLinesFinished -- DWARF :368. No export; the scan at 0x8278F7F4..0x8278F824:
// walk maStretchDistanceForHNG[0..31] and stop at the FIRST slot that is >= 0.0f (fcmpu / bge).
// The spread is finished only once every one of the 32 columns has gone negative, i.e. the
// hard-no-go lines have been carried back past the spread distance everywhere.
// =================================================================================================
bool RacingLineGenerator::HasSpreadHardNoGoLinesFinished(RacingLine* lpRacingLine)
{
    for (s32 liColumn = 0; liColumn < KI_HNG_STRETCH_COUNT; ++liColumn)
    {
        if (lpRacingLine->maStretchDistanceForHNG[liColumn] >= 0.0f)
            return false;
    }
    return true;
}

// =================================================================================================
// SpreadHardNoGoLinesThroughCurrentSection -- DWARF :372. No export; the miBackwardsStep == 0 arm
// at 0x8278F70C..0x8278F740: seed the stretch distances by spreading this section's OWN map along
// the track, then step the backwards cursor.
//   0x8278F724  GetSectionPointer(lpRacingLine, miSectionToSpread)
//   0x8278F72C  r3 = section + 0x50 == &SectionData::mHardNoGoMap
//   0x8278F728  r4 = lpRacingLine + 0xB40 == maStretchDistanceForHNG
//   0x8278F730  f1 = lfs 0xBCC == mfSpreadDistance
// =================================================================================================
void RacingLineGenerator::SpreadHardNoGoLinesThroughCurrentSection(RacingLine* lpRacingLine)
{
    SectionData* lpCurrentSectionData =
        GetSectionPointer(lpRacingLine, lpRacingLine->miSectionToSpread);

    lpCurrentSectionData->mHardNoGoMap.SpreadHNGAlongTrack(lpRacingLine->maStretchDistanceForHNG,
                                                           lpRacingLine->mfSpreadDistance);
    ++lpRacingLine->miBackwardsStep;
}

// =================================================================================================
// SpreadHNGBackOneStep @0x8278F680 (125 instructions)
//
// r3/r29 = this, r4/r31 = lpRacingLine. One slice of the round-robin hard-no-go work
// (AIDriver::DoRoundRobinWork runs up to four per frame). Returns TRUE when there is nothing left
// to do -- that is the caller's loop-exit answer, not an error.
//
// The cursor triple is (miSectionToSpread 0xBC0, miBackwardsStep 0xBC4, miHNGLineStart 0xBC8), all
// three reset by RacingLine::ClearSectionCache. miBackwardsStep sequences the work for one
// section: -1 = still building that section's own map, 0 = spread it along the track, n > 0 =
// carry it back into the n'th section behind.
//
//   0x8278F694  lbz 0xBD0 -- not initialised -> true
//   0x8278F6AC  miSectionToSpread > mLastSectionInCache -> true
//   0x8278F6BC  miSectionToSpread < mFirstSectionInCache -> snap it forward
//   0x8278F6CC  == -1 -> DropHardNoGoLinesIntoMap, bracketed by miHNGMapGenerationPM1
//   0x8278F704  ==  0 -> SpreadHardNoGoLinesThroughCurrentSection, bracketed by ...PM2
//   0x8278F758  (miSectionToSpread - miBackwardsStep) has walked off the front of the cache ->
//               FinishedSpreadingBack
//   0x8278F79C  lbz 0x98(section) == mHardNoGoMap.mbReady -- that section's map is not built yet
//               -> FinishedSpreadingBack
//   0x8278F7D0  SpreadHNGIntoPreviousSection, bracketed by ...PM3, then either
//               FinishedSpreadingBack (all 32 stretch columns exhausted) or one more step back
// =================================================================================================
bool RacingLineGenerator::SpreadHNGBackOneStep(RacingLine* lpRacingLine)
{
    if (!lpRacingLine->mbIsInitialised)
        return true;

    if (lpRacingLine->miSectionToSpread > lpRacingLine->mLastSectionInCache)
        return true;

    if (lpRacingLine->miSectionToSpread < lpRacingLine->mFirstSectionInCache)
        lpRacingLine->miSectionToSpread = lpRacingLine->mFirstSectionInCache;

    if (lpRacingLine->miBackwardsStep == -1)
    {
        CgsDev::PerfMonCpu::StartMonitor(miHNGMapGenerationPM1);
        DropHardNoGoLinesIntoMap(lpRacingLine);
        CgsDev::PerfMonCpu::StopMonitor(miHNGMapGenerationPM1);
        return false;
    }

    if (lpRacingLine->miBackwardsStep == 0)
    {
        CgsDev::PerfMonCpu::StartMonitor(miHNGMapGenerationPM2);
        SpreadHardNoGoLinesThroughCurrentSection(lpRacingLine);
        CgsDev::PerfMonCpu::StopMonitor(miHNGMapGenerationPM2);
        return false;
    }

    const s32 liPreviousSection = lpRacingLine->miSectionToSpread - lpRacingLine->miBackwardsStep;
    if (liPreviousSection < lpRacingLine->mFirstSectionInCache)
    {
        FinishedSpreadingBack(lpRacingLine);
        return false;
    }

    SectionData* lpPreviousSectionData = GetSectionPointer(lpRacingLine, liPreviousSection);
    if (!lpPreviousSectionData->mHardNoGoMap.IsReady())
    {
        FinishedSpreadingBack(lpRacingLine);
        return false;
    }

    CgsDev::PerfMonCpu::StartMonitor(miHNGMapGenerationPM3);
    lpPreviousSectionData->mHardNoGoMap.SpreadHNGIntoPreviousSection(
        lpRacingLine->maStretchDistanceForHNG);
    CgsDev::PerfMonCpu::StopMonitor(miHNGMapGenerationPM3);

    if (HasSpreadHardNoGoLinesFinished(lpRacingLine))
        FinishedSpreadingBack(lpRacingLine);
    else
        ++lpRacingLine->miBackwardsStep;

    return false;
}

// =================================================================================================
// GetPointFarAhead @0x827900A0 (118 instructions)
//
// r3/r29 = this, r4/r31 = lpRacingLine, f1/f29 = lfDistanceAhead, v1/v127 = lFrom2D,
// r6/r25 = &lrOutPosition, r7/r26 = &lrOutDirection. NOTE r5 IS SKIPPED: the float argument
// consumes its GPR slot, which is why the two reference outs land in r6/r7 (COMMON_BRIEF rule 4).
//
// Walk forward along the cached racing line until lfDistanceAhead metres have been covered, then
// return the point on that section's curve (pushed sideways by mfRoadPlacement) and the curve's
// tangent there. Returns false whenever the walk runs out of cache -- which is exactly what every
// caller (SteeringFan::CachePointAhead, AIDriver::FindPositionInFuture /
// FindFinalDriftDirection) reads as "no point ahead is known".
//
//   0x827900DC  StartMonitor(miFarAheadPM) -- the WHOLE body is bracketed, both exits included
//   0x827900F0  GetLocalSectionID(lpRacingLine, lFrom2D, miLastKnownSectionID (lwz 0xC00))
//   0x827900FC  == 0x7FFF (KI_INVALID_SECTION_INDEX) -> fail
//   0x82790118  lbz 0x98(section) == mHardNoGoMap.mbReady (SectionData +0x50 plus 0x48) -> fail
//   0x82790134  GetSectionInterpPosition(lpRacingLine, liSectionID, lFrom2D)
//   0x8279014C  SectionLength(0.5f)
//   0x82790154  fmadds: remaining = sectionLength * interp + lfDistanceAhead -- measured from the
//               START of the current section, not from the car
//   0x82790160..0x827901A0  step forward one section at a time, subtracting each section's length,
//               until one is longer than what is left (fcmpu / bgt -> found)
//   0x827901CC  fdivs then the fsel pair == saturate(remaining / sectionLength) into [0, 1]
//   0x82790204  GetPointAndNormalOnCurve(lpRacingLine, liSectionID, interp, lPoint (r7),
//                                        lNormal (r8))
//   0x82790250  vmaddfp, read in RAW FIELD ORDER vD = vA*vC + vB (tools/re/vmx128.py banner):
//               outPosition = lNormal * splat(mfRoadPlacement) + lPoint
//   0x8279025C  the two vrlimi128 writes touch ONLY lanes x and y of *lpOutDirection:
//               x = lNormal.y, y = -lNormal.x (v13 is lNormal.x XORed with the 0x80000000 sign
//               mask built by vspltisw -1 / vslw) -- the normal turned 90 degrees, i.e. the
//               tangent along the line.
// =================================================================================================
bool RacingLineGenerator::GetPointFarAhead(RacingLine* lpRacingLine, f32 lfDistanceAhead,
                                           Vector2 lFrom2D, Vector2& lrOutPosition,
                                           Vector2& lrOutDirection)
{
    CgsDev::PerfMonCpu::StartMonitor(miFarAheadPM);

    s32 liSectionID = GetLocalSectionID(lpRacingLine, lFrom2D, lpRacingLine->miLastKnownSectionID);
    if (liSectionID == AICar::KI_INVALID_SECTION_INDEX)
    {
        CgsDev::PerfMonCpu::StopMonitor(miFarAheadPM);
        return false;
    }

    SectionData* lpSectionData = GetSectionPointer(lpRacingLine, liSectionID);
    if (!lpSectionData->mHardNoGoMap.IsReady())
    {
        CgsDev::PerfMonCpu::StopMonitor(miFarAheadPM);
        return false;
    }

    const f32 lfInterpInSection = GetSectionInterpPosition(lpRacingLine, liSectionID, lFrom2D);
    f32 lfDistanceRemaining =
        lpSectionData->mHardNoGoMap.SectionLength(KF_SECTION_LENGTH_HEIGHT) * lfInterpInSection
        + lfDistanceAhead;

    f32  lfSectionLength = 0.0f;
    bool lbFoundSection  = false;

    while (liSectionID <= lpRacingLine->mLastSectionInCache)
    {
        SectionData* lpWalkSectionData = GetSectionPointer(lpRacingLine, liSectionID);
        if (!lpWalkSectionData->mHardNoGoMap.IsReady())
            break;

        lfSectionLength = lpWalkSectionData->mHardNoGoMap.SectionLength(KF_SECTION_LENGTH_HEIGHT);
        if (lfSectionLength > lfDistanceRemaining)
        {
            lbFoundSection = true;
            break;
        }

        lfDistanceRemaining -= lfSectionLength;
        ++liSectionID;
    }

    if (!lbFoundSection)
    {
        CgsDev::PerfMonCpu::StopMonitor(miFarAheadPM);
        return false;
    }

    // fsel f0, -interp, 0.0, interp   then   fsel f1, 1.0 - interp, interp, 1.0
    f32 lfInterp = lfDistanceRemaining / lfSectionLength;
    if (lfInterp <= 0.0f)
        lfInterp = 0.0f;
    if (lfInterp > 1.0f)
        lfInterp = 1.0f;

    Vector2 lPoint;
    Vector2 lNormal;
    GetPointAndNormalOnCurve(lpRacingLine, liSectionID, lfInterp, lPoint, lNormal);

    const f32 lfRoadPlacement = lpRacingLine->mfRoadPlacement;
    lrOutPosition.x = lNormal.x * lfRoadPlacement + lPoint.x;
    lrOutPosition.y = lNormal.y * lfRoadPlacement + lPoint.y;
    lrOutPosition.z = lNormal.z * lfRoadPlacement + lPoint.z;
    lrOutPosition.w = lNormal.w * lfRoadPlacement + lPoint.w;

    // Only lanes x and y are written (vrlimi128 masks 8 and 4); z / w keep whatever the caller
    // had there, exactly as the console leaves them.
    lrOutDirection.x =  lNormal.y;
    lrOutDirection.y = -lNormal.x;

    CgsDev::PerfMonCpu::StopMonitor(miFarAheadPM);
    return true;
}

// =================================================================================================
// [FLAG PC bring-up] PARKS -- nothing here bodies these, and nothing invents them.
//
//   RenderHardNoGoMap @0x82790278 (DWARF :190), DrawCentreLine (:203), RenderSectionDetails (:208)
//     -- debug render. Presentation-only on this host (COMMON_BRIEF rule 11): they draw the map
//     grid / centre line through the AI debug component, which is not part of this bring-up.
//     DELETE-WHEN the AI debug-render component is brought up on the host.
//
//   FindTargetFixedDistanceAhead (:96), FindAnyTargetOnLineAhead (:248),
//   RaceDefaultsLineToAutoCentre (:107), RaceLineDefaultsToHoldingRoadPosition (:117),
//   MoveRaceLineToSlamPlayer (:122)
//     -- DWARF-declared, NO IDA export, and NO inlined copy of any of them appears anywhere in
//     this lane's five exported bodies (they are folded into GenerateRacingLine @0x8277C4E8 and
//     the AIDriver target chain, neither of which is in this wave). There is nothing to recover
//     them from, so no body is written -- a guessed one would be invention.
//     DELETE-WHEN the wave that reconstructs their inlining call sites lands.
// =================================================================================================
}
