#include "GameSource/World/AI/BrnRouteRequestManager.h"

#include "GameSource/Math/BrnMathUtils.h"                    // BrnMath::Flatten (XZ ground plane)
#include "GameSource/World/AI/BrnAIPortal.h"                 // BrnAI::Portal (GetLinkSectionIndex)
#include "GameSource/World/AI/BrnAIBuzzBy.h"                              // BuzzBy::IsPositionInNoBuzzZone
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"      // RacingLineGenerator::GetForwardPortalIndex
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT + Begin/Fire/End
#include "GameShared/GameClasses/Development/CgsStrStream.h" // CgsDev::StrStream (default-case assert)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"        // CgsNumeric::Random (mRandom)
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // CgsDev::Log::gpDebugPrint (witnesses)
#include "GameSource/GameState/BrnGameStateSharedIO.h"      // BrnGameState::GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnAI::RouteRequestManager::Construct                          @ 0x8278A3B0 (corrected 2026-09-22)
//   BrnAI::RouteRequestManager::SetBlockSections / ClearBlockSections (inlined in AIModule::OnModeStart
//                                                                  / OnModeEnd; DWARF :85 / :88)
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
// per-checkpoint source list (int_8_::GetItem on mauBlockSectionIds[checkpoint]) to the
// Array<u32,8>'s own GetLength()/GetItem().
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

// DWARF BrnRouteRequestManager.h:158 marks mRandom `extern` -- it is a file-scope static, NOT a
// per-instance member, at X360 0x8300D570 (findinit: exactly two sites, the Construct below at
// 0x8278A3B8 and GenerateFreeRoamingDestination's draw at 0x82769670). GenerateFreeRoamingDestination
// draws section indices from it.
// ⛔ CORRECTED 2026-09-22 (crash parity, found with G04-D1): Construct used to write an unread
// "gRouteRequestWeights" table (0x3F800000, 0x3FDC4FAC, ...) and never Construct this object, so
// every free-roam destination was drawn from an all-zero Random. The console's stores at
// 0x8278A3B0..0x8278A434 are exactly CgsNumeric::Random::Construct() constant-folded: the ring
// 3F800000 3FE43E6C 3F98B09C 3FDA23E0 3FE21EDC 3FDDEB96 3F9C9A72 3F923D76, the seed
// 0xB5E330D0_2EC654DA at +0x20 (insrdi at 0x8278A418) and index 0 at +0x28.
static CgsNumeric::Random mRandom;

// [FLAG PC witness] NOT IN THE X360 BINARY -- the "[route-req]" instrument's budgets.
// A GLOBAL first-N witness is useless on this path and run6 proved it: the AI roster comes up in
// STAGES (the player's AICar is live from the junkyard hand-off; the five rivals only from
// ADD_CAR_TO_MODE about ten seconds later), so a global budget is spent entirely on the player
// before a rival exists. The first-8 "[route] extrapolated route done" witness in
// scratch/aiwave/run6/BrnGame.log burned all eight lines by log line 3,028 of 802,050 -- four
// frames after the first rival appeared -- which is why that run could not say whether the
// rivals were asking for roads at all. The budget here is therefore PER AI-CAR INDEX, with a
// hard overall cap so a 35-car roster cannot flood the log.
// DELETE-WHEN rivals are seen driving their own routes.
static const s32 KI_ROUTE_REQ_WITNESS_PER_CAR = 4;
static const s32 KI_ROUTE_REQ_WITNESS_TOTAL   = 96;
// After the opening burst, one further line per car every N eligible frames (600 == about ten
// seconds at 60 Hz), until the overall cap is reached.
static const s32 KI_ROUTE_REQ_WITNESS_PERIOD = 600;

// X360 ChooseDistanceFunction (@0x82788CC0): the hardcoded world-X divide that splits the map
// into the "countryside" half (X < this) and the "city" half. DWARF KF_HACK_CONTRYSIDE_DIVIDE
// (BrnRouteRequestManager.cpp:364). Value 200.0 attested in the pseudocode (`v64[0] = 200.0`).
static const f32 KF_HACK_CONTRYSIDE_DIVIDE = 200.0f;

