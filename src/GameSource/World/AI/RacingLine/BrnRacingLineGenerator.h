#ifndef BRN_RACING_LINE_GENERATOR_H
#define BRN_RACING_LINE_GENERATOR_H

// BrnAI::RacingLineGenerator -- builds and maintains a driver's racing line from the section
// cache held by BrnAI::RacingLine. Embedded BY VALUE in AIDriver @guest+0x1B30 (DWARF
// BrnAIDriver.h:542); the DecFIGS DWARF (BrnRacingLineGenerator.h) gives it NO instance data --
// only static PerfMon ids / a debug flag -- which is exactly why the console slot is 4 bytes
// (0x1B30 .. 0x1B34, the next member mPIDController starts at 0x1B34). Every method takes the
// RacingLine it works on as an explicit argument.
//
// BODIED (BrnRacingLineGenerator.cpp -- the SECTION-CACHE half, aiwave2 lane R1 2026-09-05):
//   * GetSectionPointer @0x827655D0 . CacheUpToDate (inlined) . GetLocalSectionID @0x82776280
//   * GetNearSectionID  @0x827765A8 . GetSectionPortal @0x827685B8
//   * GuessInwardsSectionPortal @0x82775E50 . SetTargetUpToDate @0x82768710
//   * LookForStraightSectionAhead @0x82775C10 / Behind @0x82775D30
//   * GetSimpleHermite @0x827767F0 . GetIterativeHermite @0x82776D70
//   * GetSectionInterpPosition @0x827815F8 . GetHalfRoadWidthHere @0x8278EC08
//   * GetRouteCentre @0x8278F408 . CalculateIntersectionWithProjectedRoute @0x8278F188
//   * GenerateInOutVectors @0x82780638 . SetUpHardNoGoMap @0x82780518
//   * GetCentreCentreLineHere @0x8278E930 . CacheLocalSections @0x8278ECE8
//   * SetupSectionExit @0x8278F548 . DropHardNoGoLinesIntoMap @0x8278F600
// BODIED (RacingLine/BrnRacingLineGenerator_Extrapolate.cpp, aiwave A6):
//   * ExtrapolateRouteBackwards @0x82782580 / ExtrapolateRouteForwards @0x82781AE8 /
//     ExtrapolateTwistyRoute    @0x82782018   (all three static -- the console passes the
//     generate-count in r3, never a `this`; ProcessExtrapolatedRoute @0x8278C528 `li r3, 6`)
// BODIED (RacingLine/BrnRacingLineGenerator_GetForwardPortalIndex.cpp, aiwave A5):
//   * GetForwardPortalIndex @0x827817F8
// BODIED (RacingLine/BrnRacingLineGenerator_Curve.cpp, aiwave2 lane R3 2026-09-05):
//   * GetPointAndNormalOnCurve @0x82780B18 . SetUpIncomingPortalTarget @0x8278F878
//   * IsAJunction / IsTargetUpToDate (both inlined on the console; no export)
// The QUERY half (InitialiseRacingLine, GetPointFarAhead, GetRoadPositionAsPercentage,
// SpreadHNGBackOneStep, RaceLineDefaults*, the Find*Target* pair and the three spreading
// helpers) is BrnRacingLineGenerator_Query.cpp's. Nine members carry a named [PARK] -- see the
// closing banner of BrnRacingLineGenerator.cpp for the list and each one's reason.
//
// SectionData (the cache element) lives in its DWARF home Route/BrnRacingLine.h:149.

#include "types.hpp"
#include "BrnCommonTypes.h"                               // Vector2 / Vector3
#include "GameSource/World/AI/Route/BrnRacingLine.h"      // RacingLine + SectionData
#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<T,N> (ExtrapolatedIndexArray)

