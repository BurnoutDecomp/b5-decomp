#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // BrnGameState::ModeManager::GetCurrentGameMode
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"     // BrnGameState::GameMode::IsOnline
#include "GameSource/GameState/BrnGameStateSharedIO.h"                  // GameStateModuleIO::EGameModeType (E_MODE_*_SHOWTIME)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"     // BrnProgression::ProgressionManager::GetProfile
#include "GameSource/GameState/Progression/BrnProfile.h"                // BrnProgression::Profile::SetCarUnlockAlreadyShown
#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // GameStateModuleIO::OutputBuffer (owned by pointer)
#include "SharedClasses/DataLists/VehicleList.h"                        // BrnResource::VehicleList (GetVehicleIndex / GetVehicleData)
#include "SharedClasses/DataLists/VehicleListEntry.h"                   // BrnResource::VehicleListEntry (parent id / livery + car type / stats)
#include "SharedClasses/Progression/BrnProgressionData.h"               // BrnProgression::ProgressionData::FindCarOpponentSet
#include "SharedClasses/Progression/BrnOpponentData.h"                  // BrnProgression::CarOpponentSet (opponent walk)
#include "GameShared/GameClasses/Containers/CgsArray.h"                 // CgsContainers::Array<s64,7> (opponent payload)

namespace BrnGameState
{
// The verbatim X360-baked source path every assert in this TU references.
static const char* const KAC_GSM_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp";
static const char* const KAC_OPPONENTDATA_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\sharedclasses\\progression\\BrnOpponentData.h";

// ----------------------------------------------------------------------------
// X360-attested game-action event-type ids + payload sizes (the `li r5,<type>` / `li r6,<size>`
// immediates each AddEvent is given). The payload structs are GameStateModuleIO action records
// whose field layouts are not in this TU's exports, so each is posted as a sized buffer with the
// bytes the X360 actually writes reproduced at their attested in-payload offsets.
// ----------------------------------------------------------------------------
enum EGsmGameAction
{
    KI_ACTION_PLAYER_CAR_CHANGED   = 1,     // size 8   -- OnSpecialEventPlayerCarChange (the new car id)
    KI_ACTION_CAR_OPPONENT_SET     = 4,     // size 64  -- OnPlayerCarChange (Array<CgsID,7> of opponents)
    KI_ACTION_UNPAUSE              = 87,    // size 1   -- RequestUnpause
    KI_ACTION_APPLY_CAR_STATS      = 198,   // size 24  -- ApplyCarStats
};

// ----------------------------------------------------------------------------
// Construct / Destruct.
//
// The console's GameStateModule is a CgsModule::ModuleSingleBuffered, so its
// GameStateModuleIO::OutputBuffer is the DataStructure the base allocates from the module's
// own DataBuffer inside Prepare() -> CreateOutputDataStructure(). ⚠️ FLAG (PC bring-up seam):
// nothing on PC calls this module's Prepare(), so that allocation point never runs. The
// buffer is newed here instead and freed in Destruct(); the buffer TYPE, its Construct
// (X360 0x82382940) and its accessors are the real console ones -- only the allocation SITE
// moves. DELETE-WHEN the module's Prepare()/CreateOutputDataStructure() path is real.
//
// Constructing it is what makes the game-state -> director game-action route exist at all:
// BrnGameModule::BridgeGameStateToDirector @0x823CD170 Appends this buffer's +0x04 queue into
// the director input buffer's queue every frame, and MainDirector::ProcessInputQueue drains it.
// ----------------------------------------------------------------------------
void GameStateModule::Construct()
{
    CgsModule::ModuleSingleBuffered::Construct();

    if (mpOutputBuffer == 0)
    {
        mpOutputBuffer = new GameStateModuleIO::OutputBuffer();
        mpOutputBuffer->Construct();
    }
}

void GameStateModule::Destruct()
{
    if (mpOutputBuffer != 0)
    {
        delete mpOutputBuffer;
        mpOutputBuffer = 0;
    }

    CgsModule::ModuleSingleBuffered::Destruct();
}

// X360 @ 0x823116D0. Returns whether the currently-running game mode is one of the online modes. May
// only be called while the module is updating (asserts mbIsUpdating). Fetches the current game mode
// from the embedded ModeManager and forwards to GameMode::IsOnline(); if there is no current mode,
// returns false. (X360 reads the current-mode pointer inline as *(this + 0x1DB8) inside mModeManager
// and its mbIsOnline at *(mode + 172); de-inlined to the two logical calls.)
bool GameStateModule::IsOnlineGameMode()
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");