// X360 @0x8278A3B0 (DWARF :68). Three things, in the console's order:
//   0x8278A3B0..0x8278A434  mRandom.Construct()  (the file-static at 0x8300D570, constant-folded)
//   0x8278A438..0x8278A450  for 16 slots: `stw 0, 0x20(slot)` -- each Array<u32,8>'s COUNT word
//   0x8278A454              `stw 0, 0x240(this)` -- meDefaultAStarDistanceFunction = EUCLIDEAN (0)
// ⛔ CORRECTED 2026-09-22: the body zeroed each slot's FIRST ID (mWords[0]) and slot 15's second,
// never the counts, and never touched +0x240 or the Random -- harmless only while the module sat
// in zero-initialised static storage and nothing ever filled a slot.
void RouteRequestManager::Construct()
{
    mRandom.Construct();
    for (s32 liCheckpointIndex = 0; liCheckpointIndex < BrnGameState::GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE;
         ++liCheckpointIndex)
    {
        mauBlockSectionIds[liCheckpointIndex].Construct();
    }
    meDefaultAStarDistanceFunction = E_ASTAR_DISTANCE_EUCLIDEAN;
}

// DWARF :85 / BrnRouteRequestManager.cpp:99. No standalone symbol: AIModule::OnModeStart inlines it
// per checkpoint (0x82791ED4..0x82791F08) -- the caller evaluates the ids pointer FIRST (its
// CheckpointData accessor runs before this assert, as on the console), then:
//   0x82791ED4..0x82791EF8  assert(0 <= i < KI_MAX_LANDMARKS_IN_MODE)   (:101, li r5,0x65)
//   0x82791F04              `stw 0, 0x20(slot)`                          -- Construct (count = 0)
//   0x82791F08              Array<u32,8>::AppendArray<8>(slot, ids)      @0x8278A108
void RouteRequestManager::SetBlockSections(s32 liCheckpointIndex, const Array<u32, 8u>* laBlockSectionIds)
{
    CGS_ASSERT(liCheckpointIndex >= 0
               && liCheckpointIndex < BrnGameState::GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE,
               "liCheckpointIndex >= 0 && liCheckpointIndex < KI_MAX_LANDMARKS_IN_MODE");   // :101
    mauBlockSectionIds[liCheckpointIndex].Construct();
    mauBlockSectionIds[liCheckpointIndex].AppendArray(*laBlockSectionIds);
}

