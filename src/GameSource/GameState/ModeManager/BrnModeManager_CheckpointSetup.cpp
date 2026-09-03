// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager_CheckpointSetup.cpp
// ============================================================================
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
// Wave-B keystone, AGENT 4 -- the checkpoint / opponent SETUP quartet that
// ModeManager::StartGameMode @0x8234FCE8 calls back-to-back at 0x8234FE3C..0x8234FE68:
//
//   ModeManager::SetUpCheckPointsForGameMode   X360 0x82328BC8
//   ModeManager::SetupPathfinding              X360 0x823291B0
//   ModeManager::SetupOpponentData             X360 0x82329348
//   ModeManager::SetupCheckpointDistricts      X360 0x823296F0
//
// [!!] THE ONE FACT THAT FRAMES THIS WHOLE FILE (and corrects the wave brief):
// THREE of the four open with the SAME open-coded game-mode gate --
//     `if (leGameModeType == E_MODE_OFFLINE_RACE || == E_MODE_MARKED_MAN || == E_MODE_BURNING_ROUTE)`
// (asm: `lwz r11, 0x148(params); cmpwi 0 / cmpwi 8 / cmpwi 5`) -- and do NOTHING for any
// other mode. E_MODE_STUNT_ATTACK is 7, so for a STUNT RACE SetUpCheckPointsForGameMode,
// SetupPathfinding and SetupCheckpointDistricts are inert AT THE GATE: they never reach
// their loops at all. (The grouping sheet's "stunt events have checkpointCount == 0 -- the
// loop runs zero times" is true but understates it -- the loop is not even reached.)
// SetupOpponentData has no mode gate; it gates on `lpStartGameModeParams->GetEventData()`
// and then loops muStartGridCount times.
//
// [!] HEADLINE CORRECTED 2026-08-26 (fix round). This banner used to end
//     "=> Every PARKED leg below is off the stunt-race path. None of them blocks the campaign."
// THAT IS FALSE FOR SetupOpponentData, and a conductor reading it would wrongly conclude the whole
// file is stunt-neutral. The accurate statement, re-derived from StartGameMode's call block
// 0x8234FE00..0x8234FE68:
//   * SetUpCheckPointsForGameMode @0x8234FE3C is GUARDED -- `lwz r11, 0xD98(r31)` /
//     `lbz r11, 0xAC(r11)` / `cmplwi 0` / `bne` skips it when the mode's +0xAC byte is set. (That
//     byte is GameMode::mbIsOnline, per this wave's batch-3 finding M2; the tree still transcribes
//     it as mbConstructed.) It is additionally mode-gated to {RACE, MARKED_MAN, BURNING_ROUTE}
//     inside its own body, so a stunt race never enters it. Its two parked publishes are genuinely
//     off the stunt path -- but they are ON the path for every race / marked-man / burning-route
//     event, which start with an EMPTY checkpoint array and stale per-car checkpoint progress.
//   * SetupPathfinding @0x8234FE4C and SetupCheckpointDistricts @0x8234FE68 are called
//     UNCONDITIONALLY but carry the same {0,8,5} mode gate internally, so their parked legs are
//     off the stunt path.
//   * SetupOpponentData @0x8234FE5C is called UNCONDITIONALLY and HAS NO MODE GATE. For a stunt
//     race the parked start-grid loop IS ON THE PATH. Whether it does anything depends on the
//     event's muStartGridCount (RaceEventData +0xE8) -- WHICH THIS FILE HAS NOT CHECKED, because
//     that is authored data, not something the image settles. Treat it as unknown, not as zero.
//
// Reconstructed from the export ASSEMBLY (the pseudocode for 0x82328BC8 renders the two
// 32-bit register arguments as one `__int64 a1` and 0x82329348 is flagged "local variable
// allocation has failed" -- hazards H9). Argument identity is pinned from the asm plus the
// caller: r3 = this, r4 = the StartGameModeParams*, r5 = the GameModeParams*, and for
// SetUpCheckPointsForGameMode a THIRD argument really is passed (`mr r6, r26` at
// 0x8234FE2C) which this callee never reads -- so the frozen header's 3-argument
// declaration is right and the third parameter is deliberately unused here.
//
// [X] Nothing from hazards H2's list of 16 committed bodies is re-implemented here.
// ============================================================================

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include "GameSource/GameState/BrnGameStateModule.h"                       // GetDistrictMap / GetActivePlayerCarId
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // AddLandmarkIndexForGameMode (BODIED)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"        // FindLandmarkAISectionIndex / GetProgressionData / GetProgressionRank
#include "SharedClasses/Progression/BrnProgressionData.h"                  // FindCarOpponentSet / GetInterpolatedAIBalanceGraph
#include "SharedClasses/Progression/BrnOpponentData.h"                     // CarOpponentSet / CarOpponent
#include "SharedClasses/Progression/BrnRaceBalance.h"                      // OpponentBalanceData (68 B, by value)
#include "SharedClasses/Progression/BrnRaceEventData.h"                    // RaceEventData / its CheckpointData
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDCompress("XUSCCOB2")
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"                    // WorldMap2D::GetValue + KU_INVALID_WORLD_MAP_VALUE
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // gpDebugPrint / gxMessageFilterFlags

