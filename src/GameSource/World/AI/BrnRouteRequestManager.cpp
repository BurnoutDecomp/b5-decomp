#include "GameSource/World/AI/BrnRouteRequestManager.h"

#include "GameSource/Math/BrnMathUtils.h"                    // BrnMath::Flatten (XZ ground plane)
#include "GameSource/World/AI/BrnAIPortal.h"                 // BrnAI::Portal (GetLinkSectionIndex)
#include "GameSource/World/AI/BrnAIBuzzBy.h"                              // BuzzBy::IsPositionInNoBuzzZone
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"      // RacingLineGenerator::GetForwardPortalIndex
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT + Begin/Fire/End
#include "GameShared/GameClasses/Development/CgsStrStream.h" // CgsDev::StrStream (default-case assert)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"        // CgsNumeric::Random (mRandom)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnAI::RouteRequestManager::Construct                          @ 0x8278A3B0 (KEPT verbatim)
//   BrnAI::RouteRequestManager::ChooseDistanceFunction             @ 0x82788CC0
//   BrnAI::RouteRequestManager::ComputeSectionBehind               @ 0x827892C0
//   BrnAI::RouteRequestManager::GenerateAlternativeRouteRequest    @ 0x82788F58
//   BrnAI::RouteRequestManager::GenerateExtrapolatedRouteRequest   @ 0x82789090
//   BrnAI::RouteRequestManager::GenerateFreeRoamingDestination     @ 0x827695F0
//   BrnAI::RouteRequestManager::GenerateRoute                      @ 0x827948B0
//   BrnAI::RouteRequestManager::GenerateRouteFleeingRouteRequest   @ 0x827891A0
//   BrnAI::RouteRequestManager::GenerateStandardRouteRequest       @ 0x82791490
//   BrnAI::RouteRequestManager::GetFleeVector                      @ 0x8277A598
//   BrnAI::RouteRequestManager::Update                             @ 0x82797FA8
//
// The X360 compiled the geometry over AltiVec/VMX (lvx128/vspltw/vcmpgtfp lane shuffles); it is
// de-optimised here to portable Vector2 scalar math at SEMANTIC PARITY (mirroring BrnMathUtils.cpp),
// never as __asm transcription. The route requests are built by NAMED setters on the
// RouteMapModuleIO request records (the X360 inlined those setters as direct stores into the
// request byte image; their offsets are pinned in BrnRouteMapModuleIO.h). The block-section-id
// append loop reverses int_16_::Append back to RaceRouteRequest::AddBlockSectionId, and the
// per-checkpoint source list (int_8_::GetItem on mauBlockSectionIds[checkpoint]) to the slot's
// GetBlockSectionCount()/GetBlockSectionId() views.
//
// "2D" here is the XZ ground plane: BrnMath::Flatten(Vector3) -> Vector2 with .x == world X and
// .y == world Z, exactly as the X360 vsubfp/vmaxfp lane math operates.