// DWARF :88 / BrnRouteRequestManager.cpp:116. No standalone symbol: AIModule::OnModeEnd inlines it
// at 0x8277BB8C..0x8277BBC8 -- r9 = this + 0x20 (slot 0's count), 16 x `stw 0, 0(r9); r9 += 0x24`.
void RouteRequestManager::ClearBlockSections()
{
    for (s32 liCheckpointIndex = 0; liCheckpointIndex < BrnGameState::GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE;
         ++liCheckpointIndex)
    {
        mauBlockSectionIds[liCheckpointIndex].Clear();
    }
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

    // lbDrivingAway: the destination lies BEHIND the car's heading -- `0.0 > dot(diff, dir)`.
    // ⛔ CORRECTED (FX-AINAN2, crash parity 2026-09-24): this compared against 200.0, on the note
    // that "both operands come from the same {200.0,0,0,0} literal buffer used just below". They
    // do not. The buffer (var_80) holds flt_82001CC0 (0.0) at the compare -- 0x82788D68 lfs /
    // 0x82788D70 stfs -> 0x82788D88 lvx128 / 0x82788D8C vspltw -> 0x82788DA0 `vcmpgtfp. v0, 0.0,
    // dot` (CR6 all-true, extrwi 1,24 @0x82788DB8) -- and flt_820C4318 (200.0) is stored into it
    // only AFTER, at 0x82788DBC/0x82788DC0, for the countryside test. A NaN dot is not > (false,
    // as below).
    const f32  lfHeadingDot  = lDiff.x * lDir.x + lDiff.y * lDir.y;
    const bool lbDrivingAway = 0.0f > lfHeadingDot;

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

// [FLAG PC witness] NOT IN THE X360 BINARY. THE QUEUE-BUDGET CONTROL.
// AddEventSafe (X360 @0x8277AFD8 for the 64-byte ExtrapolatedRouteRequest, @0x8277AF20 for the
// 128-byte RaceRouteRequest) is the BOUNDS-GATED append: when miLength == miMaxLength it returns
// false and DROPS the request with no assert and no log line. The transient "Route" input buffer
// holds EventQueue<ExtrapolatedRouteRequest,12> and EventQueue<RaceRouteRequest,1> (Construct
// @0x8278A028 / @0x82789FB8), so a six-car Road Rage fits the extrapolated queue with room to
// spare -- but ONE race-route request per frame is the whole race budget, and a silent drop
// there looks exactly like "the car never asked". The console ignores the return value; these
// witnesses only observe it. DELETE-WHEN rivals are seen driving their own routes.
static void WitnessDroppedRequest(const char* lpcQueue, s32 liRaceCarIndex)
{
    static s32 siDropWitnessCount = 0;
    if (siDropWitnessCount < 8 && CgsDev::Log::gpDebugPrint != 0)
    {
        ++siDropWitnessCount;
        *CgsDev::Log::gpDebugPrint
            << "[route-req] DROPPED: the " << lpcQueue << " request queue was FULL when car "
            << liRaceCarIndex << " asked -- AddEventSafe returned false, the request is lost"
            << " [FLAG PC witness]\n";
    }
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

        // 0x82791600..0x82791678: slot = this + 36*checkpoint; for (i < slot.GetLength()) -- the
        // length is re-read (and its "Array used before Construct/Clear" assert re-run, CgsArray.h
        // :336) every pass -- AddBlockSectionId(slot.GetItem(i)) (int_8_::GetItem, the checked get).
        const Array<u32, 8u>& lrBlockSectionIds = mauBlockSectionIds[liCheckpoint];
        for (u32 luIndex = 0; luIndex < lrBlockSectionIds.GetLength(); ++luIndex)
        {
            lRouteRequest.AddBlockSectionId(lrBlockSectionIds.GetItem(luIndex));
        }
    }

    // No U-turns: also block the section behind the car (by its AISection id).
    if (leAllowUTurns == E_NO_U_TURNS)
    {
        const u16 luSectionBehind = ComputeSectionBehind(lpAICar, lpAISectionData);
        lRouteRequest.AddBlockSectionId(lpAISectionData->GetAISection(luSectionBehind)->mId);
    }

    if (!lpRouteInputBuffer->GetRaceRouteRequestQueue()->AddEventSafe(lRouteRequest))
    {
        WitnessDroppedRequest("race-route(1-slot)", static_cast<s32>(luRaceCarIndex));
    }
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

    if (!lpRouteInputBuffer->GetRaceRouteRequestQueue()->AddEventSafe(lRouteRequest))
    {
        WitnessDroppedRequest("race-route(1-slot)", liRaceCarIndex);
    }
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

    if (!lpRouteInputBuffer->GetExtrapolatedRouteRequestQueue()->AddEventSafe(lRouteRequest))
    {
        WitnessDroppedRequest("extrapolated(12-slot)", static_cast<s32>(luRaceCarIndex));
    }
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

    if (!lpRouteInputBuffer->GetExtrapolatedRouteRequestQueue()->AddEventSafe(lRouteRequest))
    {
        WitnessDroppedRequest("extrapolated(12-slot)", static_cast<s32>(luRaceCarIndex));
    }
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

    // The draw is RandomInt(0, n - 1), the SIGNED bounded draw (crash parity FX-GATE, read by address):
    //   0x82769608 lwz muNumSections (+0x30) ; 0x8276960C clrlwi 16 -> the section count as a u16
    //   0x82769610 addi -1 -> liMax ; 0x82769618 cmpwi liMax, 0 ; bge past "liMax >= liMin" (0x82014D68,
    //              li r5 0x140 = CgsRandom.h:320 -- RandomInt's assert, not RandomUInt's)
    //   0x82769640 addi 1 -> luMod ; 0x82769644 cmplwi ; bne past "luMod > 0" (li r5 0x143 = :323)
    //   0x8276968C the OLD seed's high word ; 0x8276969C std the step ; 0x827696A0 divwu / mullw / subf
    //              -> draw % n ; 0x827696AC clrlwi 16
    // The old RandomUInt(0, n - 1) spelling reduced by n - 1 while RandomUInt(min, max) was one short,
    // so the last section was never drawn (and with one section the seed did not step).
    u16 luSectionIndex = static_cast<u16>(mRandom.RandomInt(0, static_cast<s32>(static_cast<u16>(luNumSections)) - 1));

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

    // [FLAG PC witness] per-AI-car-index budget -- see the banner on KI_ROUTE_REQ_WITNESS_PER_CAR.
    // saiRouteReqTick counts the frames THIS car was eligible, so the periodic sample below keeps
    // reporting long after a car's opening burst is spent (a rival that starts fine and then goes
    // quiet is exactly the failure a burst-only witness cannot see).
    static s32 saiRouteReqWitness[35] = { 0 };
    static s32 saiRouteReqTick[35]    = { 0 };
    static s32 siRouteReqWitnessTotal = 0;

    for (s32 liAICarIndex = 0; liAICarIndex < 35; ++liAICarIndex)
    {
        // The console's `addi r31, r31, 0x1560` roster step is sizeof(AICar) on the console;
        // on this host the subscript IS that step (sizeof(AICar) is static_asserted == 5472 in
        // BrnAICar.h, but the subscript stays correct even if that ever changes).
        AICar* lpAICar = &lpaAICars[liAICarIndex];

        // Eligible cars are IN_RANGE(0) or OUT_OF_RANGE(1); INACTIVE cars are skipped.
        // (X360 0x82797FDC: `lwz r11, -0x6C(car+0x1534)` == meCarState @+0x14C8; `beq`/`cmpwi 1`.)
        const EAICarState leState = lpAICar->GetState();
        const bool lbStateEligible =
            (leState == E_AI_CAR_STATE_IN_RANGE || leState == E_AI_CAR_STATE_OUT_OF_RANGE);

        // The console evaluates these two IN THIS ORDER and short-circuits on each
        // (0x8279800C `bl NeedsNewRoute`, then 0x8279801C `lhz 0(r31)` / `lhz -2(r31)` ==
        // GetBestSectionIndex's best-then-default fallback). NeedsNewRoute @0x82765F80 is a pure
        // read, and GetBestSectionIndex is two u16 loads, so hoisting both into named locals for
        // the witness below changes no behaviour -- the guarded call order is unchanged.
        const bool lbNeedsNewRoute = lbStateEligible && lpAICar->NeedsNewRoute();
        const u16  luBestSection   = lbStateEligible ? lpAICar->GetBestSectionIndex()
                                                     : AICar::KI_INVALID_SECTION_INDEX;
        const bool lbHasSection    = (luBestSection != AICar::KI_INVALID_SECTION_INDEX);
        const bool lbRequesting    = lbStateEligible && lbNeedsNewRoute && lbHasSection;

        // ---- [FLAG PC witness] NOT IN THE X360 BINARY ------------------------------------
        // One line per car, naming EVERY term of the console's three-part gate, so a run can
        // never again leave "the rivals asked but were refused" and "the rivals never asked"
        // indistinguishable. Printed only for cars the console would even look at
        // (state IN_RANGE/OUT_OF_RANGE), which is at most the six cars in a Road Rage.
        // DELETE-WHEN rivals are seen driving their own routes.
        const bool lbWitnessBurst =
            lbStateEligible && (saiRouteReqWitness[liAICarIndex] < KI_ROUTE_REQ_WITNESS_PER_CAR);
        const bool lbWitnessSample =
            lbStateEligible && ((saiRouteReqTick[liAICarIndex] % KI_ROUTE_REQ_WITNESS_PERIOD) == 0);

        if (lbStateEligible)
        {
            ++saiRouteReqTick[liAICarIndex];
        }

        if ((lbWitnessBurst || lbWitnessSample)
            && siRouteReqWitnessTotal < KI_ROUTE_REQ_WITNESS_TOTAL
            && CgsDev::Log::gpDebugPrint != 0)
        {
            ++siRouteReqWitnessTotal;
            ++saiRouteReqWitness[liAICarIndex];

            const char* lpcVerdict = "REQUEST";
            if (!lbNeedsNewRoute)
            {
                lpcVerdict = "skip: NeedsNewRoute()==false (route still good, or crashing, or no section)";
            }
            else if (!lbHasSection)
            {
                lpcVerdict = "skip: best AND default section are both 0x7FFF (no road under the car)";
            }

            *CgsDev::Log::gpDebugPrint
                << "[route-req] car " << liAICarIndex
                << " raceCarIdx " << lpAICar->GetRaceCarIndex()
                << " state " << static_cast<s32>(leState)
                << " style " << static_cast<s32>(lpAICar->GetRouteFindingStyle())
                << " needsNewRoute " << (lbNeedsNewRoute ? 1 : 0)
                << " best " << static_cast<s32>(lpAICar->muBestSectionIndex)
                << " default " << static_cast<s32>(lpAICar->muDefaultSectionIndex)
                << " dest " << static_cast<s32>(lpAICar->GetDestinationSectionIndex())
                << " hasRoute " << (lpAICar->HasValidRoute() ? 1 : 0)
                << " -> " << lpcVerdict << " [FLAG PC witness]\n";
        }

        if (lbRequesting)
        {
            GenerateRoute(lpAICar, lpPlayerCar, lpaAICars, lpAISectionData,
                          lpRouteInputBuffer, lpBuzzByManager);
        }
    }
}
}