namespace BrnGameState
{

// ============================================================================
// ModeManager::SetUpCheckPointsForGameMode -- X360 0x82328BC8
// ============================================================================
// Walks the event's checkpoint table, resolves each checkpoint's landmark id to a live
// TriggerData Landmark, and publishes {region index, AI-section index} into the
// GameModeParams checkpoint array + the TriggerQueryManager's per-mode landmark set.
//
// Console call graph (all in-order):
//   TriggerData::GetMemory(TQM + 0x620)                 -> GetTriggerData()      [accessor grow 5]
//   RaceEventData::GetCheckpointData(i)->GetLandmarkId()  (INLINED on console: the
//       "liCheckpointIndex >= 0 && liCheckpointIndex < miCheckpointCount" assert baked at
//       BrnRaceEventData.h:953 IS that accessor's own body -- de-inlined here to the real
//       call at 0x8230F808, so the assert is NOT duplicated at this call site)
//   TriggerData::FindLandmark(id)                        0x82675738
//   ProgressionManager::FindLandmarkAISectionIndex(id)   0x82359AE0
//   Array<CheckpointData,16>::Append(params + 0x260, &e) 0x82317B30   [PARKED -- see below]
//   TriggerQueryManager::AddLandmarkIndexForGameMode(ix) 0x823265E8
//
// [!] The frozen header's comment on mauLandmarkSectionIndices says this function writes it
// (via FindLandmarkAISectionIndex). IT DOES NOT: the asm's only consumer of the AI-section
// index is the CheckpointData element it appends to the GameModeParams array (`sth r28,
// 0x140+var_CE(r1)`). There is no store to this+32448 anywhere in 0x82328BC8. Reported.
// ============================================================================
void ModeManager::SetUpCheckPointsForGameMode(const StartGameModeParams* lpStartGameModeParams,
                                              GameModeParams*            lpGameModeParams,
                                              ScoringSystem*             lpScoringSystem)
{
    // lpScoringSystem: the console caller loads r6 (`mr r6, r26` @0x8234FE2C) but this body
    // never touches it. Kept in the signature because the caller passes it (and the DWARF
    // declares it); deliberately unused -- do not "fix" by dropping the parameter.
    (void)lpScoringSystem;

    // The checkpoint-mode gate (see the file banner). Console: `lwz r11, 0x148(r5)` then the
    // three compares against 0 / 8 / 5, open-coded here exactly as the console does.
    const GameStateModuleIO::EGameModeType leGameModeType = lpGameModeParams->GetGameModeType();
    if (leGameModeType != GameStateModuleIO::E_MODE_OFFLINE_RACE &&
        leGameModeType != GameStateModuleIO::E_MODE_MARKED_MAN &&
        leGameModeType != GameStateModuleIO::E_MODE_BURNING_ROUTE)
    {
        return;
    }

    const BrnTrigger::TriggerData* lpTriggerData = GetTriggerData();
    CGS_ASSERT(lpTriggerData != nullptr, "lpTriggerData != NULL");   // BrnModeManager.cpp:4912

    const BrnProgression::RaceEventData* lpRaceEventData = lpStartGameModeParams->GetEventData();
    const s32 liCheckPointCount = lpRaceEventData->GetCheckpointCount();

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "liCheckPointCount: " << liCheckPointCount << "\n";
    }