// ============================================================================================
// THE TWO BRING-UP GATES for the racing-line / steering-fan stack. Defined HERE (and not in
// BrnAIDriver.h, whose own #ifndef fallback now never fires because it includes this header
// first) so the SteeringFan / RacingLineGenerator TUs and the AIDriver TUs cannot disagree.
//
// BRN_AI_RACINGLINE_STACK_PRESENT -- the RacingLineGenerator QUERY half:
//   InitialiseRacingLine @0x8278FB20, GetPointFarAhead @0x827900A0,
//   GetRoadPositionAsPercentage @0x8278FFB0, GetCentreCentreLineHere @0x8278E930,
//   SpreadHNGBackOneStep @0x8278F680, RaceLineDefaultsToSlamPlayer @0x82777730 and the
//   GetLocalSectionID / GetSectionPointer / GetSectionInterpPosition / GetHalfRoadWidthHere
//   chain under them. As of aiwave2 (2026-09-05) that whole chain IS bodied here and in
//   BrnRacingLineGenerator_Query.cpp, and HardNoGoMap is bodied in BrnHardNoGoMap.cpp -- the
//   conductor flips this to 1 once both partfiles and BrnHardNoGoMap.cpp link. While it is 0
//   every call site still runs the console's own "not found / not known" answer.
//
// BRN_AI_STEERINGFAN_TARGET_PRESENT -- the SteeringFan weighting/target half
//   (BrnAISteeringFan_Target.cpp + BrnAISteeringFan_Weightings.cpp, aiwave R6). Those TUs ARE
//   mounted, so AIDriver::GetTargetPosition @0x8277CBF8, AIDriver::DoRoundRobinWork's
//   E_ROUND_ROBIN_FAN arm and AIDriver::CalculateDesiredSpeed's GetSpeedRatio read can all take
//   their console arm by flipping this ONE line to 1.
//
// [FLAG PC bring-up] IT IS DELIBERATELY STILL 0, and this is the R6 lane's headline finding:
//   kfBias (recovered in full -- see BrnAISteeringFan_Target.cpp) shows that in
//   eBiasMode_Race the ONLY contributors with a non-zero weight are eFan_SteerToCentre (+20),
//   eFan_AvoidHNG (+50), eFan_ExitHNG (-300), eFan_AvoidTraffic (-100),
//   eFan_AvoidOncomingTraffic (-400), eFan_AvoidEdges (-200) and eFan_DriveParallel (-200).
//   EVERY ONE of those needs something this tree does not have yet: the first, second, third and
//   seventh need RacingLine::mbCentreLineHereKnown / the HardNoGoMap section stack (i.e.
//   BRN_AI_RACINGLINE_STACK_PRESENT), the fourth and fifth need the .bss-resident tuning
//   constants IncludeConstantBearing reads, and the sixth has no IDA export at all. With all of
//   them inert every mfCumulativeWeighting slot stays 0.0, and a FLAT fan is pathological:
//   BestTargetInArea is "first wins on ties", so GetDrivingTarget returns mTarget[0] -- the ray
//   mfFanAngle (80 degrees) to the car's RIGHT -- and GetSpeedRatio turns index 0 into ratio 0.0,
//   which CalculateDesiredSpeed multiplies into (0 * 0.75 + 0.25) = a QUARTER of the desired
//   speed. That is exactly the failure BrnAIDriver_Update.cpp's GetSpeedRatio park documents
//   from run6. Flipping this to 1 before the racing-line half lands would therefore replace
//   "rival drives straight" with "rival turns hard right at a quarter speed".
//   FLIP IT TO 1 the moment BRN_AI_RACINGLINE_STACK_PRESENT can also go to 1 (or as soon as
//   IncludeHardNoGo + IncludeConstantBearing have bodies); every gated site already carries the
//   faithful console call. DELETE-WHEN both halves are present.
// ============================================================================================
#ifndef BRN_AI_RACINGLINE_STACK_PRESENT
#define BRN_AI_RACINGLINE_STACK_PRESENT 1   // flipped 2026-09-05 (aiwave2): the section-cache + query halves and HardNoGoMap are bodied
#endif
#ifndef BRN_AI_STEERINGFAN_TARGET_PRESENT
#define BRN_AI_STEERINGFAN_TARGET_PRESENT 1  // flipped 2026-09-05 (aiwave2): all 14 contributors + CalculateFanAngle are bodied
#endif

