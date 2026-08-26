#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CgsDev::Assert::Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // CgsDev::Log::gpDebugPrint / CgsDev::Message::gxMessageFilterFlags
#include "GameSource/Math/BrnMathUtils.h"            // BrnMath::IsNormal (AddStartLocation's own guard)

// =============================================================================
// BrnGameState::GameModeParams - the six X360-attested members.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct              @ 0x8231C370
//   GetCheckpointCount     @ 0x822B1EF0
//   GetFlag                @ 0x821F2C88
//   GetStartDirection      @ 0x822CBA20
//   GetStartLocationCount  @ 0x822B1F48
//   GetStartPosition       @ 0x822CB9B0
//
// GameModeParams is the per-event parameter block: the ModeManager constructs one,
// fills it from the event/progression data, and hands it to the world / AI / traffic
// modules when a mode starts. It owns no resources (plain value type).
// =============================================================================

namespace BrnGameState
{
namespace
{
// The start-location array is indexed by active-race-car slot; the X360 build asserts
// the index against BrnWorld::KI_MAX_ACTIVE_RACE_CARS (== 8). Modelled as a local
// constant rather than pulling in the BrnWorld header for a single bound.
const u32 KU_MAX_ACTIVE_RACE_CARS = 8u;

// Assert source path baked into the X360 build for the start-location bounds checks.
const char* const KPC_PARAMS_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\ModeManager/GameModes/BrnGameModeParams.h";
}

// -----------------------------------------------------------------------------
// Construct - reset every field to its "no event configured" default.
//
// The X360 body is a single flat initialiser (the compiler interleaved the writes and
// hoisted the array-count loop). Reconstructed as the logical member-by-member reset.
// Takes the real GameStateModuleIO::EGameModeType (wired in at consolidation, replacing the
// EGameModeType_Stub the worker used in isolation).
// -----------------------------------------------------------------------------
void GameModeParams::Construct(GameStateModuleIO::EGameModeType leGameModeType)
{
    meGameModeType = leGameModeType;

    // Scalars / flags.
    meStartMechanism                  = E_GAMEMODESTARTMECHANISM_DEFAULT;
    muFlags                           = 0;
    mePursuedCarGlobalIndex           = E_GLOBALRACECARINDEX_STUB;
    mfProgressionRankAsRatio          = 0.0f;
    mbIsOnline                        = false;
    mbInfiniteBoost                   = false;
    mfOnlineFreeburnDeformationAmount = 0.0f;
    mfModeTimeLimit                   = 0.0f;
    mfTrafficDensityScale             = 1.0f;
    mfLargeVehicleProbability         = 1.0f;
    mfTrafficSpeedScale               = 1.0f;
    miNumRivals                       = 0;
    miNumNetworkPlayers               = 0;
    // The X360 tail clears the player wreck count to 0 (it lands in the trailing all-zero
    // store cluster @ 0x838-0x860, just before the u64 muFlags `std` @ 0x860 -- there is no
    // -1 store in the tail). The only -1 stores are the four head-region words @ 0x40/0x4C/
    // 0x50/0x54 and the per-slot maModelIds in the loop. (Was -1; see LOW-CONFIDENCE note.)
    miPlayerWreckCount                = 0;
    // Console `stw r11(0), 0x854(r3)` @0x8231C408's tail: under the corrected +0x850/854/858
    // run that word is meAStarDistanceFunction. The console never writes 0x850
    // (mfOnlineModeTimeLimit stays unset here -- deliberate; do not add a store for it).
    meAStarDistanceFunction           = E_ASTARDIST_STUB;   // == 0

    // Per-event identity / counts cleared.
    muEventJunctionID            = 0;
    muJunctionID                 = 0;
    muNumberOfCheckpointsInEvent = 0;

    // Per-slot grade thresholds and difficulty cleared.
    mfNeedForBronze = 0.0f;
    mfNeedForSilver = 0.0f;
    mfNeedForGold   = 0.0f;

    // Per-slot reset. The X360 hoisted these into one 8-iteration loop (@ 0x8231C418..0x8231C444):
    // each pass writes six per-slot members --
    //   std  r11=0  -> maNetworkPlayerID[i]        (network player id cleared to 0)
    //   sth  r11=0  -> mau16CarColourIndex[i]      (colour index cleared to 0)
    //   sth  r11=0  -> mau16CarPaintFinishIndex[i] (paint-finish index cleared to 0)
    //   stfs f0=-1.0 -> mfOvertakingDifficulty[i]  ("no handicap" sentinel, -1)
    //   stw  r11=0  -> maePlayerTeam[i]            (team cleared to 0)
    //   stw  r6=-1  -> maModelIds[i]               ("no model" sentinel, -1)
    for (u32 luCar = 0; luCar < KU_MAX_ACTIVE_RACE_CARS; ++luCar)
    {
        maNetworkPlayerID[luCar]           = 0;
        mau16CarColourIndex[luCar]         = 0;
        mau16CarPaintFinishIndex[luCar]    = 0;
        mfOvertakingDifficulty[luCar]      = -1.0f;
        maePlayerTeam[luCar]               = E_PLAYERTEAM_STUB;     // 0
        maModelIds[luCar]                  = static_cast<CgsID>(-1);
    }

    // Reset the two embedded fixed-size arrays to empty-but-usable: the X360 stores 0 to each
    // count word (maStartLocations.miCount @ +0x250, maCheckpointDataArray.miCount @ +0x520),
    // flipping them off the KI_UNCONSTRUCTED(-1) sentinel so GetStartLocationCount() /
    // GetCheckpointCount() return 0 (rather than wrongly firing the use-before-Construct
    // assert). The Feb-2007 owner spells this maLandmarkDataArray.Construct().
    maStartLocations.Construct();
    maCheckpointDataArray.Construct();
}

// -----------------------------------------------------------------------------
// GetCheckpointCount - number of checkpoints registered for this event.
// -----------------------------------------------------------------------------
s32 GameModeParams::GetCheckpointCount() const
{
    CGS_ASSERT(maCheckpointDataArray.GetCount() != CheckpointDataArray::KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    return maCheckpointDataArray.GetCount();
}

// -----------------------------------------------------------------------------
// GetStartLocationCount - number of start-grid slots registered.
// -----------------------------------------------------------------------------
s32 GameModeParams::GetStartLocationCount() const
{
    CGS_ASSERT(maStartLocations.GetCount() != StartLocationArray::KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    return maStartLocations.GetCount();
}

// GetFlag (X360 @ 0x821F2C88) is defined inline in BrnGameModeParams.h:193 -- the out-of-line
// copy that used to live here was a duplicate definition of the same method.

// -----------------------------------------------------------------------------
// GetStartPosition - spawn position for the start-grid slot liStartLocationIndex.
// -----------------------------------------------------------------------------
Vector3 GameModeParams::GetStartPosition(s32 liStartLocationIndex) const
{
    CGS_ASSERT(static_cast<u32>(liStartLocationIndex) < KU_MAX_ACTIVE_RACE_CARS,
               "liOpponentIndex >= 0 && liOpponentIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
    return maStartLocations.Ge(static_cast<u32>(liStartLocationIndex)).mPosition;
}

// -----------------------------------------------------------------------------
// GetStartDirection - facing for the start-grid slot liStartLocationIndex.
// -----------------------------------------------------------------------------
Vector3 GameModeParams::GetStartDirection(s32 liStartLocationIndex) const
{
    CGS_ASSERT(static_cast<u32>(liStartLocationIndex) < KU_MAX_ACTIVE_RACE_CARS,
               "liOpponentIndex >= 0 && liOpponentIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
    return maStartLocations.Ge(static_cast<u32>(liStartLocationIndex)).mDirection;
}

// -----------------------------------------------------------------------------
// AddStartLocation - append one start-grid slot {spawn position, facing}.
//
// [stuntrace waveB CLOSURE round, 2026-08-26] X360-INLINED at every call site, so there is no
// standalone export to transcribe. The whole body is nonetheless VISIBLE, instruction for
// instruction, inside ModeManager::SetStartingGrid @0x82328608, which is the only producer of
// start-grid slots in the game:
//
//   0x82328714  addi r25, r30, 0x150   ; r25 = &lpGameModeParams->maStartLocations (console +0x150)
//   ...  per iteration:
//   0x82328794  stvx128 v127, r0, r11  ; r11 = var_90 <- lDirection
//   0x8232879C  stvx128 v0,   r0, r11  ; r11 = var_A0 <- lPosition
//   0x823287A0  bl BrnMath__IsNormal   ; on v1 == v127 == lDirection
//   0x823287B4  li r5, 0x490           ; assert line 1168, file BrnGameModeParams.h
//   0x823287C8  addi r4, r1, var_A0    ; &{ position, direction }
//   0x823287CC  mr   r3, r25
//   0x823287D0  bl BrnGameState__StartLocation_8___Append
//
// The two stack slots are 16 bytes apart (var_A0 -> var_90), which is exactly StartLocation's
// { Vector3 mPosition; Vector3 mDirection; } and the 32-byte record Append copies -- so the
// record is built position-first, direction-second, and the pair is passed by address. The
// bounds/constructed guards are NOT restated here: Array<T,N>::Append owns them
// (CgsArray.h:174), the same rule GetCheckpointData's banner records for Array::GetIt.
//
// ORDERING NOTE: the console stores the direction slot BEFORE the position slot and only then
// runs the IsNormal guard. Both stores are into the same by-value record and neither is read in
// between, so writing the record in declaration order and asserting first is the same program;
// the assert stays ahead of the Append, which is the only ordering that is observable.
// -----------------------------------------------------------------------------
void GameModeParams::AddStartLocation(Vector3 lPosition, Vector3 lDirection)
{
    // Verbatim, spaces included -- this is the string SetStartingGrid carries today
    // (BrnModeManager_IntroPlay.cpp:233) because the console inlined this body into it.
    CGS_ASSERT(BrnMath::IsNormal(lDirection), "BrnMath::IsNormal( lDirection )");

    StartLocation lStartLocation;
    lStartLocation.mPosition  = lPosition;
    lStartLocation.mDirection = lDirection;
    maStartLocations.Append(lStartLocation);
}

// =============================================================================
// BrnGameState::StartGameModeParams - the seven X360-attested standalone methods.
// =============================================================================

// -----------------------------------------------------------------------------
// StartGameModeParams::Construct (X360 @ 0x8231C1F8) - reset to "no event configured".
//
// Validates the start mechanism, stashes game-mode type / mechanism / player position, zeroes the
// start direction, writes the traffic-light trigger / takedown target / pursued-car index / shot
// group to their all-FF/-1 invalid sentinels, the boost earning to 1.0 and the traffic density to
// 0.0 (the binary has exactly ONE 1.0 store, [199]=mfBoostEarning), the rank ratio / base
// deformation / event+rank pointers / junction id to 0, and Construct()s the checkpoint array.
// The player position arrives in a vector register (Hex-Rays vmr128 v127,v1). Offsets not x64-
// faithful (AGENTS.md); X360 word index per member is in the class declaration. miRaceId /
// muEventJunctionId / mpPlayerCarVehicleListEntry are NOT written here (absent from the X360
// store cluster -- set by their own setters before use); miRaceId and mPursuedCarID ARE written,
// see the corrected block below.
// -----------------------------------------------------------------------------
void StartGameModeParams::Construct(GameStateModuleIO::EGameModeType leGameModeType,
                                    Vector3                          lPlayerPosition,
                                    EGameModeStartMechanism          leStartMechanism)
{
    CGS_ASSERT(static_cast<u32>(leStartMechanism) < E_GAMEMODESTARTMECHANISM_COUNT, "(uint32_t)leStartMechanism < E_GAMEMODESTARTMECHANISM_COUNT");

    meGameModeType   = leGameModeType;        // [180] +720
    meStartMechanism = leStartMechanism;      // [196] +784
    mPlayerPosition  = lPlayerPosition;       // [184] +736 (vmr128 v127,v1)
    mStartDirection.SetZero();                // [188] +752 (vspltisw v0,0)

    // Four all-FF / NaN invalid sentinels ([192],[193],[197],[200]).
    miTakedownTarget        = -1;                                        // [192] +768
    mePursuedCarGlobalIndex = static_cast<EGlobalRaceCarIndex_Stub>(-1); // [193] +772 (invalid index)
    mTrafficLightTriggerId  = static_cast<LightTriggerId>(0xFFFFFFFFu);  // [197] +788 (IsValid()==false)
    miShotGroup             = -1;                                        // [200] +800

    // The single 1.0 store is the boost-earning default; traffic density defaults to 0.0.
    mfBoostEarning           = 1.0f;          // [199] +796
    mfTrafficDensity         = 0.0f;          // [198] +792
    mfPlayerBaseDeformation  = 0.0f;          // [201] +804
    mfProgressionRankAsRatio = 0.0f;          // [206] +824

    // [x] CORRECTED 2026-08-26 (mount-closure round, re-dumped from the image). The banner above
    // used to claim miRaceId and mPursuedCarID were "NOT written here". They ARE -- the console
    // emits two 8-byte zero stores that the earlier reconstruction read past because they are
    // `std` (not `stw`) and sit apart from the scalar cluster:
    //     0x8231C24C  li  r11, 0
    //     0x8231C274  std r11, 0x2C8(r31)      <- miRaceId      (+712, 8-byte CgsID)
    //     0x8231C280  std r11, 0x308(r31)      <- mPursuedCarID (+776, 8-byte CgsID)
    // This matters to THIS TU's new GetRaceId()/GetPursuedCarID() bodies: without the stores a
    // freshly-Constructed params block hands ModeManager::Start (BrnModeManager_Start.cpp,
    // `mRaceId = lpStartGameModeParams->GetRaceId()`) whatever was on the caller's stack.
    // The two genuinely-unwritten members are muEventJunctionId (+0x328) and
    // mpPlayerCarVehicleListEntry (+0x33C) -- neither offset appears in the store cluster.
    miRaceId      = 0;                        // [177] +712 (`std r11, 0x2C8`)
    mPursuedCarID = 0;                        // [194] +776 (`std r11, 0x308`)

    // Per-event handles / ids cleared.
    mpEventData           = NULL;             // [203] +812
    muJunctionID          = 0;                // [204] +816
    mpProgressionRankData = NULL;             // [205] +820

    // Empty-but-usable checkpoint list (X360: trailing count word -> 0, off the -1 sentinel).
    maCheckpointDataArray.Construct();        // count @ +704
}

// -----------------------------------------------------------------------------
// StartGameModeParams::GetTrafficLightTriggerId (X360 @ 0x8231C2D8). Return the stored trigger
// handle, asserting it is valid first (IsValid() inlined).
// -----------------------------------------------------------------------------
LightTriggerId StartGameModeParams::GetTrafficLightTriggerId() const
{
    const bool lbIsValid = !(((mTrafficLightTriggerId & 0xFFFF00u) == 0xFFFF00u) ||
                             (mTrafficLightTriggerId == 255u));
    CGS_ASSERT(lbIsValid, "mTrafficLightTriggerId.IsValid()");
    return mTrafficLightTriggerId;
}

// -----------------------------------------------------------------------------
// StartGameModeParams::SetTrafficLightTriggerId (X360 @ 0x823616E8). Store the trigger handle
// after asserting it is valid (same IsValid() test as the getter).
// -----------------------------------------------------------------------------
void StartGameModeParams::SetTrafficLightTriggerId(LightTriggerId lTriggerId)
{
    const bool lbIsValid = !(((lTriggerId & 0xFFFF00u) == 0xFFFF00u) ||
                             (lTriggerId == 255u));
    CGS_ASSERT(lbIsValid, "lTriggerId.IsValid()");
    mTrafficLightTriggerId = lTriggerId;
}

// -----------------------------------------------------------------------------
// StartGameModeParams::SetProgressionRankData (X360 @ 0x82354490). Store the rank-data pointer
// (asserts non-null). X360 word [205] (+820) -> mpProgressionRankData.
// -----------------------------------------------------------------------------
void StartGameModeParams::SetProgressionRankData(const BrnProgression::ProgressionRankData* lpProgressionRankData)
{
    CGS_ASSERT(lpProgressionRankData != NULL, "lpProgressionRankData != NULL");
    mpProgressionRankData = lpProgressionRankData;
}

// -----------------------------------------------------------------------------
// StartGameModeParams::SetProgressionRankAsRatio (X360 @ 0x823544F0). Store the 0..1 rank ratio
// (X360 byte +824 -> mfProgressionRankAsRatio). When the AI message-filter bit is set the X360
// logs "<AI> Setting progression rank to <value>\n" through the debug-print stream.
// -----------------------------------------------------------------------------
void StartGameModeParams::SetProgressionRankAsRatio(f32 lfProgressionRankAsRatio)
{
    // X360: `gpDebugPrint << "<AI> Setting progression rank to " << lfProgressionRankAsRatio
    // << "\n"` -- the label goes through vtable slot 1 (StrStreamBase::operator<<(const char*)),
    // then sub_821F0F40 (the f32 overload) formats the value, then a second slot-1 call emits the
    // newline. Restored as the stream chain now that CgsLog.h supplies the real DebugPrint.
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "<AI> Setting progression rank to "
                                   << lfProgressionRankAsRatio << "\n";
    }
    mfProgressionRankAsRatio = lfProgressionRankAsRatio;
}

// -----------------------------------------------------------------------------
// StartGameModeParams::SetPlayerVehicleGamePlayData (X360 @ 0x82354590). Store the player car's
// vehicle-list entry pointer (asserts non-null). X360 word [207] (+828) -> mpPlayerCarVehicleListEntry.
// -----------------------------------------------------------------------------
void StartGameModeParams::SetPlayerVehicleGamePlayData(const BrnResource::VehicleListEntry* lpPlayerCarVehicleListEntry)
{
    CGS_ASSERT(lpPlayerCarVehicleListEntry != NULL, "lpPlayerCarVehicleListEntry != NULL");
    mpPlayerCarVehicleListEntry = lpPlayerCarVehicleListEntry;
}

// -----------------------------------------------------------------------------
// StartGameModeParams::AddCheckpoint (X360 @ 0x8236AAC0). Append one checkpoint (landmark +
// AI-section) to the event's checkpoint list. Element setup is CheckpointData::Construct inlined
// (district = E_DISTRICT_INVALID (18), empty block-section list).
// -----------------------------------------------------------------------------
void StartGameModeParams::AddCheckpoint(LandmarkIndex luLandmarkIndex, u16 luAISectionIndex)
{
    CheckpointData lCheckpointData;
    lCheckpointData.Construct(luLandmarkIndex, luAISectionIndex);
    maCheckpointDataArray.Append(lCheckpointData);
}

// =============================================================================
// StartGameModeParams - the read-side accessors.  [stuntrace wave B, MOUNT-CLOSURE round]
//
// NONE of these has an out-of-line symbol in BURNOUT_X360_ARTIST.XEX: the whole set is inlined
// at every call site (a sweep of the export set for functions whose prototype names
// StartGameModeParams returns 14 hits, of which only the seven standalone methods bodied above
// are members of this class). So each body below is derived from the CONSUMER asm, not from a
// getter address, and every member mapping is cited from at least two independent places. The
// offsets used as evidence are the CONSOLE ones (X360 word index per member is in the class
// declaration); host offsets are not x64-faithful and parity here is by named member (AGENTS.md).
//
// THE FRAME, pinned end to end -- this is why the un-cited members in the middle cannot slide:
//   * StartGameModeParams::Construct @0x8231C1F8 emits the complete store cluster
//     0x2C0 / 0x2C8 / 0x2D0 / 0x2E0 / 0x2F0 / 0x300 / 0x304 / 0x308 / 0x310 / 0x314 / 0x318 /
//     0x31C / 0x320 / 0x324 / 0x32C / 0x330 / 0x334 / 0x338 -- a dense, gap-free run whose ends
//     are named by the four X360-attested standalone SETTERS bodied above (0x314
//     mTrafficLightTriggerId, 0x334 mpProgressionRankData, 0x338 mfProgressionRankAsRatio,
//     0x33C mpPlayerCarVehicleListEntry).
//   * The committed BrnModeManager_Start.cpp banner re-derived 0x2D0 / 0x2F0 / 0x310 / 0x328 /
//     0x32C / 0x330 / 0x334 / 0x338 from a different call site (StuntAttackMode::Start
//     @0x82331EA8, r29 = lpStartGameModeParams).
//   * The store WIDTHS discriminate the two 8-byte members from their 4-byte neighbours: 0x2C8
//     and 0x308 are std/ld (CgsID, u64), 0x2E0 and 0x2F0 are stvx128 (16-byte vector), and
//     everything else is stw/lwz/stfs/lfs.
// Together those fix all twenty members, so no accessor below is a one-slot guess -- the failure
// mode this campaign just paid for on the medal-run +0x60..0x6C accessors.
// =============================================================================

// -----------------------------------------------------------------------------
// GetGameModeType -- meGameModeType, console +0x2D0 (720).
// Attested at three independent inlined call sites: OnlineStuntRunMode's mode gate
// (lwz r11, 0x2D0(r25); banner BrnOnlineStuntRunMode.cpp:259), SurvivorMode::Start @0x82332364
// (lwz r4, 0x2D0(r29) -- passed straight on as a2), and ModeManager::SetupOpponentData
// @0x8232960C. Construct stores its leGameModeType argument here (mr r29,r4 at entry, then
// stw r29, 0x2D0(r31) @0x8231C250), which is an argument-to-offset link. No assert anywhere.
// -----------------------------------------------------------------------------
GameStateModuleIO::EGameModeType StartGameModeParams::GetGameModeType() const
{
    return meGameModeType;
}

// -----------------------------------------------------------------------------
// GetRaceId -- miRaceId, console +0x2C8 (712), 8-byte CgsID.
// Pinned by the committed BrnModeManager_Start.cpp assembly transcript (0x8234FEB0
// ld r11, 0x2C8(r30) -> stdx r11, r31, 0x8030 == ModeManager::mRaceId), whose ld width is what
// separates this member from the 4-byte meGameModeType that follows it at +0x2D0. That
// transcript is also what cleared the header's old "+708" SUSPECT note: 708 is the word index
// times four, but a CgsID is 8-aligned, so the member sits at 712.
// -----------------------------------------------------------------------------
CgsID StartGameModeParams::GetRaceId() const
{
    return miRaceId;
}

// -----------------------------------------------------------------------------
// GetStartDirection -- mStartDirection, console +0x2F0 (752), one 16-byte vector slot.
// Construct zeroes it with stvx128 v0(=0), r31, r8 where r8 == 0x2F0 (@0x8231C27C) -- a VECTOR
// store, which is what distinguishes this member and mPlayerPosition (+0x2E0, stvx128 v127 ==
// the incoming position argument) from the scalar run that follows. Independently named at
// +0x2F0 by the committed BrnStuntAttackMode.cpp:192 offset table, whose Start body reads it,
// zeroes the Y lane and stores it to the mode's mStartDir.
// -----------------------------------------------------------------------------
Vector3 StartGameModeParams::GetStartDirection() const
{
    return mStartDirection;
}

// -----------------------------------------------------------------------------
// GetStartMechanism -- meStartMechanism, console +0x310 (784).
// Construct stores the validated leStartMechanism argument straight into it (mr r30,r5 at entry,
// then stw r30, 0x310(r31) @0x8231C258) -- the same argument-to-offset link as GetGameModeType,
// and the argument the "(uint32_t)leStartMechanism < E_GAMEMODESTARTMECHANISM_COUNT" assert
// guards. Read back inlined by SurvivorMode::Start @0x823325D4 (lwz r11, 0x310(r29)).
// -----------------------------------------------------------------------------
EGameModeStartMechanism StartGameModeParams::GetStartMechanism() const
{
    return meStartMechanism;
}

// -----------------------------------------------------------------------------
// GetTakedownTarget -- miTakedownTarget, console +0x300 (768).
// PursuitMode::Start @0x823221E0 reads the three pursuit fields as one consecutive run --
//     lwz r11, 0x300(r29) -> stw r11, 0x50(r31)
//     lwz r11, 0x304(r29) -> stw r11, 0x54(r31)
//     ld  r11, 0x308(r29) -> std r11, 0x58(r31)
// i.e. exactly {takedown target, pursued car index, pursued car id} in declaration order, copied
// into three consecutive GameModeParams slots. Construct writes -1 here (stw r10(-1), 0x300),
// so "no target" is a legal return. No assert on the console read.
// -----------------------------------------------------------------------------
s32 StartGameModeParams::GetTakedownTarget() const
{
    return miTakedownTarget;
}

// -----------------------------------------------------------------------------
// GetPursuedCarGlobalIndex -- mePursuedCarGlobalIndex, console +0x304 (772).
// Middle member of the PursuitMode::Start run above (lwz r11, 0x304(r29) @0x823221F4). A 4-byte
// load, so it is the enum and not the CgsID that starts at +0x308. Construct writes the all-FF
// invalid index (stw r10(-1), 0x304 @0x8231C298).
// -----------------------------------------------------------------------------
EGlobalRaceCarIndex_Stub StartGameModeParams::GetPursuedCarGlobalIndex() const
{
    return mePursuedCarGlobalIndex;
}

// -----------------------------------------------------------------------------
// GetPursuedCarID -- mPursuedCarID, console +0x308 (776), 8-byte CgsID.
// Third member of the PursuitMode::Start run: ld r11, 0x308(r29) @0x823221FC followed by
// std r11, 0x58(r31) -- an 8-byte load feeding an 8-byte store, which is what proves this slot
// is one u64 id rather than two words. Construct clears it with std r11(0), 0x308 @0x8231C280.
// -----------------------------------------------------------------------------
CgsID StartGameModeParams::GetPursuedCarID() const
{
    return mPursuedCarID;
}

// -----------------------------------------------------------------------------
// GetTrafficDensity -- mfTrafficDensity, console +0x318 (792).
// The cleanest producer-to-consumer pin in the block, and it appears twice:
//     PursuitMode::Start  @0x82322150  lfs f0, 0x318(r29) ; stfs f0, 0x30(r31)
//     SurvivorMode::Start @0x823224DC  lfs f0, 0x318(r29) ; stfs f0, 0x30(r31)
// GameModeParams+0x30 is mfTrafficDensityScale in the committed BrnModeManager_Start.cpp table
// and, independently, in BrnModeManager_IntroPlay.cpp:195-199 -- so the start params' traffic
// density is copied into the mode params' traffic-density scale. lfs, so it is the f32 here and
// not the s32 miShotGroup two slots along. Construct defaults it to 0.0f (the single 1.0f store
// in this body belongs to mfBoostEarning at +0x31C).
// -----------------------------------------------------------------------------
f32 StartGameModeParams::GetTrafficDensity() const
{
    return mfTrafficDensity;
}

// -----------------------------------------------------------------------------
// GetBoostEarning -- mfBoostEarning, console +0x31C (796).
// ModeManager::PrepareForMode @0x82342A94 reads it as a FLOAT (lfs f0, 0x31C(r31), r31 ==
// lpStartGameModeParams -- the pointer that body's own "lpStartGameModeParams != NULL" assert
// guards) and stores it into the PrepareForModeAction record at +0x8C8, which is
// mfPlayerBoostEarning in BrnGameActions.h. That is exactly this wave's producer chain,
// BrnModeManager_Prepare.cpp:575:
//     lAction.SetPlayerBoostEarning(lpStartGameModeParams->GetBoostEarning());   // action +0x8C8
// and BOTH of its ends are closed in this same round, from this one asm window.
// -----------------------------------------------------------------------------
f32 StartGameModeParams::GetBoostEarning() const
{
    return mfBoostEarning;
}

// -----------------------------------------------------------------------------
// GetShotGroup -- miShotGroup, console +0x320 (800).
// The instruction PAIRED with GetBoostEarning's in PrepareForMode: lwz r9, 0x320(r31)
// @0x82342A90, stored to the action record at +0x8CC == miShotGroup. Read as a WORD immediately
// next to a float read of the neighbouring slot, which is what separates the two. Construct
// writes -1 (stw r10(-1), 0x320 @0x8231C294), so that "no shot group" sentinel is a legal
// return. Consumer side: BrnModeManager_Prepare.cpp:576.
// -----------------------------------------------------------------------------
s32 StartGameModeParams::GetShotGroup() const
{
    return miShotGroup;
}

// -----------------------------------------------------------------------------
// GetEventJunctionId -- muEventJunctionId, console +0x328 (808).
// SurvivorMode::Start @0x82332408 does lwz r11, 0x328(r29) -> stw r11, 0x44(r31), and
// GameModeParams+0x44 is muEventJunctionID in the committed BrnModeManager_Start.cpp and
// BrnModeManager_IntroPlay.cpp tables -- a same-name copy across the two param blocks.
// NOTE this is one of only two members Construct does NOT write (0x328 is absent from the store
// cluster), so it carries whatever SetEventJunctionId last put there. Deliberate on the console.
// -----------------------------------------------------------------------------
u32 StartGameModeParams::GetEventJunctionId() const
{
    return muEventJunctionId;
}

// -----------------------------------------------------------------------------
// GetJunctionID -- muJunctionID, console +0x330 (816).
// The very next load at the same call site: SurvivorMode::Start @0x82332418 does
// lwz r11, 0x330(r29) -> stw r11, 0x48(r31) == GameModeParams::muJunctionID. The two junction
// ids copy across as an adjacent pair at BOTH ends (+0x328/+0x330 -> +0x44/+0x48), which is what
// keeps them from being swapped for one another. Construct clears this one (stw r11(0), 0x330).
// -----------------------------------------------------------------------------
u32 StartGameModeParams::GetJunctionID() const
{
    return muJunctionID;
}

// -----------------------------------------------------------------------------
// GetEventData -- mpEventData, console +0x32C (812).
// Named at +0x32C by BrnProgressionManager.cpp:667's committed transcript
// (0x8237B6C4 lwz r19, 0x32C(r5), with a3 == lpStartGameModeParams) and read inlined by
// SurvivorMode::Start @0x823322D4.
// NO ASSERT IN THE GETTER. The null test that follows the load at SurvivorMode::Start belongs to
// the CALLER: it fires with file string aDP4B5MainBurno_100 at line 67 and the message
// "lpEventData" -- a local variable's name -- whereas an inlined getter assert carries THIS
// header's file string (aDP4B5MainBurno_37, the same one Construct's assert uses) and a member's
// name, exactly as GetProgressionRankData's does below. That file-string/line difference is the
// whole reason this body is bare and the next one is not; it is not a judgement call.
// -----------------------------------------------------------------------------
const BrnProgression::RaceEventData* StartGameModeParams::GetEventData() const
{
    return mpEventData;
}

// -----------------------------------------------------------------------------
// GetProgressionRankData -- mpProgressionRankData, console +0x334 (820).
// The member is named directly by the X360-attested setter bodied above
// (SetProgressionRankData @0x823544D4 stw r31, 0x334(r30)), so this getter's slot needs no
// inference at all.
// IT CARRIES ITS OWN ASSERT, recovered from PursuitMode::Start @0x823220B4..0x823220E4:
//     lwz r11, 0x334(r29) ; cmplwi cr6, r11, 0 ; bne cr6, skip
//     BeginAssert ; r3 = "mpProgressionRankData != NULL"
//                 ; r4 = aDP4B5MainBurno_37 ; r5 = 0x3B1 ; FireAssert ; EndAssert
// r4 is the SAME file string StartGameModeParams::Construct's own assert uses (== this class's
// header, BrnGameModeParams.h) and r5 == 945 is a line inside the class -- both marks of an
// inlined member assert rather than a caller's. The caller then RE-loads +0x334 @0x823220E8 and
// asserts again against a different file string; that second one is PursuitMode's, not ours.
// Non-gating tripwire, like the rest of the CGS_ASSERT family: the console returns the pointer
// either way.
// -----------------------------------------------------------------------------
const BrnProgression::ProgressionRankData* StartGameModeParams::GetProgressionRankData() const
{
    CGS_ASSERT(mpProgressionRankData != NULL, "mpProgressionRankData != NULL");
    return mpProgressionRankData;
}

// -----------------------------------------------------------------------------
// GetProgressionRankAsRatio -- mfProgressionRankAsRatio, console +0x338 (824).
// Member named directly by its X360-attested setter above (SetProgressionRankAsRatio
// @0x8235456C stfs f31, 0x338(r30)). Read back inlined as a float by PursuitMode::Start
// @0x82322184 and twice by SurvivorMode::Start (@0x82332394, @0x8233256C). ALL THREE reads
// are immediately preceded by the getter's OWN inlined `mpProgressionRankData != NULL`
// assert -- BrnGameModeParams.h line 0x3C0 == 960, DISTINCT from GetProgressionRankData's
// 945, and the `lwz r11, 0x334` there is loaded only for the compare (closure verify
// 2026-08-26, PursuitMode 0x82322160..80 / SurvivorMode 0x8233236C..8C + 0x82332550..68).
// -----------------------------------------------------------------------------
f32 StartGameModeParams::GetProgressionRankAsRatio() const
{
    CGS_ASSERT(mpProgressionRankData != NULL, "mpProgressionRankData != NULL");
    return mfProgressionRankAsRatio;
}

// =============================================================================
// ⭐⭐ [stuntrace wave D, D3] THE EIGHT WRITE TWINS THE OFFLINE EVENT START NEEDS.
//
// All eight were DECLARE-ONLY. They have no out-of-line X360 symbol -- like the fifteen read
// accessors above, the console INLINES every one of them -- and the producer that inlines all
// eight in one run is GameStateModule::StartModeAtLights @0x82396CF8, whose frame base for the
// params is `var_3D0` (0x430 - 0x3D0 == +0x60 on its stack). Subtracting that base from each
// store gives the member offset directly, and every one lands inside the dense Construct
// cluster the banner above already pinned:
//
//   0x82396FF0/FF8  lfs f0, 8(event)    -> stfs var_B8  == +0x378-0x60 == 0x318 (792)  mfTrafficDensity
//   0x82397000/004  lfs f0, 0xC(event)  -> stfs var_B4  == +0x37C-0x60 == 0x31C (796)  mfBoostEarning
//   0x82397268      stw r11 (junction+0xC) -> var_B0    == +0x380-0x60 == 0x320 (800)  miShotGroup
//   0x82397024      stw r23 (muEventJunctionID) -> var_A8 == 0x328 (808)               muEventJunctionId
//   0x82397020      stw r31 (lpEventData)       -> var_A4 == 0x32C (812)               mpEventData
//   0x82397038      stw r11 (JunctionLogicBox::muID) -> var_A0 == 0x330 (816)          muJunctionID
//   0x82397294      stvx128 v0 (LightTriggerStartData::GetStartDirection) -> var_E0
//                                                     == +0x350-0x60 == 0x2F0 (752)    mStartDirection
//   0x823972A0      stfs f1 (Profile::GetPlayerBaseDeformAmount) -> var_AC == 0x324 (804)
//                                                                                      mfPlayerBaseDeformation
//
// Six of the eight are the exact write twins of read accessors already bodied above from
// INDEPENDENT consumer asm (GetTrafficDensity, GetBoostEarning, GetShotGroup,
// GetEventJunctionId, GetEventData, GetJunctionID, GetStartDirection) -- so each member mapping
// here has two witnesses on opposite sides of the seam, not one. Plain stores; no asserts (the
// console emits none at any of the eight store sites).
// =============================================================================

void StartGameModeParams::SetTrafficDensity(f32 lfTrafficDensity)
{
    mfTrafficDensity = lfTrafficDensity;
}

void StartGameModeParams::SetBoostEarning(f32 lfBoostEarning)
{
    mfBoostEarning = lfBoostEarning;
}

void StartGameModeParams::SetShotGroup(s32 liShotGroup)
{
    miShotGroup = liShotGroup;
}

void StartGameModeParams::SetEventJunctionId(u32 luEventJunctionId)
{
    muEventJunctionId = luEventJunctionId;
}

void StartGameModeParams::SetEventData(const BrnProgression::RaceEventData* lpEventData)
{
    mpEventData = lpEventData;
}

void StartGameModeParams::SetJunctionID(u32 luJunctionID)
{
    muJunctionID = luJunctionID;
}

void StartGameModeParams::SetStartDirection(Vector3 lStartDirection)
{
    mStartDirection = lStartDirection;
}

void StartGameModeParams::SetPlayerBaseDeformation(f32 lfPlayerBaseDeformation)
{
    mfPlayerBaseDeformation = lfPlayerBaseDeformation;
}
}