    for (s32 liCurrentCheckPoint = 0; liCurrentCheckPoint < liCheckPointCount; ++liCurrentCheckPoint)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "liCurrentCheckPoint: " << liCurrentCheckPoint << "\n";
        }

        const u32 liCheckPointLandmarkId =
            lpRaceEventData->GetCheckpointData(liCurrentCheckPoint)->GetLandmarkId();

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "liCheckPointLandmarkId: " << liCheckPointLandmarkId << "\n";
        }

        const BrnTrigger::Landmark* lpLandmark =
            lpTriggerData->FindLandmark(static_cast<CgsID>(liCheckPointLandmarkId));
        CGS_ASSERT(lpLandmark != nullptr, "lpLandmark");             // BrnModeManager.cpp:4932

        // Console: `lwz r11, 0x24(r31)` == TriggerRegion::mId (the landmark's CgsID, fed to the
        // AI-section lookup) and `lhz r29, 0x28(r31)` == TriggerRegion::miRegionIndex (the
        // 16-bit REGION index that becomes the checkpoint's LandmarkIndex). Both offsets match
        // BrnTriggerBase.h's committed layout exactly (mId +0x24, miRegionIndex +0x28).
        const s32 lLandmarkIndex = lpLandmark->GetRegionIndex();
        const u16 luLandmarkAISection =
            GetProgressionManager()->FindLandmarkAISectionIndex(lpLandmark->GetId());

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "luLandmarkAISection: "
                                       << static_cast<u32>(luLandmarkAISection) << "\n";
            *CgsDev::Log::gpDebugPrint << "lLandmarkIndex: " << lLandmarkIndex << "\n";
        }

        // --------------------------------------------------------------------
        // [X] PARKED PUBLISH 1 of 4 -- HEADER GAP, NOT A SCOPE DECISION.
        // Console (an INLINED GameModeParams::AddCheckpoint, DWARF BrnGameModeParams.h:388,
        // whose body is CheckpointData::Construct + the array Append):
        //     CheckpointData lCheckpointData;                       // stack, 44 B
        //     lCheckpointData.Construct(LandmarkIndex(lLandmarkIndex), luLandmarkAISection);
        //         // sth landmark@+0 / sth aisection@+2 / stw 18(E_DISTRICT_INVALID)@+4 /
        //         // stw 0 @+40 (the block-section Array's count word)
        //     Array<CheckpointData,16>::Append(lpGameModeParams + 0x260, &lCheckpointData);
        // BrnGameState::CheckpointData already carries that exact Construct() inline
        // (BrnCheckpointData.h:17-23, including the district == 18 default), but
        // GameModeParams::maCheckpointDataArray is PRIVATE and this tree's GameModeParams
        // declares NEITHER AddCheckpoint NOR a non-const GetCheckpointData -- the DWARF
        // declares both (:388 / :399) and the SIBLING StartGameModeParams already has
        // AddCheckpoint committed (BrnGameModeParams.h:307). A raw-offset reach-around is NOT
        // an option: that header states outright that its byte offsets are not x64-faithful.
        // DELETE-WHEN header_request #1 lands, replacing this block with the one line:
        //     lpGameModeParams->AddCheckpoint(LandmarkIndex(lLandmarkIndex), luLandmarkAISection);
        // [!] CONSEQUENCE WHILE PARKED (stated plainly 2026-08-26, fix round -- it was previously
        // only implied): every RACE / MARKED_MAN / BURNING_ROUTE event starts with an EMPTY
        // GameModeParams checkpoint array. Everything downstream that walks that array does
        // nothing -- SetupPathfinding's block-section loop, SetupCheckpointDistricts' whole loop,
        // and the :5012 audit assert below, which is why that assert fires by construction.
        // STATUS 2026-08-26 (CLOSURE round) -- THE DECLARATION HALF LANDED; THE PARK STILL STANDS,
        // ON A DIFFERENT BLOCKER. The old text here ("header_request #1 has NOT landed ...
        // AddCheckpoint on the SIBLING StartGameModeParams only") is STALE and was corrected this
        // round. class GameModeParams now declares all three (BrnGameModeParams.h:226 AddCheckpoint,
        // :227/:228 the const + non-const GetCheckpointData, and :309 SetAStarDistanceFunction).
        // THE REAL RESIDUE IS THE BODY: BrnGameModeParams.cpp defines only Construct,
        // GetCheckpointCount (:109), GetStartLocationCount, GetStartPosition, GetStartDirection and
        // AddStartLocation (:176) for this class -- GameModeParams::AddCheckpoint and BOTH
        // GetCheckpointData overloads are DECLARE-ONLY, so un-parking the one line below compiles
        // clean (selfcheck is /c only, it cannot see this) and then takes LNK2019 at the mount.
        // DELETE-WHEN those bodies land in BrnGameModeParams.cpp -- not when the header moves again.
        // --------------------------------------------------------------------

        mpTriggerQueryManager->AddLandmarkIndexForGameMode(LandmarkIndex(lLandmarkIndex));

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "lLandmarkIndex: " << lLandmarkIndex << "\n";
        }
    }

    // Console `stw r30, 0x144(r31)` -- unconditional inside the mode gate (it also runs when
    // the event has zero checkpoints). +0x144 is muNumberOfCheckpointsInEvent: it is the word
    // immediately below meGameModeType@+0x148, and GameModeParams::Construct @0x8231C370
    // zeroes it at 0x8231C414. NOTE this is a DIFFERENT quantity from GetCheckpointCount(),
    // which reads the checkpoint ARRAY's count word @+0x520 (X360 0x822B1EF0).
    lpGameModeParams->muNumberOfCheckpointsInEvent = static_cast<u32>(liCheckPointCount);

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "liCheckPointCount: " << liCheckPointCount << "\n";
        *CgsDev::Log::gpDebugPrint << "**********************************************\n";
    }

    // ------------------------------------------------------------------------
    // [X] PARKED PUBLISH 2 of 4 -- the per-car checkpoint-progress reset.
    // Console tail (0x82329188-0x823291A4), an INLINED 8-iteration loop:
    //     addi r10, r21, 0x677C   ; r21 == this  -> this + 26492
    //     li   r11, 8
    //     loop: stw 0, 0(r10); r10 += 0x18
    // this+26492 == mScoringSystem + 22988 == ScoringSystem + 0x59CC, and the tree already
    // pins maRaceCarPositioningData at ScoringSystem+0x59C0 with stride 0x18
    // (BrnCarData.cpp:33, BrnScoringSystem_UpdateA.cpp:367). 0x59CC - 0x59C0 == 12 == the
    // fourth dword of RaceCarPositioningData == miCurrentCheckpoint (member order
    // meActiveRaceCarIndex / mfDistanceToNextCheckpoint / mfDistanceToFinish /
    // miCurrentCheckpoint / miFinishPosition / mbDisconnected). So the console line is:
    //     for (s32 liSlot = 0; liSlot < 8; ++liSlot)
    //         maRaceCarPositioningData[liSlot].miCurrentCheckpoint = 0;
    // -- i.e. "the checkpoint list was just rebuilt, so nobody is at a checkpoint yet".
    // maRaceCarPositioningData is PRIVATE to ScoringSystem and no accessor exists; hazards H9
    // forbids reaching ScoringSystem internals by raw offset off `this`.
    // DELETE-WHEN header_request #7 lands, replacing this block with:
    //     GetScoringSystem()->ResetAllCarsCurrentCheckpoint();
    // [!] CONSEQUENCE WHILE PARKED (stated plainly 2026-08-26, fix round): every RACE /
    // MARKED_MAN / BURNING_ROUTE event starts with STALE per-car checkpoint progress carried over
    // from the previous event -- all eight RaceCarPositioningData slots keep whatever
    // miCurrentCheckpoint the last mode left in them, so position/lap logic can begin the event
    // believing cars are already partway round.
    // STATUS 2026-08-26: header_request #7 has NOT landed (BrnScoringSystem.h untouched since
    // 02.08). Park stands.
    // ------------------------------------------------------------------------
}