namespace BrnAI
{
struct AICar;
struct Route;   // fwd (GameSource/World/AI/Route/BrnRoute.h) -- CacheLocalSections argument

// ---- ADDITIVE (aiwave A6, 2026-09-03) ---------------------------------------------------
struct AISection;                 // fwd (SharedClasses/AI/AISectionsResourceType.h)
struct AISectionsData;            // fwd (SharedClasses/AI/AISectionsResourceType.h)
struct SectionAndPortalIndices;   // fwd (GameSource/World/AI/Route/BrnRouteMapModule.h:48)

// DWARF BrnRouteMapModule.h:55 -- `typedef CgsContainers::Array<BrnAI::SectionAndPortalIndices,
// 16u> ExtrapolatedIndexArray;`. Its DWARF home is BrnRouteMapModule.h (not this lane's file); a
// typedef may be redeclared to the same type, so this copy stays legal once it lands there too.
// The tree spells the container as the unqualified Array<T,N> (see the CgsArray.h banner).
typedef Array<SectionAndPortalIndices, 16> ExtrapolatedIndexArray;

class RacingLineGenerator
{
public:
    // ---- ADDITIVE static route extrapolators (aiwave A6, 2026-09-03) ---------------------
    // Bodied in RacingLine/BrnRacingLineGenerator_Extrapolate.cpp (the three Extrapolate*)
    // and RacingLine/BrnRacingLineGenerator_GetForwardPortalIndex.cpp (aiwave A5).
    //
    // Register map shared by the three Extrapolate* bodies: r3 = liNumSectionsToGenerate,
    // r4 = the start section index (u16 via `clrlwi r31,r4,16` in Backwards; s32 in the other
    // two), v1 = lCarDirection, v2 = lCarPosition (NEVER read by any of the three -- only v1
    // is copied out of the argument registers), r5 = lpAISectionsData, r6 = &lpauGeneratedIndices.
    // The argument order (direction BEFORE position) is the DWARF BrnRacingLineGenerator.cpp
    // :2699/:2877/:3085 prototype and is confirmed by ProcessExtrapolatedRoute @0x8278C534..
    // 0x8278C53C (`lvx128 v2, req+0x10 (mCarPosition)` / `lvx128 v1, req+0x20 (mCarDirection)`)
    // and by Backwards negating v1 into lCarBackwardsDirection (:3097).
    //
    // Each returns the number of (section, portal) pairs written to lpauGeneratedIndices
    // (Backwards can return -1: see its banner). The array must already carry a live count of
    // 16 (ProcessExtrapolatedRoute stores 16 at +0x80 before the first call).

    // DWARF BrnRacingLineGenerator.h:139 -- @0x82782580 (398 insns). Walks the road BEHIND the
    // car: entry i = { the section behind, the portal OF THAT SECTION that leads back }.
    static s32 ExtrapolateRouteBackwards(s32 liNumSectionsToGenerate, u16 luStartSectionIndex,
                                         Vector2 lCarDirection, Vector2 lCarPosition,
                                         const AISectionsData* lpAISectionsData,
                                         ExtrapolatedIndexArray& lpauGeneratedIndices);

    // DWARF BrnRacingLineGenerator.h:148 -- @0x82781AE8 (330 insns). Walks the road AHEAD,
    // always through the most-ahead portal: entry i = { section i, its exit portal }.
    static s32 ExtrapolateRouteForwards(s32 liNumSectionsToGenerate, s32 liStartSectionIndex,
                                        Vector2 lCarDirection, Vector2 lCarPosition,
                                        const AISectionsData* lpAISectionsData,
                                        ExtrapolatedIndexArray& lpauGeneratedIndices);

    // DWARF BrnRacingLineGenerator.h:158 -- @0x82782018 (344 insns). As Forwards, but picks the
    // LEAST-ahead portal that still points forward (the twistiest road).
    static s32 ExtrapolateTwistyRoute(s32 liNumSectionsToGenerate, s32 liStartSectionIndex,
                                      Vector2 lCarDirection, Vector2 lCarPosition,
                                      const AISectionsData* lpAISectionsData,
                                      ExtrapolatedIndexArray& lpauGeneratedIndices);

    // DWARF BrnRacingLineGenerator.h:166 -- @0x827817F8. The portal of lpAISection whose
    // direction from lCarPosition is most aligned with lCarDirection (0 when none beats the
    // -2.0 seed). NOTE this one takes (position, direction) -- the DWARF order for THIS member.
    static u8 GetForwardPortalIndex(const AISectionsData* lpAISectionData,
                                    const AISection* lpAISection,
                                    Vector2 lCarPosition, Vector2 lCarDirection);

    // ============================ DWARF SHAPE (aiwave2 lane R1, 2026-09-05) ==================
    // The class is grown to the full DecFIGS BrnRacingLineGenerator.h shape. Access levels and
    // parameter lists are the DWARF's; the addresses are the ARTIST name index's. Members
    // marked [R1] are bodied in BrnRacingLineGenerator.cpp, [R2] in
    // BrnRacingLineGenerator_Query.cpp, and [PARK] carry a named park in the .cpp banner.
    // NOTE the DWARF's access split: GetLocalSectionID / GetNearSectionID / GetSectionPointer /
    // CacheUpToDate are PUBLIC (SteeringFan calls all four), everything below `private:` is not.