namespace BrnAI
{
// (The two TU-local collaborator shims that stood here -- `class BuzzBy` and `class RacingLineGenerator`
//  -- were retired 2026-09-03: both homes exist now (BrnAIBuzzBy.h defines `struct BuzzBy`;
//  RacingLine/BrnRacingLineGenerator.h declares GetForwardPortalIndex, bodied in
//  BrnRacingLineGenerator_GetForwardPortalIndex.cpp). The `class` shim also mangled BuzzBy with the
//  wrong class-key, which made RouteRequestManager::Update unresolvable from AIModule::Update.)

// Module-global weight/bound table written during construction (guest 0x8300D570..).
float gRouteRequestWeights[8] = {
    1.0f,         // 0x3F800000
    1.78315496f,  // 0x3FDC4FAC
    1.19288969f,  // 0x3F98B2DC
    1.70421982f,  // 0x3FDA2520
    1.76656675f,  // 0x3FE21A5C
    1.73375201f,  // 0x3FDDEAD6
    1.22346330f,  // 0x3F9C9FB2
    1.14250064f,  // 0x3F924336
};
float gRouteRequestBoundLo = 9.0190702e-11f; // 0x2EC3A75A
float gRouteRequestBoundHi = 0.0f;
float gRouteRequestPad     = 0.0f;

// DWARF BrnRouteRequestManager.h:158 marks mRandom `extern` -- it is a file-scope static, NOT a
// per-instance member. GenerateFreeRoamingDestination draws section indices from it.
static CgsNumeric::Random mRandom;

// X360 ChooseDistanceFunction (@0x82788CC0): the hardcoded world-X divide that splits the map
// into the "countryside" half (X < this) and the "city" half. DWARF KF_HACK_CONTRYSIDE_DIVIDE
// (BrnRouteRequestManager.cpp:364). Value 200.0 attested in the pseudocode (`v64[0] = 200.0`).
static const f32 KF_HACK_CONTRYSIDE_DIVIDE = 200.0f;

RouteRequestManager* RouteRequestManager::Construct()
{
    gRouteRequestWeights[0] = 1.0f;
    gRouteRequestWeights[1] = 1.78315496f;
    gRouteRequestWeights[2] = 1.19288969f;
    gRouteRequestWeights[3] = 1.70421982f;
    gRouteRequestWeights[4] = 1.76656675f;
    gRouteRequestWeights[5] = 1.73375201f;
    gRouteRequestWeights[6] = 1.22346330f;
    gRouteRequestWeights[7] = 1.14250064f;
    gRouteRequestBoundLo    = 9.0190702e-11f;
    gRouteRequestBoundHi    = 0.0f;
    gRouteRequestPad        = 0.0f;

    for (int i = 0; i < 16; ++i)
    {
        mauBlockSectionIds[i].mWords[0] = 0;
    }
    mauBlockSectionIds[15].mWords[1] = 0;

    return this;
}

// X360 @0x82788CC0. Pick the A* heuristic for routing lpAICar to luDestinationSectionIndex.
// Returns plain Euclidean when both endpoints sit in the "countryside" half of the map
// (world X < KF_HACK_CONTRYSIDE_DIVIDE); otherwise biases the heuristic along the dominant axis
// of the start->end offset, or -- when the car is heading roughly toward the destination -- only
// X/Y-biases when one axis dominates the other by more than 2x, else falls back to the car's
// heading's dominant axis.
AStarDistanceFunction RouteRequestManager::ChooseDistanceFunction(
    const AICar* lpAICar, const AISectionsData* lpAISectionData,
    AStarDistanceFunction leDefaultAStarDistanceFunction, u16 luDestinationSectionIndex)
{
    (void)leDefaultAStarDistanceFunction;   // X360 receives it but never returns it from this path

    const Vector3 lStartPos3 = lpAICar->GetPosition();
    const Vector3 lEndPos3   = lpAISectionData->GetAISection(luDestinationSectionIndex)->GetMiddle();

    const Vector2 lStart = BrnMath::Flatten(lStartPos3);   // (x, z) of the car
    const Vector2 lEnd   = BrnMath::Flatten(lEndPos3);     // (x, z) of the destination middle

    // diff = end - start (XZ); absDiff = |diff| componentwise.
    const Vector2 lDiff    = { lEnd.x - lStart.x, lEnd.y - lStart.y };
    const Vector2 lAbsDiff = { (lDiff.x < 0.0f) ? -lDiff.x : lDiff.x,
                               (lDiff.y < 0.0f) ? -lDiff.y : lDiff.y };

    const Vector2 lDir    = BrnMath::Flatten(lpAICar->GetDirection());
    const Vector2 lAbsDir = { (lDir.x < 0.0f) ? -lDir.x : lDir.x,
                              (lDir.y < 0.0f) ? -lDir.y : lDir.y };

    // The X360 self-equality NaN cascades on every Flatten output are rw::math::vpu::IsValid
    // tripwires (folded out, like BrnMathUtils.cpp); behaviour is unchanged.

    // lbDrivingAway: the projection of the offset onto the heading falls short of the same
    // "countryside" divide used for the Euclidean early-out. Reconstructed from the asm compare
    // (both operands loaded from the SAME {200.0,0,0,0} literal buffer used just below):
    //   KF_HACK_CONTRYSIDE_DIVIDE > (diff.x*dir.x + diff.y*dir.y)
    const f32  lfHeadingDot  = lDiff.x * lDir.x + lDiff.y * lDir.y;
    const bool lbDrivingAway = KF_HACK_CONTRYSIDE_DIVIDE > lfHeadingDot;

    // Both endpoints in the countryside half -> plain Euclidean.
    if (KF_HACK_CONTRYSIDE_DIVIDE > lEnd.x && KF_HACK_CONTRYSIDE_DIVIDE > lStart.x)
        return E_ASTAR_DISTANCE_EUCLIDEAN;

    if (lbDrivingAway)
    {
        // Bias along the dominant offset axis (X when |dx| > |dy|, else Y).
        return (lAbsDiff.x > lAbsDiff.y) ? E_ASTAR_DISTANCE_EUCLIDEAN_X_BIASED
                                         : E_ASTAR_DISTANCE_EUCLIDEAN_Y_BIASED;
    }

    // Heading roughly toward the destination: only bias when one axis dominates by > 2x.
    if (lAbsDiff.x > 2.0f * lAbsDiff.y)
        return E_ASTAR_DISTANCE_EUCLIDEAN_X_BIASED;
    if (lAbsDiff.y > 2.0f * lAbsDiff.x)
        return E_ASTAR_DISTANCE_EUCLIDEAN_Y_BIASED;

    // Neither offset axis dominates: fall back to the car heading's dominant axis.
    // X360 (@0x82789028..): `vcmpgtfp absDir.y > absDir.x` -> result = (that ? 2 : 1), i.e.
    // absDir.y > absDir.x -> Y_BIASED(2), else X_BIASED(1).
    return (lAbsDir.y > lAbsDir.x) ? E_ASTAR_DISTANCE_EUCLIDEAN_Y_BIASED
                                   : E_ASTAR_DISTANCE_EUCLIDEAN_X_BIASED;
}

// X360 @0x827892C0. The section immediately behind the car: take the car's current best section,
// look up the forward portal along its racing line, and return that portal's link-section index
// (the section the car came from). Used to block U-turns on a standard race route.
u16 RouteRequestManager::ComputeSectionBehind(const AICar* lpAICar, const AISectionsData* lpAISectionData)
{
    const u16 luBestSectionIndex = lpAICar->GetBestSectionIndex();
    const AISection* lpBestSection = lpAISectionData->GetAISection(luBestSectionIndex);

    // 0x82789304..0x82789334: v2 = Flatten(-GetDirection()) (the `vspltisw -1 ; vslw ; vxor`
    // trio flips the sign bits = negate), v1 = Flatten(GetPosition()). Looking BACKWARDS along
    // the car's heading, the "forward" portal is the one behind the car.
    const Vector3 lDirection = lpAICar->GetDirection();
    const Vector3 lNegatedDirection = Vector3{ -lDirection.x, -lDirection.y, -lDirection.z };
    const Vector2 lCarDirection2D = BrnMath::Flatten(lNegatedDirection);
    const Vector2 lCarPosition2D  = BrnMath::Flatten(lpAICar->GetPosition());

    const u8 lu8ForwardPortalIndex =
        RacingLineGenerator::GetForwardPortalIndex(lpAISectionData, lpBestSection,
                                                   lCarPosition2D, lCarDirection2D);

    return lpBestSection->GetPortal(lu8ForwardPortalIndex)->GetLinkSectionIndex();
}

// X360 @0x82791490. The full race route to the car's destination section. Skips entirely when the
// car has no destination, or the destination equals its current section. Optionally chooses a
// per-route heuristic, blocks the current-checkpoint sections, and -- when U-turns are disallowed
// -- blocks the section behind the car.
void RouteRequestManager::GenerateStandardRouteRequest(
    AICar* lpAICar, const AISectionsData* lpAISectionData,
    RouteMapModuleIO::InputBuffer* lpRouteInputBuffer, EUTurns leAllowUTurns)
{
    const u16 luDestinationSectionIndex = lpAICar->GetDestinationSectionIndex();
    if (luDestinationSectionIndex == AICar::KI_INVALID_SECTION_INDEX)
        return;

    const u16 luBestSectionIndex = lpAICar->GetBestSectionIndex();
    if (luBestSectionIndex == luDestinationSectionIndex)
        return;

    CGS_ASSERT(luDestinationSectionIndex < lpAISectionData->muNumSections,
               "luDestinationSectionIndex < lpAISectionData->GetAISectionCount()");

    const u16 luRaceCarIndex = static_cast<u16>(lpAICar->GetRaceCarIndex());

    RouteMapModuleIO::RaceRouteRequest lRouteRequest;
    lRouteRequest.Construct(lpAICar->GetPosition(),
                            lpAISectionData->GetMiddle(luDestinationSectionIndex),
                            luBestSectionIndex, luDestinationSectionIndex, luRaceCarIndex);
    lRouteRequest.SetUseAIShortcuts(lpAICar->UseAIShortcuts());

    // Heuristic: the manager default (this+0x240) UNLESS the car is the player car AND flagged to
    // run the per-route ChooseDistanceFunction (X360: mbIsPlayer @+0x1549 != 0 && flag @+0x1550 != 0).
    AStarDistanceFunction leDistanceFunction;
    if (!lpAICar->IsPlayerCar() || !lpAICar->UseChosenDistanceFunction())
        leDistanceFunction = meDefaultAStarDistanceFunction;
    else
        leDistanceFunction = ChooseDistanceFunction(lpAICar, lpAISectionData,
                                                    meDefaultAStarDistanceFunction,
                                                    luDestinationSectionIndex);
    lRouteRequest.SetDistanceFunction(leDistanceFunction);

    // Block the current-checkpoint's block-section ids (when this car has them).
    if (lpAICar->HasBlockCheckpoints())
    {
        const s32 liCheckpoint = lpAICar->GetCurrentCheckpoint();
        CGS_ASSERT(liCheckpoint < 16, "lpAICar->GetCurrentCheckpoint() < KI_MAX_LANDMARKS_IN_MODE");

        lRouteRequest.SetQuality(E_ASTAR_QUALITY_MEDIUM);   // X360 stores 1 == the medium preset

        const RequestSlot& lrSlot = mauBlockSectionIds[liCheckpoint];
        for (u32 luIndex = 0; luIndex < lrSlot.GetBlockSectionCount(); ++luIndex)
        {
            CGS_ASSERT(lrSlot.GetBlockSectionCount() != 0xFFFFFFFFu,
                       "Array used before Construct/Clear was called");
            lRouteRequest.AddBlockSectionId(lrSlot.GetBlockSectionId(luIndex));
        }
    }

    // No U-turns: also block the section behind the car (by its AISection id).
    if (leAllowUTurns == E_NO_U_TURNS)
    {
        const u16 luSectionBehind = ComputeSectionBehind(lpAICar, lpAISectionData);
        lRouteRequest.AddBlockSectionId(lpAISectionData->GetAISection(luSectionBehind)->mId);
    }

    lpRouteInputBuffer->GetRaceRouteRequestQueue()->AddEventSafe(lRouteRequest);
}

// X360 @0x82788F58. A route biased onto a different line than the player's. Only issued when the
// car has a valid destination distinct from its current section; the alternative-ness is encoded
// as a distance function of MANHATTAN(3) / DIAGONAL(4) chosen by the race-car index's parity.
void RouteRequestManager::GenerateAlternativeRouteRequest(
    AICar* lpAICar, const AICar* lpPlayerCar, const AISectionsData* lpAISectionData,
    RouteMapModuleIO::InputBuffer* lpRouteInputBuffer)
{
    (void)lpPlayerCar;   // taken by the X360 but the alt-line is keyed off the car index

    const u16 luDestinationSectionIndex = lpAICar->GetDestinationSectionIndex();
    if (luDestinationSectionIndex == AICar::KI_INVALID_SECTION_INDEX)
        return;

    const u16 luBestSectionIndex = lpAICar->GetBestSectionIndex();
    if (luBestSectionIndex == luDestinationSectionIndex)
        return;

    CGS_ASSERT(luDestinationSectionIndex < lpAISectionData->muNumSections,
               "luDestinationSectionIndex < lpAISectionData->GetAISectionCount()");

    const s32 liRaceCarIndex = lpAICar->GetRaceCarIndex();

    RouteMapModuleIO::RaceRouteRequest lRouteRequest;
    lRouteRequest.Construct(lpAICar->GetPosition(),
                            lpAISectionData->GetMiddle(luDestinationSectionIndex),
                            luBestSectionIndex, luDestinationSectionIndex,
                            static_cast<u16>(liRaceCarIndex));
    lRouteRequest.SetUseAIShortcuts(lpAICar->UseAIShortcuts());

    // Alternate line: even car index -> Manhattan (3), odd -> Diagonal (4).
    const AStarDistanceFunction leDistanceFunction =
        static_cast<AStarDistanceFunction>(((liRaceCarIndex & 1) != 0) + E_ASTAR_DISTANCE_MANHATTAN);
    lRouteRequest.SetDistanceFunction(leDistanceFunction);

    lpRouteInputBuffer->GetRaceRouteRequestQueue()->AddEventSafe(lRouteRequest);
}

// X360 @0x82789090. A short look-ahead request that predicts the car forward along its current
// heading (its useful direction). Used by ROAD_RAGE / PURSUIT / ALWAYS_STRAIGHT / MARKED_MAN.
void RouteRequestManager::GenerateExtrapolatedRouteRequest(
    AICar* lpAICar, const AISectionsData* lpAISectionData,
    RouteMapModuleIO::InputBuffer* lpRouteInputBuffer)
{
    (void)lpAISectionData;

    CGS_ASSERT(lpAICar->GetBestSectionIndex() != AICar::KI_INVALID_SECTION_INDEX,
               "lpAICar->GetBestSectionIndex() != BrnWorld::KI_INVALID_SECTION_INDEX");

    const u16 luBestSectionIndex = lpAICar->GetBestSectionIndex();
    const u16 luRaceCarIndex     = static_cast<u16>(lpAICar->GetRaceCarIndex());

    // The request stores the FLATTENED (XZ) 2D car direction + position (the X360 vrlimi128
    // lane-packs the useful direction's and position's X/Z lanes).
    const Vector2 lDirection2D = BrnMath::Flatten(lpAICar->GetUsefulDirection());
    const Vector2 lPosition2D  = BrnMath::Flatten(lpAICar->GetPosition());

    RouteMapModuleIO::ExtrapolatedRouteRequest lRouteRequest;
    lRouteRequest.Construct(lDirection2D, lPosition2D, luBestSectionIndex, luRaceCarIndex);

    lpRouteInputBuffer->GetExtrapolatedRouteRequestQueue()->AddEventSafe(lRouteRequest);
}

// X360 @0x8277A598. The world-space direction to flee a threat: the AI car's position minus the
// threat's position. Degenerates to the AI car's own useful direction when there is no distinct
// threat. The X360 gates the directional flee vector on the THREAT car's state (lwz r11, 0x14C8(r5)
// where r5 is lpCarToAvoid): the difference is built only when lpCarToAvoid is IN_RANGE(0) or
// OUT_OF_RANGE(1) AND it is a distinct car from lpAICar.
Vector3 RouteRequestManager::GetFleeVector(AICar* lpCarToAvoid, AICar* lpAICar)
{
    const EAICarState leState = lpCarToAvoid->GetState();   // X360: state read from r5 == lpCarToAvoid
    const bool lbActiveThreat = (leState == E_AI_CAR_STATE_IN_RANGE || leState == E_AI_CAR_STATE_OUT_OF_RANGE);

    if (!lbActiveThreat || lpAICar == lpCarToAvoid)
        return lpAICar->GetUsefulDirection();

    const Vector3 lAICarPos  = lpAICar->GetPosition();
    const Vector3 lThreatPos = lpCarToAvoid->GetPosition();
    return Vector3{ lAICarPos.x - lThreatPos.x,
                    lAICarPos.y - lThreatPos.y,
                    lAICarPos.z - lThreatPos.z };
}

// X360 @0x827891A0. A look-ahead request whose direction is GetFleeVector (away from the threat),
// used by the AVOID_PLAYER route-finding style.
void RouteRequestManager::GenerateRouteFleeingRouteRequest(
    AICar* lpAICar, AICar* lpCarToAvoid, const AISectionsData* lpAISectionData,
    RouteMapModuleIO::InputBuffer* lpRouteInputBuffer)
{
    (void)lpAISectionData;

    CGS_ASSERT(lpAICar->GetBestSectionIndex() != AICar::KI_INVALID_SECTION_INDEX,
               "lpAICar->GetBestSectionIndex() != BrnWorld::KI_INVALID_SECTION_INDEX");

    const u16 luBestSectionIndex = lpAICar->GetBestSectionIndex();
    const u16 luRaceCarIndex     = static_cast<u16>(lpAICar->GetRaceCarIndex());

    const Vector2 lDirection2D = BrnMath::Flatten(GetFleeVector(lpCarToAvoid, lpAICar));
    const Vector2 lPosition2D  = BrnMath::Flatten(lpAICar->GetPosition());

    RouteMapModuleIO::ExtrapolatedRouteRequest lRouteRequest;
    lRouteRequest.Construct(lDirection2D, lPosition2D, luBestSectionIndex, luRaceCarIndex);

    lpRouteInputBuffer->GetExtrapolatedRouteRequestQueue()->AddEventSafe(lRouteRequest);
}

// X360 @0x827695F0. Pick a random destination section for a free-roaming (cruising) car: draw a
// random section index, then scan forward until landing on a usable section (not a shortcut, not
// flagged 0x40/0x04) whose middle is not inside a no-buzz zone. Stores the chosen index back into
// the car's destination-section field.
void RouteRequestManager::GenerateFreeRoamingDestination(
    AICar* lpAICar, AICar* lpPlayerCar, const AISectionsData* lpAISectionData, BuzzBy* lpBuzzByManager)
{
    (void)lpPlayerCar;

    const u32 luNumSections = lpAISectionData->muNumSections;

    // mRandom.RandomUInt(0, muNumSections - 1). The X360 inlines RandomUInt's LCG + modulo together
    // with RandomUInt's own dev-asserts (liMax >= liMin / luMod > 0); those asserts live inside the
    // named RandomUInt body and are NOT duplicated at this call site.
    u16 luSectionIndex = static_cast<u16>(mRandom.RandomUInt(0, luNumSections - 1));

    if (luSectionIndex == AICar::KI_INVALID_SECTION_INDEX)
        return;

    for (;;)
    {
        const AISection* lpAISection = lpAISectionData->GetAISection(luSectionIndex);

        // Usable section: none of flags 0x01 (shortcut), 0x40, 0x04 set.
        if ((lpAISection->mx8Flags & 0x01) == 0 &&
            (lpAISection->mx8Flags & 0x40) == 0 &&
            (lpAISection->mx8Flags & 0x04) == 0)
        {
            const Vector3 lMiddle = lpAISection->GetMiddle();
            if (!lpBuzzByManager->IsPositionInNoBuzzZone(lMiddle))
                break;
        }

        // Advance to the next section (wrapping), bounds-checked like the X360.
        luSectionIndex = static_cast<u16>((luSectionIndex + 1) % luNumSections);
        CGS_ASSERT(luSectionIndex < luNumSections, "luSectionIndex < muNumSections");
    }

    lpAICar->SetDestinationSectionIndex(luSectionIndex);
}

// X360 @0x827948B0. Dispatch one car to the request builder for its route-finding style.
void RouteRequestManager::GenerateRoute(
    AICar* lpAICar, AICar* lpPlayerCar, AICar* lpaAICars, const AISectionsData* lpAISectionData,
    RouteMapModuleIO::InputBuffer* lpRouteInputBuffer, BuzzBy* lpBuzzByManager)
{
    (void)lpaAICars;

    switch (lpAICar->GetRouteFindingStyle())
    {
        case E_ROUTE_FINDING_FREE_ROAM:
            // The player car free-roams by extrapolation, not by a cruise destination
            // (X360 reads mbIsPlayer @+0x1549 == IsPlayerCar()).
            if (lpAICar->IsPlayerCar())
            {
                GenerateExtrapolatedRouteRequest(lpAICar, lpAISectionData, lpRouteInputBuffer);
                break;
            }
            GenerateFreeRoamingDestination(lpAICar, lpPlayerCar, lpAISectionData, lpBuzzByManager);
            // No-U-turns when the car is in the IN_RANGE state (meCarState == 0).
            GenerateStandardRouteRequest(lpAICar, lpAISectionData, lpRouteInputBuffer,
                                         (lpAICar->GetState() == E_AI_CAR_STATE_IN_RANGE)
                                             ? E_NO_U_TURNS : E_ALLOW_U_TURNS);
            break;

        case E_ROUTE_FINDING_RACE:
            // Race: take an alternative line when the car wants one and isn't forced standard.
            if (lpAICar->WantsAlternativeRoute() && !lpAICar->ForceStandardRoute())
                GenerateAlternativeRouteRequest(lpAICar, lpPlayerCar, lpAISectionData, lpRouteInputBuffer);
            else
                GenerateStandardRouteRequest(lpAICar, lpAISectionData, lpRouteInputBuffer, E_NO_U_TURNS);
            break;

        case E_ROUTE_FINDING_ROAD_RAGE:
        case E_ROUTE_FINDING_PURSUIT:
        case E_ROUTE_FINDING_ALWAYS_STRAIGHT:
        case E_ROUTE_FINDING_MARKED_MAN:
            GenerateExtrapolatedRouteRequest(lpAICar, lpAISectionData, lpRouteInputBuffer);
            break;

        case E_ROUTE_FINDING_AVOID_PLAYER:
            GenerateRouteFleeingRouteRequest(lpAICar, lpPlayerCar, lpAISectionData, lpRouteInputBuffer);
            break;

        default:
        {
            // X360 streams "Unknown route finding style <n>\n" into the assert buffer.
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Unknown route finding style " << static_cast<s32>(lpAICar->GetRouteFindingStyle()) << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer,
                                       "..\\..\\..\\GameSource\\World/AI/BrnRouteRequestManager.cpp", 201);
            CgsDev::Assert::EndAssert();
            break;
        }
    }
}