// ============================================================================
// ModeManager::SetupPathfinding -- X360 0x823291B0
// ============================================================================
// Two independent jobs, both gated on the same checkpoint-mode test:
//   (1) translate the event's authored A* type into the GameModeParams' A* distance
//       function, and
//   (2) copy each event checkpoint's block-section list into the matching GameModeParams
//       checkpoint entry (the AI route-finder's "do not route through here" list).
//
// [!] `this` IS UNUSED by the console body (r3 is never read after the prologue) -- the
// function reaches everything through its two parameters. Kept as a member because the
// caller calls it as one and the DWARF declares it as one.
// ============================================================================
void ModeManager::SetupPathfinding(const StartGameModeParams* lpStartGameModeParams,
                                   GameModeParams*            lpGameModeParams)
{
    // Console order: the event-data null test comes FIRST, the mode gate second.
    const BrnProgression::RaceEventData* lpRaceEventData = lpStartGameModeParams->GetEventData();
    if (lpRaceEventData == nullptr)
    {
        return;
    }

    const GameStateModuleIO::EGameModeType leGameModeType = lpGameModeParams->GetGameModeType();
    if (leGameModeType != GameStateModuleIO::E_MODE_OFFLINE_RACE &&
        leGameModeType != GameStateModuleIO::E_MODE_MARKED_MAN &&
        leGameModeType != GameStateModuleIO::E_MODE_BURNING_ROUTE)
    {
        return;
    }

    // ------------------------------------------------------------------------
    // [X] PARKED PUBLISH 3 of 4 -- the A* distance-function translation.
    // Console 0x823291FC-0x82329250:
    //     lbz r11, 0xF5(lpRaceEventData)          ; RaceEventData::mu8AStarType
    //     switch (luAStarType)
    //     {
    //     case 0:  lpGameModeParams->SetAStarDistanceFunction(BrnAI::E_ASTAR_DISTANCE_EUCLIDEAN);          break;
    //     case 1:  lpGameModeParams->SetAStarDistanceFunction(BrnAI::E_ASTAR_DISTANCE_EUCLIDEAN_X_BIASED); break;
    //     case 2:  lpGameModeParams->SetAStarDistanceFunction(BrnAI::E_ASTAR_DISTANCE_EUCLIDEAN_Y_BIASED); break;
    //     default: CGS_ASSERT(false, "Unknow A* type");   // BrnModeManager.cpp:5006 -- console
    //              break;                                 // spelling, verbatim, no store on this arm
    //     }
    // The store is `stw r11, 0x854(lpGameModeParams)` and the value mapping is the identity
    // 0/1/2, i.e. the data-side A* type indexes BrnAI::AStarDistanceFunction directly
    // (BrnAStar.h:47-55). TWO gaps stop it compiling today:
    //   (a) RaceEventData::mu8AStarType is still inside maPad_EE[0x0A] and has no accessor --
    //       its seat at +0xF5 is INDEPENDENTLY confirmed by that header's own pad-run comment
    //       (BrnRaceEventData.h:311-316 lists mu8Mode..mi8UnlockRank as +0xEC..+0xF6, which
    //       puts mu8AStarType at exactly +0xF5) and by this instruction. header_request #5.
    //   (b) SetAStarDistanceFunction -- LANDED (closure round): BrnGameModeParams.h:345 now
    //       declares it. Only gap (a) still parks this leg; restate any future status against
    //       (a) alone. Park stands on (a). OFF THE STUNT PATH (the {0,8,5} gate above).
    // [!!] +0x854 IS meAStarDistanceFunction -- SETTLED AND LANDED (closure round): the
    // BrnGameModeParams.h run is corrected to 0x850 mfOnlineModeTimeLimit / 0x854
    // meAStarDistanceFunction / 0x858 miPlayerWreckCount (evidence: SetupPathfinding's integer
    // `stw r11, 0x854` @0x82329250; UpdateCurrentMode's `lfs f0, 0x850` @0x823512FC under the
    // "GetOnlineTimeLimit() > 0.0f" assert; Construct writes 0x854/0x858 and never 0x850).
    // GetAStarDistanceFunctionRaw is RETIRED; OnModeStart's +0x858 read is
    // GetPlayerWreckCount() -- the road-rage crash target -- and the lifecycle TU now uses it.
    // ------------------------------------------------------------------------

    // [!!] SELF-INFLICTED-ASSERT NOTE -- BANNER CORRECTED 2026-08-26 (fix round), AND STILL AN
    // OPEN CONDUCTOR CALL. This banner previously claimed the line's noise was legitimate under
    // hazards H10. IT IS NOT: H10's assert storm is a WIRE MAP, and each round's last assert is
    // supposed to name the next MISSING WIRE. This line names nothing new -- it re-announces a park
    // that this same file created two functions above (PARKED PUBLISH 1), so it is a self-inflicted
    // line in an oracle whose whole value is that every line points somewhere.
    // FACTS, so the conductor can decide without re-deriving:
    //   * It is the CONSOLE's own assert -- BrnModeManager.cpp:5012 (`li r5, 0x1394`), string
    //     verbatim -- so deleting it is itself a divergence, which is why it is left ARMED here
    //     rather than removed on an implementer's own authority.
    //   * It compares the EVENT's checkpoint count against the GameModeParams checkpoint ARRAY's
    //     count, i.e. it audits exactly what PARKED PUBLISH 1 no longer appends. While that park
    //     stands the array is empty, so it fires "0 != N" ONCE PER RACE / MARKED-MAN /
    //     BURNING-ROUTE START, by construction.
    //   * IT DOES NOT FIRE ON THE STUNT-RACE PATH. Both this function and its caller gate stunt out
    //     (E_MODE_STUNT_ATTACK == 7 fails the {0,8,5} test above), so the campaign's own boot
    //     oracle -- a stunt race -- is NOT poisoned by it.
    //   * It goes silent the moment PARKED PUBLISH 1 is re-armed. [!] BLOCKER RESTATED 2026-08-26
    //     (CLOSURE round) -- the previous text here said the DECLARATIONS had not landed. They
    //     have: class GameModeParams carries AddCheckpoint (BrnGameModeParams.h:226), both
    //     GetCheckpointData overloads (:227/:228) and SetAStarDistanceFunction (:309); only
    //     AddStartLocation was landed in the earlier headers pass, and that is the sentence this
    //     banner was still repeating. THE RESIDUE IS THE BODIES: BrnGameModeParams.cpp bodies
    //     AddStartLocation (:176) and GetCheckpointCount (:109) but NOT AddCheckpoint and NOT
    //     either GetCheckpointData -- they are declare-only, so re-arming the park would compile
    //     and then fail the LINK. That, not a header gap, is what keeps this assert armed.
    // CONDUCTOR: if a race / marked-man / burning-route boot is scheduled before header_request #1
    // lands, silence this line for that round rather than reading it as a wire.
    CGS_ASSERT(lpRaceEventData->GetCheckpointCount() == lpGameModeParams->GetCheckpointCount(),
               "lpRaceEventData->GetCheckpointCount() == lpGameModeParams->GetCheckpointCount()"); // :5012

    // The console re-reads *(lpRaceEventData + 0x1C) on every pass of BOTH loops (0x8232932C
    // and 0x82329318) rather than caching it -- kept, it is the loop's real bound.
    for (s32 liCheckpointIndex = 0;
         liCheckpointIndex < lpRaceEventData->GetCheckpointCount();
         ++liCheckpointIndex)
    {
        const BrnProgression::CheckpointData* lpEventCheckpoint =
            lpRaceEventData->GetCheckpointData(liCheckpointIndex);

        // --------------------------------------------------------------------
        // [X] PARKED PUBLISH 4 of 4 -- the block-section copy.
        // Console 0x823292BC-0x82329328:
        //     CheckpointData* lpCheckpointData = lpGameModeParams->GetCheckpointData(liCheckpointIndex);
        //         // emitted as Array<CheckpointData,16>::GetIt(params + 0x260, i) @0x8231A7D8
        //     for (s32 liIndex = 0; liIndex < lpEventCheckpoint->GetBlockSectionCount(); ++liIndex)
        //     {
        //         lpCheckpointData->AddBlockSectionId(lpEventCheckpoint->GetBlockSectionId(liIndex));
        //             // emitted as Array<int,8>::Append(lpCheckpointData + 8, &luId) @0x82317A10,
        //             // i.e. CheckpointData::AddBlockSectionId inlined; the bounds assert
        //             // "liIndex >= 0 && liIndex < miBlockSectionCount" (BrnRaceEventData.h:847)
        //             // is GetBlockSectionId's OWN body, so it is not duplicated at this site.
        //     }
        // Both callees exist on this tree (BrnCheckpointData.h:25 AddBlockSectionId,
        // BrnRaceEventData.h:164 GetBlockSectionId, both declare-only). The ONLY thing missing
        // is the non-const GameModeParams::GetCheckpointData(s32) -- DWARF :399 -- so there is
        // no way to name the destination entry. header_request #2.
        // STATUS 2026-08-26 (CLOSURE round): the DECLARATION landed -- class GameModeParams now has
        // BOTH GetCheckpointData overloads (BrnGameModeParams.h:227 const, :228 non-const); the old
        // "neither overload" text was stale. Park still stands on the BODY: neither overload is
        // defined in BrnGameModeParams.cpp, so this leg would link-fail, and it is inert anyway
        // while PARKED PUBLISH 1 leaves the array empty.
        // OFF THE STUNT PATH (the {0,8,5} gate above), and inert anyway while PARKED PUBLISH 1
        // leaves the destination array empty.
        // --------------------------------------------------------------------
        (void)lpEventCheckpoint;
    }
}


