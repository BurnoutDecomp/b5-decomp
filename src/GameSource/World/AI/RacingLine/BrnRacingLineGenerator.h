#ifndef BRN_RACING_LINE_GENERATOR_H
#define BRN_RACING_LINE_GENERATOR_H

// BrnAI::RacingLineGenerator -- builds and maintains a driver's racing line from the section
// cache held by BrnAI::RacingLine. Embedded BY VALUE in AIDriver @guest+0x1B30 (DWARF
// BrnAIDriver.h:542); the DecFIGS DWARF (BrnRacingLineGenerator.h) gives it NO instance data --
// only static PerfMon ids / a debug flag -- which is exactly why the console slot is 4 bytes
// (0x1B30 .. 0x1B34, the next member mPIDController starts at 0x1B34). Every method takes the
// RacingLine it works on as an explicit argument.
//
// BODIED (BrnRacingLineGenerator.cpp):
//   * SetupSectionExit         @0x8278F548
//   * DropHardNoGoLinesIntoMap @0x8278F600
// BODIED (RacingLine/BrnRacingLineGenerator_Extrapolate.cpp, aiwave A6):
//   * ExtrapolateRouteBackwards @0x82782580 / ExtrapolateRouteForwards @0x82781AE8 /
//     ExtrapolateTwistyRoute    @0x82782018   (all three static -- the console passes the
//     generate-count in r3, never a `this`; ProcessExtrapolatedRoute @0x8278C528 `li r3, 6`)
// BODIED (RacingLine/BrnRacingLineGenerator_GetForwardPortalIndex.cpp, aiwave A5):
//   * GetForwardPortalIndex @0x827817F8
// DECLARE-ONLY: the six members below, with the DWARF's own prototypes. Their AIDriver call
// sites stay gated behind BRN_AI_RACINGLINE_STACK_PRESENT (BrnAIDriver.h) until the ~26
// remaining members are reconstructed.
//
// SectionData (the cache element) lives in its DWARF home Route/BrnRacingLine.h:149.

#include "types.hpp"
#include "BrnCommonTypes.h"                               // Vector2 / Vector3
#include "GameSource/World/AI/Route/BrnRacingLine.h"      // RacingLine + SectionData
#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<T,N> (ExtrapolatedIndexArray)

namespace BrnAI
{
struct AICar;

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

    // @0x8278F548  Set the section's exit target to the midpoint of the hard-no-go map's
    // maximal edges.
    void SetupSectionExit(RacingLine* lpRacingLine, s32 liNodeIndex);

    // @0x8278F600  Place the current section's next budgeted batch of hard-no-go lines; advance
    // the per-frame success count or the line cursor.
    void DropHardNoGoLinesIntoMap(RacingLine* lpRacingLine);

    // ---- DECLARE-ONLY members (DWARF prototypes verbatim; see the file banner) --------------
    // Signatures are the DecFIGS DWARF's, NOT inferred from call sites: the dump gives the exact
    // parameter list of every member, and the addresses below come from the ARTIST name index.
    // No body is written here -- these exist so the AIDriver / racing-line bodies that land next
    // can name them without a second, guessed declaration appearing in another TU.
    void InitialiseRacingLine(RacingLine* lpRacingLine, AICar* lpCar, s32 liSectionIndex,
                              AISectionsData* lpAISectionsData);                  // :75  @0x8278FB20
    void RaceLineDefaultsToSlamPlayer(RacingLine* lpRacingLine, Vector3 lTarget);  // :112 @0x82777730
    f32  GetRoadPositionAsPercentage(RacingLine* lpRacingLine, AICar* lpCar);      // :186 @0x8278FFB0
    bool GetPointFarAhead(RacingLine* lpRacingLine, f32 lfDistanceAhead, Vector2 lFrom2D,
                          Vector2& lrOutPosition, Vector2& lrOutDirection);        // :198 @0x827900A0
    bool SpreadHNGBackOneStep(RacingLine* lpRacingLine);                           // :212 @0x8278F680
    bool GetCentreCentreLineHere(RacingLine* lpRacingLine, Vector2 lCarPos2D, Vector2 lCarDir2D,
                                 Vector2& lrCentreHere, Vector2& lrCentreAhead);   // :221 @0x8278E930

private:
    // Resolve the cache entry for a section index (bodied in another TU).
    SectionData* GetSectionPointer(RacingLine* lpRacingLine, s32 liIndex);
};
}

#endif