    const GameMode* lpCurrentGameMode = mModeManager.GetCurrentGameMode();
    if (lpCurrentGameMode != nullptr)
    {
        return lpCurrentGameMode->IsOnline();
    }
    return false;
}

// The cached current-game-mode type. The X360 reads it as the raw scalar just below the embedded
// mModeManager (GameStateModule+7604 == mModeManager+0xD94, meCurrentGameModeType); de-inlined to
// the ModeManager's own named accessor -- same read, no offset poke.
GameStateModuleIO::EGameModeType GameStateModule::GetCurrentGameModeType() const
{
    return mModeManager.GetCurrentGameModeType();
}

// X360 @ 0x82311620. Returns the player's GLOBAL race-car index (its slot in the full world race-car
// table). May only be called while the module is updating (asserts mbIsUpdating).
s32 GameStateModule::GetPlayerGlobalRaceCarIndex()
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
    return miPlayerGlobalRaceCarIndex;
}

// X360 @ 0x82356870. Returns whether the active race car in slot leRaceCarIndex is currently crashing.
// Asserts the module is updating and that the index is in [E_ACTIVE_RACE_CAR_INDEX_0,
// E_ACTIVE_RACE_CAR_INDEX_COUNT); reproduces the three X360 asserts verbatim (the module-updating one,
// then the two range guards) before returning the cached per-slot crash flag.
bool GameStateModule::IsRaceCarCrashing(::EActiveRaceCarIndex leRaceCarIndex)
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
    CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    return maRaceCarCrashing[leRaceCarIndex];
}

// X360 @ 0x823567A8. True when the current game mode is a showtime mode (offline or online). May only
// be called while the module is updating (asserts mbIsUpdating). The X360 reads the current game-mode
// type inline (this+7604) and tests == E_MODE_OFFLINE_SHOWTIME (2) || == E_MODE_ONLINE_SHOWTIME (16);
// de-inlined to the GetCurrentGameModeType() accessor (same read) to avoid a raw offset access.
bool GameStateModule::IsShowtimeGameMode()
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");

    const GameStateModuleIO::EGameModeType leGameModeType = GetCurrentGameModeType();
    return leGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME
        || leGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME;
}

// X360 @ 0x82356978. True when the simulation is currently paused. miSimPauseFlags is a bitfield of
// active pause reasons; a nonzero value means paused. When lbCheckGameMode is set and the current mode
// is online, some pause-reason bits are ignored: the X360 masks off bits {2,3,5} (mask 0xFFFFFFD3, the
// strict path -- returns immediately) when lbStrictMask is set, otherwise bits {1,2,3,5}
// (mask 0xFFFFFFD1). (The two bool parameters are named from the asm's branch structure; their exact
// call-site meaning is confirmed when the pause callers -- RequestPause/RequestUnpause -- are homed.)
bool GameStateModule::IsSimPaused(bool lbCheckGameMode, bool lbStrictMask) const
{
    s32 liPauseFlags = miSimPauseFlags;
    if (lbCheckGameMode)
    {
        const GameMode* lpCurrentGameMode = mModeManager.GetCurrentGameMode();
        if (lpCurrentGameMode != nullptr && lpCurrentGameMode->IsOnline())
        {
            if (lbStrictMask)
            {
                return (liPauseFlags & ~0x2C) != 0;   // X360 mask 0xFFFFFFD3 -- clear bits {2,3,5}
            }
            liPauseFlags &= ~0x2E;                     // X360 mask 0xFFFFFFD1 -- clear bits {1,2,3,5}
        }
    }
    return liPauseFlags != 0;
}

// X360 @ 0x823566F8. Hands back the module's per-frame output GUI event queue (the
// CgsModule::VariableEventQueue<18432,16> that the PaybackManager and other managers publish their
// HUD/GUI events onto). May only be called while the module is updating (asserts mbIsUpdating).
CgsModule::VariableEventQueue<18432, 16>* GameStateModule::GetOutputGuiEventQueue()
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
    return &mOutputGuiEventQueue;
}

// X360 @ 0x82363698. Mark car lCarId as already-shown in the unlock sequence. The X360 resolves the
// player Profile through the embedded progression manager (GetProfile inlines to the by-value Profile
// sub-object), asserts it is non-null with the verbatim message, then forwards to
// Profile::SetCarUnlockAlreadyShown.
void GameStateModule::SetCarUnlockAlreadyShown(CgsID lCarId)
{
    BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
    CGS_ASSERT(lpProfile != nullptr, "mProgressionManager.GetProfile()");
    lpProfile->SetCarUnlockAlreadyShown(lCarId);
}