// ============================================================================
// ModeManager::SetupOpponentData -- X360 0x82329348
// ============================================================================
// Builds one BrnGameState::OpponentData record per authored start-grid slot and appends it
// to GameModeParams::maOpponentData (Array<OpponentData,7>). Each record is
// {car model id, the event's start-grid slot, an interpolated AI balance graph, a racer
// personality}.
//
// This body has NO mode gate -- its only guard is `lpStartGameModeParams->GetEventData()`.
//
// [x] BODIED 2026-09-02 (rival-spawn wave R). The banner that stood here recorded this function
// as ~5% implemented -- five statements and the whole start-grid loop as a COMMENT -- waiting on
// six header items. All six are landed now, each in its DWARF home:
//   #6  BrnProgression::EventStartGridSlot + RaceEventData::GetStartGridCount/GetStartGridSlot
//       (SharedClasses/Progression/BrnRaceEventData.h -- the +0x5C table and the +0xE8 count are
//       carved out of maPad_58; both accessors inline, carrying the :1166/:1167 asserts).
//   #8  ProgressionData::GetPersonality(u32) (SharedClasses/Progression/BrnProgressionData.h,
//       inline, carrying the :497 assert).
//   #9  ProgressionManager::GetProgressionRankForGameModeNormalised /
//       GetProgressionRankNormalisedForCurrentRank -- were ALREADY bodied in
//       BrnProgressionManager.cpp (stuntrace wave D) when this banner still listed them missing.
//   #10 GameStateModule::GetOriginalCarId -- was ALREADY public (moved 2026-08-26).
//   #11 the real 112-byte BrnGameState::OpponentData (GameSource/GameState/BrnOpponentData.h,
//       pad retired, sizeof/offsetof pinned) + Array<OpponentData,7> in BrnGameModeParams.h with
//       AddOpponentData / GetOpponentCount / GetOpponentData.
// The loop below is walked from the export ASSEMBLY (0x823293DC..0x823296E4); the pseudocode is
// flagged "local variable allocation has failed" and mis-merges r30 (the player car id) with the
// event pointer into one __PAIR64__, so the asm is the only source for who holds what.
//
// [!] KI_MAX_RIVALS_IN_MODE == 7 is pinned by the console's own assert compare
// (`cmplwi r11, 7; ble`) and matches Array<OpponentData,7>; it now has its DWARF home,
// GameSource/World/BrnWorldSharedConstants.h.
// [!] The cop-car arm is real gameplay, not debug: when the mode sets
// KU_FLAG_SET_OPPONENTS_TO_COPS (0x400000000 -- the console builds the mask as
// `li r4,1; extldi r4,r4,64,34`, i.e. 1 << 34) every rival's model id is replaced with
// CgsIDCompress("XUSCCOB2"), the cop car.
// [!] THE TWO BALANCE-GRAPH INDEXES ARE CROSSED AND IT MATTERS: 0x82329604 loads slot+0x0C
// (miSlowAIBalanceGraphIndex) into r31 and 0x82329608 loads slot+0x08 (miFastAIBalanceGraphIndex)
// into r30, and the call passes r5 = r31 (liIndexA == SLOW) and r6 = r30 (liIndexB == FAST). With
// GetInterpolatedAIBalanceGraph's `A + (B - A) * blend`, a rank ratio of 0 gives the SLOW graph
// and 1 the FAST graph. Feeding them in slot order blends the graph backwards.
// [!] ASSERT-IS-NOT-A-GUARD, reproduced: lpOpponentSet is asserted non-null at :5057 and then
// RE-TESTED (`cmplwi cr6, r26, 0; beq` @0x82329504) -- a car with no authored opponent set still
// gets one OpponentData per grid slot, carrying the PLAYER's own model id (`mr r29, r30`
// @0x823294FC is the fall-through value) and a zeroed personality.
// ============================================================================
void ModeManager::SetupOpponentData(const StartGameModeParams* lpStartGameModeParams,
                                    GameModeParams*            lpGameModeParams)
{
    // Console: `addis r3, mpProgressionManager, 2; addi r3, r3, 0x8E4` == manager + 133348 ==
    // the ResourcePtr<ProgressionData>; null slot -> null, otherwise
    // ResourcePtr<ProgressionData>::operator->() @0x82325790 (the "Can not instance resource
    // pointer - it has no main memory resource" assert). That IS GetProgressionData().
    const BrnProgression::ProgressionData* lpProgressionData =
        GetProgressionManager()->GetProgressionData();

    const BrnProgression::RaceEventData* lpEventData = lpStartGameModeParams->GetEventData();

    // Console fires this assert BEFORE the event-data test (both operands are already loaded).
    // The missing space in "lpProgressionData!= NULL" is the console's own spelling -- verbatim.
    CGS_ASSERT(lpProgressionData != nullptr, "lpProgressionData!= NULL");   // BrnModeManager.cpp:5045

    if (lpEventData == nullptr)
    {
        return;
    }

    // 0x823293DC..0x82329420. The loop bound is read ONCE (`lwz r14, 0xE8(r29)`) and compared
    // SIGNED (`cmpwi cr6, r14, 0; ble` / `cmpw cr6, r24, r14; blt`), hence the s32 here even
    // though the accessor returns u32.
    const s32   liStartGridCount = static_cast<s32>(lpEventData->GetStartGridCount());
    const CgsID lPlayerCarId     = mpGameStateModule->GetActivePlayerCarId();          // ldx gsm+0x456D8
    const s32   liPlayerRank     = GetProgressionManager()->GetProgressionRank();      // extsb r31, r3
    const CgsID lOriginalCarId   = mpGameStateModule->GetOriginalCarId(lPlayerCarId);  // 0x8232940C

    const BrnProgression::CarOpponentSet* lpOpponentSet =
        lpProgressionData->FindCarOpponentSet(lOriginalCarId, liPlayerRank);
    CGS_ASSERT(lpOpponentSet != nullptr, "lpOpponentSet != NULL");            // BrnModeManager.cpp:5057

    for (s32 liIndex = 0; liIndex < liStartGridCount; ++liIndex)
    {
        // GetStartGridSlot's own :1166/:1167 asserts fire inside it (re-reading the count each
        // pass, as the console does at 0x823294B0/0x823294D4).
        const BrnProgression::EventStartGridSlot* lpStartGridSlot =
            lpEventData->GetStartGridSlot(static_cast<u32>(liIndex));

        BrnProgression::EventRacerPersonality lPersonality;
        lPersonality.Construct();                                            // 0x82329500

        // `mr r29, r30` @0x823294FC: the player's own car id is the fall-through model id.
        CgsID lCarModelId = lPlayerCarId;

        if (lpOpponentSet != nullptr)                                        // re-tested @0x82329504
        {
            CGS_ASSERT(lpOpponentSet->GetOpponentCount() > 0,
                       "lpOpponentSet->GetOpponentCount() > 0");             // BrnModeManager.cpp:5079

            // `divwu / mullw / subf` == an UNSIGNED modulo of the slot's opponent selector by the
            // set's count (`twllei r11, 0` is the divide-by-zero trap the assert above pre-empts).
            const s32 liOpponentCount    = lpOpponentSet->GetOpponentCount();
            const s32 liCarOpponentIndex = static_cast<s32>(
                lpStartGridSlot->GetOpponentIndex() % static_cast<u32>(liOpponentCount));
            CGS_ASSERT(liCarOpponentIndex >= 0 && liCarOpponentIndex < liOpponentCount,
                       "liCarOpponentIndex >= 0 && liCarOpponentIndex < miOpponentCount");
                                                                              // BrnOpponentData.h:224
            const BrnProgression::CarOpponent* lpCarOpponent =
                lpOpponentSet->GetCarOpponent(liCarOpponentIndex);

            // 0x82329570..0x823295A4: GetFlag(1 << 34) then either the cop car or the set's car.
            if (lpGameModeParams->GetFlag(GameModeParams::KU_FLAG_SET_OPPONENTS_TO_COPS))
            {
                lCarModelId = CgsIDCompress("XUSCCOB2");
            }
            else
            {
                lCarModelId = lpCarOpponent->GetCarId();
            }

            // 0x823295A8..0x823295FC: GetPersonality (its :497 assert is inside it), then the
            // 16-byte copy into the stack personality.
            lPersonality = *lpProgressionData->GetPersonality(
                static_cast<u32>(lpCarOpponent->GetPersonalityIndex()));
        }

        // 0x82329604/0x82329608 -- the CROSSED pair (see the banner): r5 <- +0x0C, r6 <- +0x08.
        const s32 liSlowAIBalanceGraphIndex = lpStartGridSlot->GetSlowAIBalanceGraphIndex();
        const s32 liFastAIBalanceGraphIndex = lpStartGridSlot->GetFastAIBalanceGraphIndex();

        // 0x8232960C..0x82329654: the START params' mode type (+0x2D0), NOT the GameModeParams
        // one, forks the rank ratio between the per-mode arm (modes 0/3/7/8) and the global one
        // (sub_8237B610 == GetProgressionRankNormalisedForCurrentRank).
        const GameStateModuleIO::EGameModeType leStartModeType =
            lpStartGameModeParams->GetGameModeType();
        f32 lfRankNormalised;
        if (leStartModeType == GameStateModuleIO::E_MODE_OFFLINE_RACE ||
            leStartModeType == GameStateModuleIO::E_MODE_ROAD_RAGE    ||
            leStartModeType == GameStateModuleIO::E_MODE_STUNT_ATTACK ||
            leStartModeType == GameStateModuleIO::E_MODE_MARKED_MAN)
        {
            lfRankNormalised =
                GetProgressionManager()->GetProgressionRankForGameModeNormalised(leStartModeType);
        }
        else
        {
            lfRankNormalised = GetProgressionManager()->GetProgressionRankNormalisedForCurrentRank();
        }

        const BrnProgression::OpponentBalanceData lBalanceData =
            lpProgressionData->GetInterpolatedAIBalanceGraph(liSlowAIBalanceGraphIndex,   // r5
                                                             liFastAIBalanceGraphIndex,   // r6
                                                             lfRankNormalised);           // f1

        // 0x82329674..0x823296D4: the 112-byte record assembled on the stack, then appended.
        OpponentData lOpponentData;
        lOpponentData.Construct(lCarModelId, lpStartGridSlot, &lBalanceData, &lPersonality);
        lpGameModeParams->AddOpponentData(&lOpponentData);   // Array<OpponentData,7>::Append @0x82317D90
    }
}