    void InitialiseRacingLine(RacingLine* lpRacingLine, AICar* lpCar, s32 liSectionIndex,
                              AISectionsData* lpAISectionsData);                  // :75  @0x8278FB20 [R2]

    // @0x82776280 [R1] -- the cached section that contains lPosition. Starts from
    // liSectionID (KI_INVALID_SECTION_INDEX == scan the whole cache), then probes
    // +1 / -1, then walks forwards to mLastSectionInCache and backwards to
    // mFirstSectionInCache. Returns KI_INVALID_SECTION_INDEX when nothing contains it.
    s32  GetLocalSectionID(RacingLine* lpRacingLine, Vector2 lPosition, s32 liSectionID);   // :81

    // @0x827765A8 [R1] -- the cheap neighbour-only twin of GetLocalSectionID: tests
    // liSectionID, then liSectionID +/- 1, and gives up. Asserts on an invalid seed.
    s32  GetNearSectionID(RacingLine* lpRacingLine, Vector2 lPosition, s32 liSectionID);    // :87

    s32  FindTargetFixedDistanceAhead(RacingLine* lpRacingLine, Vector2 lFrom2D,
                                      Vector2& lrTarget, f32 lfDistance, f32 lfOffset,
                                      s32 liSectionID);                            // :96  [R2]
    f32  GetPerpendicularDistanceToCentreLine(RacingLine* lpRacingLine, Vector2 lPosition,
                                              f32 lfInterp);                       // :102 [R2]
    void RaceDefaultsLineToAutoCentre(RacingLine* lpRacingLine);                    // :107 [R2]
    void RaceLineDefaultsToSlamPlayer(RacingLine* lpRacingLine, Vector3 lTarget);   // :112 @0x82777730 [R2]
    void RaceLineDefaultsToHoldingRoadPosition(RacingLine* lpRacingLine, f32 lfRoadPlacement); // :117 [R2]
    void MoveRaceLineToSlamPlayer(RacingLine* lpRacingLine, Vector3 lTarget);       // :122 [R2]

    // @0x82780638 [R1] -- the incoming/outgoing tangents of the section's hermite centre line,
    // each the normalised step to the neighbouring section's portal, scaled by
    // min(|entrance - exit|, KF_IN_OUT_VECTOR_MAX_LENGTH).
    void GenerateInOutVectors(RacingLine* lpRacingLine, s32 liSectionIndex,
                              Vector2& lrInVector, Vector2& lrOutVector);          // :129

    // @0x827655D0 [R1] -- &lpRacingLine->maSectionCache[liIndex & 15], asserting the entry is
    // the one asked for. PUBLIC in the DWARF (:171); SteeringFan::IncludeHardNoGo calls it.
    SectionData* GetSectionPointer(RacingLine* lpRacingLine, s32 liIndex);          // :171

    SectionData* GetSectionPointerForWrite(RacingLine* lpRacingLine, s32 liIndex);  // :176 [PARK]
    s32  GetBestSectionIndex(RacingLine* lpRacingLine, s32 liSectionIndex);         // :181 [PARK]
    f32  GetRoadPositionAsPercentage(RacingLine* lpRacingLine, AICar* lpCar);       // :186 @0x8278FFB0 [R2]
    void RenderHardNoGoMap(RacingLine* lpRacingLine);                               // :190 @0x82790278 [PARK]
    bool GetPointFarAhead(RacingLine* lpRacingLine, f32 lfDistanceAhead, Vector2 lFrom2D,
                          Vector2& lrOutPosition, Vector2& lrOutDirection);         // :198 @0x827900A0 [R2]
    void DrawCentreLine(RacingLine* lpRacingLine, Vector2 lPosition);               // :203 [PARK]
    void RenderSectionDetails(RacingLine* lpRacingLine, Vector2 lPosition);         // :208 [PARK]
    bool SpreadHNGBackOneStep(RacingLine* lpRacingLine);                            // :212 @0x8278F680 [R2]