// ============================================================================================
// THE JUNKYARD / CAR-SELECT PRODUCER SURFACE (2026-08-01).
//
// Everything below is what CarSelectManager reaches for through mpGameStateModule. Each body is
// recovered from the X360 ASM, not the Hex-Rays prototype: on the X360 a CgsID is a full 64-bit
// value in ONE 64-bit GPR (`stdx r4, this, 0x456D8`), and Hex-Rays renders those register pairs
// as a single `__int64 a2`, dropping every argument after it without a trace. Three of the five
// signatures below would have been wrong if taken from the pseudocode.
// ============================================================================================

// The loaded vehicle list (X360 `lwzx rN, this, 0x456E8`).
BrnResource::VehicleList* GameStateModule::GetVehicleList()
{
    return mpVehicleList;
}

// The active player car / wheel ids the CarSelect FSM compares against its desired car
// (X360 raw reads at this+0x456D8 / +0x456E0; both are written by OnSpecialEventPlayerCarChange).
CgsID GameStateModule::GetActivePlayerCarId() const
{
    return mActivePlayerCarId;
}

CgsID GameStateModule::GetActivePlayerWheelId() const
{
    return mActivePlayerWheelId;
}

// The module's cached active-race-car snapshot (X360 embedded interface at this+0x397E0).
const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
    GameStateModule::GetLastActiveRaceCarInterface() const
{
    return &mLastActiveRaceCarInterface;
}

// ⭐ The raw `*(this + 232288)` nonzero test three console call sites open-code. That word IS
// miSimPauseFlags (see the header note), so this is exactly IsSimPaused(false, false).
bool GameStateModule::IsTrainingPauseSuppressed() const
{
    return miSimPauseFlags != 0;
}

// --------------------------------------------------------------------------------------------
// RequestUnpause (X360 0x82382138).
// Clear the leUnpauseModule pause-reason bits from miSimPauseFlags. The console samples
// IsSimPaused BEFORE and AFTER the clear and only broadcasts the unpause action (87, 1B) when the
// answer actually changed; the two asserts guard the "we asked to unpause and ended up paused"
// inversions. The X360 keeps `lbWasPausedBefore = (flags != 0)` from the PRE-clear word and
// `lbPausedAfterClear = (newFlags != 0)` from the post-clear word, which is why the second assert
// can fire even on the no-change path.
// --------------------------------------------------------------------------------------------
void GameStateModule::RequestUnpause(s32 leUnpauseModule, GameStateModuleIO::GameActionQueue* lpQueue)
{
    if (leUnpauseModule == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("leUnpauseModule != E_PAUSE_NONE", KAC_GSM_FILE, 6159);
        CgsDev::Assert::EndAssert();
    }

    const bool lbSimPausedBefore = IsSimPaused(false, false);

    const s32  liPreviousFlags = miSimPauseFlags;
    const s32  liNewFlags      = liPreviousFlags & ~leUnpauseModule;
    const bool lbWasPausedBefore = (liPreviousFlags != 0);
    miSimPauseFlags = liNewFlags;

    const bool lbSimPausedAfter  = IsSimPaused(false, false);
    const bool lbStillPaused     = (liNewFlags != 0);

    if (lbSimPausedAfter == lbSimPausedBefore)
    {
        if (lbStillPaused != lbWasPausedBefore && lbStillPaused)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Paused from RequestUnpause...?", KAC_GSM_FILE, 6187);
            CgsDev::Assert::EndAssert();
        }
        return;
    }

    if (lbSimPausedAfter)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Sim paused from RequestUnpause...?", KAC_GSM_FILE, 6182);
        CgsDev::Assert::EndAssert();
    }

    u8 lacUnpause[1] = { 0 };   // X360 posts the uninitialised 1-byte local verbatim
    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacUnpause), KI_ACTION_UNPAUSE, 1);
}

