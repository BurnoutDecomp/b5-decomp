// ============================================================================
// BrnWorld::RaceCarEntityModule -- THE RIVAL SPAWN SLICE (rival-spawn wave R, 2026-09-02).
//
//   RaceCarEntityModule::SetUpAIForMode   X360 0x82301620   (DWARF BrnRaceCarEntityModule.h:779)
//   RaceCarEntityModule::RemoveRivals     X360 0x82305E00   (DWARF :782)
//
// ⭐ WHY THIS SLICE EXISTS. HandlePrepareForModeAction @0x823092F0 -> SetupOpponents
// @0x82307DF0 is the console's "populate the grid" chain, and SetupOpponents had BOTH of its
// rival legs parked ("no declaration and no body"). So an offline Road Rage started and ran with
// the player alone on the grid: ModeManager::SetupOpponentData @0x82329348 had no records to
// publish (it was itself a comment) and nothing in the world module would have spawned a car
// from them anyway. This partfile is the world-module end of that chain -- one physical AI race
// car per OpponentData record, seated on its StartLocation, attached to an active slot and
// announced to the AI module.
//
// SOURCE: BURNOUT_X360_ARTIST.XEX, raw asm. The pseudocode for 0x82301620 is flagged "local
// variable allocation has failed" and renders SpawnRaceCar with twenty-two arguments and the
// two vector loads as inline `__asm`; every claim below is from the assembly listing.
//
// [!] THE LOOP BOUND IS miNumRivals, NOT THE OPPONENT ARRAY'S LENGTH. `lbz r11, 0(r25); extsb`
// at entry (stashed at sp+0x70 and re-read at the loop's foot @0x82301A20) is
// GameModeParams::GetOpponentCount(), which the console implements over the mode's rival count
// byte; the same byte is re-read inside GetOpponentData's :1154 assert every pass. The
// Array<OpponentData,7>'s own count word (params+0x838) is never read here.
// [!] THE UP VECTOR IS A CONSTANT. unk_82181510 (`lvx128 v126` @0x823017E8, passed as v3 to
// BuildTransform) dumps from the image as {0.0f, 1.0f, 0.0f, 0.0f}: the world Y axis, the same
// row SetUpPlayerCarForMode and HandleResetPlayerCarAction feed BuildTransform.
// [!] THE TRANSFORM IS COPIED TO THE STACK AND READ BACK BY ROW. BuildTransform writes sp+0x100;
// the four rows are copied to sp+0xC0..0xF0 (`stvx128` x4 @0x82301888..0x823018B4), sp+0xC0 is
// what SpawnRaceCar receives as lrTransform (r5 @0x823018E0), and rows 2 and 3 (sp+0xE0 == zAxis,
// sp+0xF0 == wAxis) are reloaded @0x82301948/0x82301950 as SetUpOutOfRangeRaceCar's lAt/lPosition.
// [!] GetActiveRaceCar(leActiveRaceCarIndex) @0x8230193C is CALLED AND ITS RESULT DISCARDED
// (r3 is overwritten by the next `mr r3, r19` before any use). Kept, because it is the console's
// call and it carries the slot-range assert; the pointer is not used, exactly as on console.
// ============================================================================
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCar.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h" // OutputBuffer_PreScene::GetRaceCarAIInterface
#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"          // RaceCarAIInterface / AddCarToCurrentModeEvent / E_EVENT_ADD_CAR_TO_MODE
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h" // GameModeParams / OpponentData / StartLocation
#include "GameSource/GameState/BrnOpponentData.h"                         // BrnGameState::OpponentData
#include "SharedClasses/Progression/BrnRaceEventData.h"                   // EventStartGridSlot::E_FLAG_CAN_DEVIATE_FROM_ROUTE
#include "GameSource/Math/BrnMathUtils.h"                                 // BrnMath::BuildTransform
#include "SharedClasses/World/BrnWorldRegion.h"                           // BrnWorld::E_DISTRICT_INVALID
#include "GameSource/BurnoutConstants.h"                                  // EGlobalRaceCarIndex / EActiveRaceCarIndex (+ the ++ guards)
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDCompress("XUSCCOB2")
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT

namespace BrnWorld
{

// --------------------------------------------------------------------------------------------
// The literals this slice carries, each X360-attested at its use site.
// --------------------------------------------------------------------------------------------

// `li r5, 0x7FFF` @0x82301964 -- SetUpOutOfRangeRaceCar's section argument: the rival is seeded
// with NO resolved AI section (the same 32767 SetupOpponents' no-checkpoint fallback uses).
static const u16 KU_RIVAL_NO_AI_SECTION = 0x7FFFu;

// `li r7, 0` @0x8230196C -- SetUpOutOfRangeRaceCar's luNumberOfMedalsToUnlock: none.
static const u8 KU_RIVAL_MEDALS_TO_UNLOCK = 0;

// unk_82181510, dumped from the ARTIST image (16 bytes @ file offset 0x181510):
// 00000000 3F800000 00000000 00000000 == {0, 1, 0, 0}. The world UP axis BuildTransform takes.
static const Vector3 KV_RIVAL_GRID_UP_AXIS = { 0.0f, 1.0f, 0.0f, 0.0f };


// ============================================================================================
// SetUpAIForMode @0x82301620
//
// Console, de-inlined (one pass of the loop, r30 == liOpponentIndex):
//   0x82301714..0x8230174C  GetOpponentData(i)   [:1154 assert + Array<OpponentData,7>::GetItem
//                                                 on params+0x528 -> r28]
//   0x82301750..0x823017DC  maStartLocations count (params+0x250): CgsArray.h:336 constructed
//                           assert, then "Missing Starting grid for mode\n" (:8718) when <= 0
//   0x823017E0..0x82301820  v126 = UP; GetStartDirection(i) [:1185 assert; lvx128 v127, r3, 0x10]
//   0x82301824..0x82301868  GetStartPosition(i)  [:1177 assert; lvx128 v1, r0, r11]
//   0x8230186C              BuildTransform(sp+0x100, pos, dir, up)
//   0x82301870..0x823018B4  copy the 4 rows to sp+0xC0 (the SpawnRaceCar transform)
//   0x82301878..0x823018D8  GetFlag(1 << 34) ? CgsIDCompress("XUSCCOB2") : ld 0(r28)
//   0x823018DC..0x8230190C  SpawnRaceCar(GetRaceCarAIInterface(), &sp+0xC0, 1 == AI, modelId,
//                           r8 = 0 keepResetSection, r9 = 0 wheel, r10 = 0 rival id,
//                           sp+0x54 = liOpponentIndex)
//   0x82301910..0x8230192C  AttachActiveRaceCar(GetGlobalRaceCar(global), -1)
//   0x8230193C              GetActiveRaceCar(active)   -- result unused (see banner)
//   0x82301940..0x82301970  SetUpOutOfRangeRaceCar(global, wAxis, zAxis, 0x7FFF, 18, 0)
//   0x82301974..0x823019B4  AddCarToCurrentModeEvent {global, i, a18, (slot.flags & 1),
//                           params+4 (mfProgressionRankAsRatio), params+0x74+4i
//                           (mfOvertakingDifficulty[i])} -> AddEvent(type 6)
//   0x823019B8..0x82301A14  SetActiveRaceCarForPlayerScoringIndex(i, active)
//                           [the :1203/:1205 asserts are its inline body; the store is
//                            this+0x187BC+4i == maActiveRaceCarForPlayerScoringIndex[i]]
// ============================================================================================
void RaceCarEntityModule::SetUpAIForMode(
    const BrnGameState::GameModeParams* lpGameModeParams,
    RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput,
    u16 lu16DestinationSectionIndex)
{
    // `lbz r11, 0(r25); extsb; stw sp+0x70` -- the bound is taken once, before the loop.
    const s32 liOpponentCount = lpGameModeParams->GetOpponentCount();

    for (s32 liOpponentIndex = 0; liOpponentIndex < liOpponentCount; ++liOpponentIndex)
    {
        // The :1154 assert and the Array constructed/bounds pair are GetOpponentData's own.
        const BrnGameState::OpponentData* lpOpponentData =
            lpGameModeParams->GetOpponentData(liOpponentIndex);

        // 0x82301750..0x823017DC. GetStartLocationCount() carries the CgsArray.h:336 constructed
        // assert the console inlines alongside it; the streamed "Missing Starting grid for mode"
        // message (BrnRaceCarEntityModule.cpp:8718) is this call site's, trailing newline and
        // all. An assert is not a guard: the console falls through into the slot reads below.
        CGS_ASSERT(lpGameModeParams->GetStartLocationCount() > 0, "Missing Starting grid for mode\n");

        // Direction first (v127 @0x82301820), then position (v1 @0x82301868) -- the console's
        // order; each accessor carries its own :1185/:1177 range assert.
        const Vector3 lStartDirection = lpGameModeParams->GetStartDirection(liOpponentIndex);
        const Vector3 lStartPosition  = lpGameModeParams->GetStartPosition(liOpponentIndex);

        Matrix44Affine lGridTransform;
        BrnMath::BuildTransform(lGridTransform, lStartPosition, lStartDirection,
                                KV_RIVAL_GRID_UP_AXIS);

        // 0x82301878..0x823018D8: the cop-car substitution, the same 1 << 34 mask
        // SetupOpponentData tests (the OpponentData already carries the cop id when the flag
        // was set at publish time; the console tests it again here regardless).
        CgsID lCarModelId;
        if (lpGameModeParams->GetFlag(BrnGameState::GameModeParams::KU_FLAG_SET_OPPONENTS_TO_COPS))
        {
            lCarModelId = CgsIDCompress("XUSCCOB2");
        }
        else
        {
            lCarModelId = lpOpponentData->GetCarModelId();                  // ld r31, 0(r28)
        }

        // 0x823018DC..0x8230190C. r6 == 1 == E_RACE_CAR_TYPE_AI; r8/r9/r10 == 0 (no
        // keep-reset-section, wheel set resolved by SpawnRaceCar from the model, no rival id);
        // the opponent index goes in the stack slot (`stw r30, sp+0x54`).
        const EGlobalRaceCarIndex leGlobalRaceCarIndex =
            SpawnRaceCar(lpOutput->GetRaceCarAIInterface(), lGridTransform,
                         E_RACE_CAR_TYPE_AI, lCarModelId, false,
                         0 /* lWheelModelId: resolved from the model */,
                         0 /* lpRivalId: none */,
                         liOpponentIndex);

        // 0x82301910..0x8230192C: `li r5, -1` -- let AttachActiveRaceCar pick the first free slot.
        const EActiveRaceCarIndex leActiveRaceCarIndex =
            AttachActiveRaceCar(GetGlobalRaceCar(leGlobalRaceCarIndex),
                                E_ACTIVE_RACE_CAR_INDEX_INVALID);

        // 0x8230193C: called for its slot-range assert; the pointer is not consumed (banner).
        (void)GetActiveRaceCar(leActiveRaceCarIndex);

        // 0x82301940..0x82301970: rows 3 and 2 of the STACK COPY of the grid transform.
        lpOutput->GetRaceCarAIInterface()->SetUpOutOfRangeRaceCar(
            leGlobalRaceCarIndex,
            lGridTransform.wAxis,                   // lvx128 v126 <- sp+0xF0  (v1, lPosition)
            lGridTransform.zAxis,                   // lvx128 v127 <- sp+0xE0  (v2, lAt)
            KU_RIVAL_NO_AI_SECTION,                 // li r5, 0x7FFF
            E_DISTRICT_INVALID,                     // li r6, 0x12 == 18
            KU_RIVAL_MEDALS_TO_UNLOCK);             // li r7, 0

        // 0x82301974..0x823019B4: six stores in the console's own order. The deviate flag is
        // bit 0 of the OpponentData's start-grid slot flags (`lbz r11, 0x19(r28); clrlwi 31`),
        // the rank ratio is params+4 and the overtaking difficulty is this opponent's entry of
        // the params' per-car table (`lfs f31, 0(r17)`, r17 == params+0x74 stepping by 4).
        BrnAI::AIModuleIO::AddCarToCurrentModeEvent lAddCarEvent;
        lAddCarEvent.meGlobalRaceCarIndex     = leGlobalRaceCarIndex;
        lAddCarEvent.miOpponentIndex          = liOpponentIndex;
        lAddCarEvent.muDestinationAISection   = lu16DestinationSectionIndex;
        lAddCarEvent.mbDeviateFromRoute       = lpOpponentData->GetStartGridSlot()->GetFlag(
            BrnProgression::EventStartGridSlot::E_FLAG_CAN_DEVIATE_FROM_ROUTE);
        lAddCarEvent.mfProgressionRankAsRatio = lpGameModeParams->mfProgressionRankAsRatio;
        lAddCarEvent.mfOvertakingDifficulty   = lpGameModeParams->mfOvertakingDifficulty[liOpponentIndex];

        lpOutput->GetRaceCarAIInterface()->mManagementQueue
            .AddEvent<BrnAI::AIModuleIO::AddCarToCurrentModeEvent>(
                &lAddCarEvent, BrnAI::AIModuleIO::E_EVENT_ADD_CAR_TO_MODE);   // li r5, 6

        // 0x823019B8..0x82301A14: the :1203/:1205 asserts are SetActiveRaceCarForPlayerScoringIndex's
        // own inline body, and the store lands in maActiveRaceCarForPlayerScoringIndex[i]
        // (`stw r29, 0(r11)`, r11 walking this+0x187BC by 4). Rival i takes scoring slot i; the
        // player took slot miNumRivals in SetUpPlayerCarForMode.
        SetActiveRaceCarForPlayerScoringIndex(
            static_cast<BrnGameState::GameStateModuleIO::EPlayerScoringIndex>(liOpponentIndex),
            leActiveRaceCarIndex);
    }
}


// ============================================================================================
// RemoveRivals @0x82305E00
//
// Console (r30 == the global index, r24 == lbRemovePlayerCar, r31 == the RaceCar):
//   do {
//       r31 = GetGlobalRaceCar(r30);
//       if (!r24) { [BrnRaceCar.h:577 type assert] if (muType == 0 /* PLAYER */) skip; }
//       [BrnRaceCar.h:547 type assert]  if (muType != 3 /* INACTIVE */)          == IsInWorld()
//       [BrnRaceCar.h:590 type assert]      if (muType != 2 /* NETWORK */)        == !IsNetworkDriven()
//                                                RemoveRaceCar(r30, lpOutput);
//       ++r30; [BurnoutConstants.h:84 "leEnumIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT"]
//   } while (r30 < 35);
// The three baked BrnRaceCar.h lines are the inline bodies of IsPlayerDriven / IsInWorld /
// IsNetworkDriven (each guarded by "muType < E_RACE_CAR_TYPE_COUNT" on the console); this tree's
// inline predicates carry no assert, so none is restated here. The :84 guard is the
// EGlobalRaceCarIndex post-increment operator's (BurnoutConstants.h).
// ============================================================================================
void RaceCarEntityModule::RemoveRivals(
    RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput,
    bool lbRemovePlayerCar)
{
    for (EGlobalRaceCarIndex leGlobalRaceCarIndex = E_GLOBAL_RACE_CAR_INDEX_0;
         leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT;
         leGlobalRaceCarIndex++)
    {
        const RaceCar* lpRaceCar = GetGlobalRaceCar(leGlobalRaceCarIndex);

        if (!lbRemovePlayerCar && lpRaceCar->IsPlayerDriven())              // :577
        {
            continue;
        }

        if (lpRaceCar->IsInWorld() && !lpRaceCar->IsNetworkDriven())        // :547 / :590
        {
            RemoveRaceCar(leGlobalRaceCarIndex, lpOutput);
        }
    }
}

} // namespace BrnWorld