    // @0x8278E930 [R1] -- the point on the cached centre line closest to lTargetPos2D, plus a
    // second point KF_CENTRE_LINE_AHEAD_STEP further along it. False when no cached section
    // yields a curve (both outputs then collapse to lTargetPos2D).
    // BOTH vector parameters are POSITIONS, not a position/direction pair: the asm feeds v1
    // to GetLocalSectionID and v2 to GetSectionInterpPosition + the distance minimisation
    // (@0x8278E990 / @0x8278EA28 / @0x8278EAF0), and both call sites pass the flattened car
    // position twice. The old `lCarDir2D` spelling was a guess; renamed to match the asm.
    bool GetCentreCentreLineHere(RacingLine* lpRacingLine, Vector2 lCarPos2D, Vector2 lTargetPos2D,
                                 Vector2& lrCentreHere, Vector2& lrCentreAhead);    // :221

    // [R1] -- no export (inlined at every call site as
    // `maSectionCache[i & 15].mCachedSectionIndex == i`); attested by GetSectionPointer
    // @0x827655EC, GetLocalSectionID @0x827763E0, SetTargetUpToDate @0x82768728 and five more.
    bool CacheUpToDate(RacingLine* lpRacingLine, s32 liSectionIndex);                // :226

private:
    void DoSlowTurn();                                                              // :231 [PARK]

    // @0x8278ECE8 [R1] -- refresh every cache slot in [mFirstSectionInCache, mLastSectionInCache]
    // from the route; when a NEGATIVE slot is stale, extrapolate the road behind the car instead.
    // Returns the first section index the cache is now valid from.
    s32  CacheLocalSections(RacingLine* lpRacingLine, const Route* lpRoute,
                            AISectionsData* lpAISectionsData, AICar* lpCar);        // :239

    s32  FindAnyTargetOnLineAhead(RacingLine* lpRacingLine, Vector2 lFrom2D, Vector2& lrTarget,
                                  f32 lfDistance, f32 lfOffset, s32 liSectionID);   // :248 [R2]

    // @0x8278EC08 [R1] -- half the estimated road width under lPosition, minus a
    // KF_HALF_ROAD_WIDTH_INSET margin and floored at KF_HALF_ROAD_WIDTH_MIN.
    f32  GetHalfRoadWidthHere(RacingLine* lpRacingLine, Vector2 lPosition);          // :253

    // @0x827767F0 [R1] -- the plain cubic hermite of (A, B) with tangents (in, out) at lInterp.
    void GetSimpleHermite(Vector2 lInVectorA, Vector2 lInVectorB, Vector2 lInVector,
                          Vector2 lOutVector, VecFloat lInterp, Vector2& lrResult);  // :262

    // @0x82776D70 [R1] -- five relaxation steps that find the hermite parameter whose point
    // projects onto lInterp along the chord A->B. Returns the parameter it settled on.
    f32  GetIterativeHermite(Vector2 lInVectorA, Vector2 lInVectorB, Vector2 lInVector,
                             Vector2 lOutVector, f32 lInterp, Vector2& lrResult);    // :271

    // @0x827815F8 [R1] -- where lPosition falls along the section's entrance->exit chord
    // (0 at the entrance, 1 at the exit); 0 for a degenerate section.
    f32  GetSectionInterpPosition(RacingLine* lpRacingLine, s32 liSectionIndex, Vector2 lPosition); // :278

    // @0x8278F188 [R1] -- intersect the segment lFrom->lTo with the section's maximal hard-no-go
    // edge and store the crossing point as the section's exit target.
    void CalculateIntersectionWithProjectedRoute(RacingLine* lpRacingLine, s32 liNodeIndex,
                                                 Vector2 lFrom, Vector2 lTo, f32 lfUnused); // :287

    // [R3] -- no export (inlined); the `mpLineSection->mx8Flags & 0x10` test, recovered from
    // InitialiseRacingLine @0x8278FE88..0x8278FEA4.
    bool IsAJunction(RacingLine* lpRacingLine, s32 liSectionIndex);                  // :292

    // @0x8278F548 [R1] -- set the section's exit target to the midpoint of the hard-no-go map's
    // maximal edges.
    void SetupSectionExit(RacingLine* lpRacingLine, s32 liNodeIndex);                // :297

    // @0x8278F408 [R1] -- the midpoint of the section's maximal hard-no-go edges at interp 1.0.
    void GetRouteCentre(RacingLine* lpRacingLine, s32 liSectionIndex, Vector2& lrCentre); // :303

    // @0x82775C10 / @0x82775D30 [R1] -- walk the cache for the first section AHEAD / BEHIND
    // liSectionIndex that is not flagged as a junction.
    s32  LookForStraightSectionAhead(RacingLine* lpRacingLine, s32 liSectionIndex);  // :308
    s32  LookForStraightSectionBehind(RacingLine* lpRacingLine, s32 liSectionIndex); // :313