// --------------------------------------------------------------------------------------------
// ApplyCarStats (X360 0x82381188).
// Publish the newly-selected car's gameplay stats out of its VehicleListEntry as the 24-byte
// action 198. The six payload words and their sources are read straight off the asm:
//   [+0]  = entry byte @0xE8 + 1  -> no; see the store map below (byte offsets, not indices).
// X360 store map (`li r6, 0x18` == 24 bytes, `li r5, 0xC6` == 198):
//   payload +0x00 <- lbz entry+0x99      payload +0x0C <- lbz entry+0x98
//   payload +0x04 <- lbz entry+0x9B      payload +0x10 <- lfs entry+0x90 (f32)
//   payload +0x08 <- lbz entry+0x9A      payload +0x14 <- lbz entry+0xE8
// ⚠️ FLAG: entry+0x90..+0x9B is inside VehicleListEntry's leading opaque header (maPad0), whose
// individual gameplay fields are not named yet -- CanAutoRepair()/IsTrophyCar()/GetUnlockRank()
// are the three bits of it that have been recovered so far. The five reads here are therefore
// taken as raw bytes/word from that region rather than through named accessors; entry+0xE8 IS
// named (GetCarType()). DELETE-WHEN the VehicleListEntry gameplay-data sub-object is homed.
// --------------------------------------------------------------------------------------------
void GameStateModule::ApplyCarStats(CgsID lCarId, GameStateModuleIO::GameActionQueue* lpQueue)
{
    const BrnResource::VehicleList* lpVehicleList = mpVehicleList;
    const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lCarId);
    const BrnResource::VehicleListEntry* lpVehicleListEntry =
        (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);

    if (lpVehicleListEntry == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpVehicleListEntry", KAC_GSM_FILE, 4720);
        CgsDev::Assert::EndAssert();
        return;   // the X360 falls through into a null deref; bail instead of faulting
    }

    const u8* lpcEntryBytes = reinterpret_cast<const u8*>(lpVehicleListEntry);

    struct ApplyCarStatsAction
    {
        s32 miStatA;        // entry +0x99
        s32 miStatB;        // entry +0x9B
        s32 miStatC;        // entry +0x9A
        s32 miStatD;        // entry +0x98
        f32 mfStatE;        // entry +0x90
        s32 miCarType;      // entry +0xE8 == GetCarType()
    };
    ApplyCarStatsAction lAction;
    lAction.miStatA   = lpcEntryBytes[0x99];
    lAction.miStatB   = lpcEntryBytes[0x9B];
    lAction.miStatC   = lpcEntryBytes[0x9A];
    lAction.miStatD   = lpcEntryBytes[0x98];
    std::memcpy(&lAction.mfStatE, lpcEntryBytes + 0x90, sizeof(f32));
    lAction.miCarType = lpVehicleListEntry->GetCarType();

    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                      KI_ACTION_APPLY_CAR_STATS, sizeof(ApplyCarStatsAction));
}

// --------------------------------------------------------------------------------------------
// GetOriginalCarId (X360 0x823758E8).
// Walk lCarId up its VehicleListEntry parent chain to the base ("original") car a livery variant
// derives from. The console walks AT MOST TWO levels -- car -> parent -> grandparent -- and
// returns the deepest non-null id it reaches. The leading redundant lookup exists only to carry
// the console's own assert.
// --------------------------------------------------------------------------------------------
CgsID GameStateModule::GetOriginalCarId(CgsID lCarId)
{
    const BrnResource::VehicleList* lpVehicleList = mpVehicleList;

    {
        const s32 liIndex = lpVehicleList->GetVehicleIndex(lCarId);
        if (liIndex < 0 || lpVehicleList->GetVehicleData(liIndex) == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("NULL != mpVehicleList->GetVehicleData(lCarId)", KAC_GSM_FILE, 5545);
            CgsDev::Assert::EndAssert();
            return lCarId;   // the X360 falls through into a null deref; bail instead of faulting
        }
    }

    const s32 liCarIndex = lpVehicleList->GetVehicleIndex(lCarId);
    const BrnResource::VehicleListEntry* lpCarEntry =
        (liCarIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liCarIndex);
    const CgsID lParentId = lpCarEntry->GetParentId();
    if (lParentId == 0)
    {
        return lCarId;
    }

    const s32 liParentIndex = lpVehicleList->GetVehicleIndex(lParentId);
    const BrnResource::VehicleListEntry* lpParentEntry =
        (liParentIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liParentIndex);
    const CgsID lGrandParentId = (lpParentEntry != 0) ? lpParentEntry->GetParentId() : 0;
    return (lGrandParentId != 0) ? lGrandParentId : lParentId;
}