// ============================================================================
// ModeManager::SetupCheckpointDistricts -- X360 0x823296F0
// ============================================================================
// Labels every already-published checkpoint with the world district its landmark stands in,
// by sampling the GameStateModule's 2D district map at the landmark's box-region position.
// Runs AFTER SetUpCheckPointsForGameMode (StartGameMode calls them in that order), which is
// why it iterates the GameModeParams checkpoint ARRAY rather than the event's table.
// ============================================================================
void ModeManager::SetupCheckpointDistricts(GameModeParams* lpGameModeParams)
{
    const GameStateModuleIO::EGameModeType leGameModeType = lpGameModeParams->GetGameModeType();
    if (leGameModeType != GameStateModuleIO::E_MODE_OFFLINE_RACE &&
        leGameModeType != GameStateModuleIO::E_MODE_MARKED_MAN &&
        leGameModeType != GameStateModuleIO::E_MODE_BURNING_ROUTE)
    {
        return;
    }

    // [!] OFFSET CITATION ADDED 2026-08-26 (fix round) -- this was the one binding in an otherwise
    // fully-cited file that carried no offset proof in the source. Console 0x82329734..0x82329748:
    //     lwz   r11, 0x6D58(r24)      ; mpGameStateModule
    //     addis r23, r11, 4           ; + 4*65536 == +262144
    //     addi  r23, r23, -0x3F70     ;            -  16240   ==> gsm + 245904 == gsm + 0x3C090
    // and BrnGameStateModule.h:743-745 already pins GetDistrictMap() at exactly
    // "GameStateModule+0x3C090 (245904)". Same byte, independently, from both ends.
    CgsWorld::WorldMap2D* lpDistrictMap = mpGameStateModule->GetDistrictMap();

    // The console re-reads the array count (and re-fires the CgsArray.h:336 "Array used before
    // Construct/Clear was called" assert that GetCheckpointCount owns) on EVERY pass -- kept.
    for (s32 liCheckpointIndex = 0;
         liCheckpointIndex < lpGameModeParams->GetCheckpointCount();
         ++liCheckpointIndex)
    {
        // --------------------------------------------------------------------
        // [X] PARKED -- same single gap as SetupPathfinding's block-section copy. [!] RESIDUE
        // RESTATED 2026-08-26 (CLOSURE round): the non-const GameModeParams::GetCheckpointData(s32)
        // is now DECLARED (BrnGameModeParams.h:228, DWARF :399) -- what is missing is its BODY in
        // BrnGameModeParams.cpp, so naming the entry here would compile and then LNK2019. Console
        // (0x823297A8-0x82329870), de-inlined:
        //
        //   CheckpointData* lpCheckpointData = lpGameModeParams->GetCheckpointData(liCheckpointIndex);
        //       // Array<CheckpointData,16>::GetIt(params + 0x260, i) @0x8231A7D8
        //   const s32 liRegionIndex = lpCheckpointData->GetLandmarkIndex();   // lhz + extsh, +0x00
        //   const BrnTrigger::Landmark* lpLandmark =
        //       GetTriggerData()->GetLandmarkFromRegionIndex(liRegionIndex);
        //       // INLINED on console, which is why BOTH of that accessor's baked asserts appear
        //       // here -- "liRegionIndex < miRegionCount" (BrnTriggerData.h:624, GetRegion's) and
        //       // "lpTriggerRegion->GetType() == TriggerRegion::E_TYPE_LANDMARK"
        //       // (BrnTriggerData.h:615). De-inlined to the real call @0x8231B648, so neither is
        //       // duplicated at this site. NOTE the console re-resolves the TriggerData resource
        //       // through the TQM once per checkpoint (`lwz r11, 0x6D60(this); addi r3, r11, 0x620;
        //       // bl ResourcePtr<TriggerData>::operator->`) rather than hoisting it.
        //   const u8 luDistrict = lpDistrictMap->GetValue(lpLandmark->GetBoxRegion()->GetPosition2D());
        //       // console loads the box-region's three floats + a zero w into a stack Vector3, then
        //       // vperm's it through the constant at 0x82CDA450 -- dumped from the image this
        //       // session as 00 01 02 03 | 18 19 1A 1B | 00 01 02 03 | 00 01 02 03, i.e. lanes
        //       // {x, z, x, x} -- and calls the Vector2 overload @0x82907FF8. GetPosition2D()
        //       // (BrnRegion.h:100) IS that permute by name: Vector2{ x, z, 0, 0 }.
        //   if (luDistrict != CgsWorld::KU_INVALID_WORLD_MAP_VALUE)   // console: != 255
        //   {
        //       lpCheckpointData->SetDistrict(luDistrict);            // stw @+4, keeping the
        //                                                             // Construct default 18
        //                                                             // (E_DISTRICT_INVALID) when
        //                                                             // the sample is off-map
        //   }
        // DELETE-WHEN header_request #2 lands -- this block is a straight uncomment.
        // --------------------------------------------------------------------
        (void)lpDistrictMap;
    }
}

}