// X360 @0x82797FA8. Per-frame: when there is a player car, walk the AI roster (stride 0x1560
// bytes) and, for each car whose state is IN_RANGE(0) or OUT_OF_RANGE(1) that NeedsNewRoute() and
// has a valid best section, issue a fresh route request. The X360 loop count is 35 iterations
// (`li r24, 0x23`); the per-car eligibility reads meCarState @+0x14C8 (`lwz r11, -0x6C(car+0x1534)`).
void RouteRequestManager::Update(
    AICar* lpaAICars, AICar* lpPlayerCar, const AISectionsData* lpAISectionData,
    RouteMapModuleIO::InputBuffer* lpRouteInputBuffer, BuzzBy* lpBuzzByManager)
{
    // The X360 guards the whole roster walk on the player car being present (r5 != 0).
    if (lpPlayerCar == nullptr)
        return;

    for (s32 liAICarIndex = 0; liAICarIndex < 35; ++liAICarIndex)
    {
        // The console's `addi r31, r31, 0x1560` roster step is sizeof(AICar) on the console;
        // on this host the subscript IS that step (sizeof(AICar) is static_asserted == 5472 in
        // BrnAICar.h, but the subscript stays correct even if that ever changes).
        AICar* lpAICar = &lpaAICars[liAICarIndex];

        // Eligible cars are IN_RANGE(0) or OUT_OF_RANGE(1); INACTIVE cars are skipped.
        const EAICarState leState = lpAICar->GetState();
        if (leState == E_AI_CAR_STATE_IN_RANGE || leState == E_AI_CAR_STATE_OUT_OF_RANGE)
        {
            if (lpAICar->NeedsNewRoute() &&
                lpAICar->GetBestSectionIndex() != AICar::KI_INVALID_SECTION_INDEX)
            {
                GenerateRoute(lpAICar, lpPlayerCar, lpaAICars, lpAISectionData,
                              lpRouteInputBuffer, lpBuzzByManager);
            }
        }
    }
}
}