// --------------------------------------------------------------------------------------------
// OnSpecialEventPlayerCarChange (X360 0x8238FB40).
// The single point every player-car swap funnels through: cache the new car + wheel ids, tell the
// progression layer, publish the car's stats, stamp the car's TYPE onto the profile, and broadcast
// the 8-byte "player car changed" action (1).
// ARG SHAPE FROM ASM: r3=this, r4=carId(std @0x456D8), r5=wheelId(std @0x456E0), r6=queue,
// r7=the bool forwarded to ProgressionManager::OnPlayerCarChange.
// --------------------------------------------------------------------------------------------
void GameStateModule::OnSpecialEventPlayerCarChange(CgsID lCarId, CgsID lWheelId,
                                                    GameStateModuleIO::GameActionQueue* lpQueue,
                                                    bool lbUpdateProfile)
{
    mActivePlayerCarId   = lCarId;
    mActivePlayerWheelId = lWheelId;

    mProgressionManager.OnPlayerCarChange(lCarId, lWheelId, lbUpdateProfile);
    ApplyCarStats(lCarId, lpQueue);

    const BrnResource::VehicleList* lpVehicleList = mpVehicleList;
    const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lCarId);
    const BrnResource::VehicleListEntry* lpPlayerCarVehicleListEntry =
        (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);

    if (lpPlayerCarVehicleListEntry == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lPlayerCarVehicleListEntry != NULL", KAC_GSM_FILE, 4694);
        CgsDev::Assert::EndAssert();
        return;   // the X360 falls through into a null deref; bail instead of faulting
    }

    // X360 `lbz r29, 0xE8(entry)` -> the car TYPE byte, stored into the profile's cached
    // "current car type" word (Profile +117948, mProfile.meCurrentCarType).
    const u8 luCarType = lpPlayerCarVehicleListEntry->GetCarType();

    BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProfile != NULL", KAC_GSM_FILE, 4698);
        CgsDev::Assert::EndAssert();
        return;
    }
    lpProfile->SetCurrentCarType(static_cast<s32>(luCarType));

    CgsID lNewCarId = lCarId;
    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lNewCarId),
                      KI_ACTION_PLAYER_CAR_CHANGED, sizeof(CgsID));
}

// --------------------------------------------------------------------------------------------
// OnPlayerCarChange (X360 0x82396B88).
// The offline junkyard-exit path. Does everything OnSpecialEventPlayerCarChange does, then looks
// up the AI opponent set for the car's ORIGINAL (base) id at the player's current progression rank
// and broadcasts up to seven opponent car ids as the 64-byte action 4.
// ARG SHAPE FROM ASM: identical to OnSpecialEventPlayerCarChange -- r3..r7 are forwarded to it
// unchanged (the X360 does not even reload them).
// --------------------------------------------------------------------------------------------
void GameStateModule::OnPlayerCarChange(CgsID lCarId, CgsID lWheelId,
                                        GameStateModuleIO::GameActionQueue* lpQueue,
                                        bool lbUpdateProfile)
{
    mActivePlayerCarId   = lCarId;
    mActivePlayerWheelId = lWheelId;

    // The X360 zeroes the opponent array's count word BEFORE the forwarded call (the local lives
    // across it), i.e. Array<CgsID,7>::Construct().
    Array<s64, 7> laOpponentCarIds;
    laOpponentCarIds.Construct();

    OnSpecialEventPlayerCarChange(lCarId, lWheelId, lpQueue, lbUpdateProfile);

    const BrnProgression::ProgressionData* lpProgressionData = mProgressionManager.GetProgressionData();
    if (lpProgressionData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProgressionData != NULL", KAC_GSM_FILE, 4636);
        CgsDev::Assert::EndAssert();
    }
    else
    {
        const CgsID lOriginalCarId = GetOriginalCarId(lCarId);
        // X360 `extsb r5, r3` -- the rank is sign-extended from a BYTE before the lookup.
        const s32 liProgressionRank =
            static_cast<s32>(static_cast<s8>(mProgressionManager.GetProgressionRank()));

        const BrnProgression::CarOpponentSet* lpCarOpponentSet =
            lpProgressionData->FindCarOpponentSet(lOriginalCarId, liProgressionRank);
        if (lpCarOpponentSet != 0)
        {
            s32 liOpponents = lpCarOpponentSet->GetOpponentCount();
            if (liOpponents >= static_cast<s32>(Array<s64, 7>::KU_SIZE))
            {
                liOpponents = static_cast<s32>(Array<s64, 7>::KU_SIZE);
            }
            for (s32 liCarOpponentIndex = 0; liCarOpponentIndex < liOpponents; ++liCarOpponentIndex)
            {
                if (liCarOpponentIndex < 0 || liCarOpponentIndex >= lpCarOpponentSet->GetOpponentCount())
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "liCarOpponentIndex >= 0 && liCarOpponentIndex < miOpponentCount",
                        KAC_OPPONENTDATA_FILE, 224);
                    CgsDev::Assert::EndAssert();
                }
                laOpponentCarIds.Append(
                    static_cast<s64>(lpCarOpponentSet->GetCarOpponent(liCarOpponentIndex)->GetCarId()));
            }
        }
    }

    // X360 `li r6, 0x40` -- the whole Array<CgsID,7> (7*8 elements + count) goes on the wire.
    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&laOpponentCarIds),
                      KI_ACTION_CAR_OPPONENT_SET, sizeof(laOpponentCarIds));
}