    // @0x82768710 [R1] -- mark the cached section's target as up to date.
    void SetTargetUpToDate(RacingLine* lpRacingLine, s32 liSectionIndex);            // :318

    // [R3] -- no export (inlined); the READ twin of SetTargetUpToDate. Attested at
    // @0x8278FE44..0x8278FE74 (InitialiseRacingLine) and @0x8278F8A0..0x8278F8D0
    // (SetUpIncomingPortalTarget). A stale cache answers false, without an assert.
    bool IsTargetUpToDate(RacingLine* lpRacingLine, s32 liSectionIndex);             // :323

    // @0x82780B18 [R3] -- the hermite centre-line point at lfInterp plus the UNIT NORMAL there.
    // The asm proves the DWARF parameter ORDER: r6 is skipped for the f32, so r7 == lrPoint (it
    // is GetIterativeHermite's &lrResult @0x827810A4) and r8 == lrNormal (the closing normalise
    // stores into it @0x827815C4). Declaration unchanged.
    void GetPointAndNormalOnCurve(RacingLine* lpRacingLine, s32 liSectionIndex, f32 lfInterp,
                                  Vector2& lrPoint, Vector2& lrNormal);              // :331
    f32  GetWidthHalfWidthOfKnownSection(RacingLine* lpRacingLine, s32 liSectionIndex,
                                         f32 lfInterp);                              // :337 [PARK]

    // @0x827685B8 [R1] -- the two endpoints of the section's target-portal boundary line.
    void GetSectionPortal(SectionData* lpSectionData, Vector2& lrLeft, Vector2& lrRight);   // :343

    // @0x82775E50 [R1] -- when the next section is not cached, guess its portal as the OPPOSITE
    // edge of this section's four-corner footprint.
    void GuessInwardsSectionPortal(SectionData* lpSectionData, Vector2& lrLeft, Vector2& lrRight); // :349

    // @0x82780518 [R1] -- (re)build the section's hard-no-go map corners from its own portal and
    // the previous section's (or the guessed inwards portal), then clear the map.
    void SetUpHardNoGoMap(RacingLine* lpRacingLine, s32 liSectionIndex);             // :354

    // @0x8278F878 [R3] -- the incoming half of SetupSectionExit: the section's entrance becomes
    // the previous section's exit when that target is up to date, else the midpoint of this
    // section's own maximal hard-no-go edges at map height 0.0.
    void SetUpIncomingPortalTarget(RacingLine* lpRacingLine, s32 liSectionIndex);    // :359
    void FinishedSpreadingBack(RacingLine* lpRacingLine);                            // :363 [R2]
    bool HasSpreadHardNoGoLinesFinished(RacingLine* lpRacingLine);                    // :368 [R2]
    void SpreadHardNoGoLinesThroughCurrentSection(RacingLine* lpRacingLine);          // :372 [R2]

    // @0x8278F600 [R1] -- place the current section's next budgeted batch of hard-no-go lines;
    // advance the per-frame success count or the line cursor.
    void DropHardNoGoLinesIntoMap(RacingLine* lpRacingLine);                         // :376

    // ---- the class's only static state (DWARF :380..:385, :393) -------------------------
    // Six CgsDev::PerfMonCpu monitor handles, pinned to the .bss words the bodies read:
    //   miGenerateRacingLinePM 0x82F30254 . miGenerateInSectionPM  0x82F30258
    //   miHNGMapGenerationPM1  0x82F3025C . miHNGMapGenerationPM2  0x82F30260
    //   miHNGMapGenerationPM3  0x82F30264 . miFarAheadPM           0x82F30268
    // (attested by InitialiseRacingLine @0x8278FB20, which touches all six in that order, and by
    // the Start/StopMonitor pairs in GetLocalSectionID / GetNearSectionID /
    // SpreadHNGBackOneStep / GetPointFarAhead). DEFINED ONCE, in BrnRacingLineGenerator.cpp.
    static s32  miGenerateRacingLinePM;                                              // :380
    static s32  miGenerateInSectionPM;                                               // :381
    static s32  miHNGMapGenerationPM1;                                               // :382
    static s32  miHNGMapGenerationPM2;                                               // :383
    static s32  miHNGMapGenerationPM3;                                               // :384
    static s32  miFarAheadPM;                                                        // :385
    static bool mbDrawAvoidanceDebug;                                                // :393
};
}

#endif