// --------------------------------------------------------------------------------------------
// RequestStreamingForVehicleSelection (X360 0x82382550).
//
// ⛔ HONEST PARTIAL -- READ THIS BEFORE TRUSTING THE CALL SITES.
//
// The console body builds the junkyard carousel's PRE-STREAM window: it asks
// GetListOfPlayerSelectableVehicles (X360 0x82376500) for the player's full selectable-car list
// into an Array<CgsID,128>, finds lCarId in it (falling back to the car's PARENT id when lCarId is
// a livery variant -- entry+0xE9 in {1,3,4}), then walks outward from that index collecting up to
// KI_MAX_ACTIVE_RACE_CARS(8) neighbours with a per-entry direction tag, and posts the resulting
// 88-byte action 69.
//
// WHAT IS REPRODUCED HERE: the lookup of the requested car, its livery->parent fallback, and both
// of the console's asserts. WHAT IS NOT: the selectable-list build and the neighbour window,
// because GetListOfPlayerSelectableVehicles is 183 instructions of its own and reaches four
// GameStateModule members that are not modelled on this slice (+183860 / +183937 / +183944 -- the
// online-event car-restriction state -- plus the profile car walk).
//
// WHY THIS IS NOT A SILENT DROP: (a) it logs, once per changed car, exactly what it did not send;
// (b) NOTHING IN THIS BUILD CONSUMES ACTION 69 -- the console consumer is
// RaceCarEntityModule::HandleSelectionRequestStreamingAction @0x822E9918, which is not
// reconstructed (grep for it: the only hit in the tree is a comment). So today the only observable
// difference between this and the full body is the log line.
// DELETE-WHEN GetListOfPlayerSelectableVehicles lands (then the window build comes back with it).
// --------------------------------------------------------------------------------------------
void GameStateModule::RequestStreamingForVehicleSelection(CgsID lCarId)
{
    const BrnResource::VehicleList* lpVehicleList = mpVehicleList;
    if (lpVehicleList == 0)
    {
        return;
    }

    CgsID lStreamCarId = lCarId;

    const s32 liCurrentVehicleIndex = lpVehicleList->GetVehicleIndex(lCarId);
    const BrnResource::VehicleListEntry* lpCurrentVehicleData =
        (liCurrentVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liCurrentVehicleIndex);
    if (lpCurrentVehicleData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpCurrentVehicleData != NULL", KAC_GSM_FILE, 7461);
        CgsDev::Assert::EndAssert();
    }
    else
    {
        // A livery variant is not itself in the selectable list -- the console re-looks-up its
        // parent (X360: entry+0xE9 in {1,3,4} -> use GetParentId()).
        const u8 luLiveryType = lpCurrentVehicleData->GetLiveryType();
        if (luLiveryType == 1 || luLiveryType == 3 || luLiveryType == 4)
        {
            if (lpCurrentVehicleData->GetParentId() == 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpCurrentVehicleData->GetParentId() != kCGSID_NULL",
                                           KAC_GSM_FILE, 7465);
                CgsDev::Assert::EndAssert();
            }
            lStreamCarId = lpCurrentVehicleData->GetParentId();
        }
    }

    static CgsID slLastLoggedCarId = 0;
    if (slLastLoggedCarId != lStreamCarId && CgsDev::Log::gpDebugPrint != 0)
    {
        slLastLoggedCarId = lStreamCarId;
        *CgsDev::Log::gpDebugPrint
            << "[FLAG PC bring-up] RequestStreamingForVehicleSelection(" << static_cast<u32>(lStreamCarId)
            << "): the 88-byte carousel pre-stream action (69) is NOT posted -- "
               "GetListOfPlayerSelectableVehicles is not reconstructed. Nothing in this build "
               "consumes action 69 either (HandleSelectionRequestStreamingAction is absent), so "
               "no consumer is being starved today.\n";
    }
}
}
