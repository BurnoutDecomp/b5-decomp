// BrnGameState::CarSelectManager -- CAR-CHANGE / UNLOCK-LIST / STREAMING / SPAWN method group.
//
// A class's methods may live across several .cpp TUs; this is the second body file for
// CarSelectManager (the FSM-core/state group lives in BrnCarSelectManager.cpp). It bodies the
// unlock-car-list build (Setup{Normal,Shutdown}UnlockList / GetNextUnlockCarID /
// IsThisCarInCurrentUnlockSequence), the actual car swap (RequestChangeCar /
// RequestStreamingForUnlock / StreamingFinished / TeleportCurrentVehicle / SpawnInStartCar /
// SetupSpawnLocations), GetProfileCarData, and DEBUG_UnlockCarsForTesting.
//
// Behaviour from the X360 ARTIST pseudocode; shape from the BrnCarSelectManager DWARF + the frozen
// owning header. The console class uses 4-byte pointers + 8-byte CgsID; we bind to the named members
// of the (parity-by-named-member) frozen header, NOT to absolute console byte offsets.

#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"

#include <cstring>   // memset / memcpy (opaque action payloads)

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CgsDev::Assert Begin/Fire/EndAssert
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue<13312,16> (game-action queue)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags

#include "SharedClasses/Trigger/BrnSpawnLocation.h"                 // BrnTrigger::SpawnLocation (Type / GetType / junkyard id / position)
#include "SharedClasses/Trigger/BrnTriggerData.h"                  // BrnTrigger::TriggerData (GetSpawnLocation / GetSpawnLocationCount)
#include "SharedClasses/DataLists/VehicleList.h"                    // BrnResource::VehicleList (GetVehicleIndex / GetVehicleData / count)
#include "SharedClasses/DataLists/VehicleListEntry.h"              // BrnResource::VehicleListEntry (unlock rank / trophy-car bit)
#include "GameSource/GameState/Progression/BrnProgressionCarData.h" // BrnProgression::CarData (Id / unlock type / deform)
#include "GameSource/GameState/Progression/BrnProfile.h"           // BrnProgression::Profile (GetCarData / GetCarCount / FindCar / livery)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"// BrnProgression::ProgressionManager (rank / AddCar / colour+palette)
#include "GameSource/GameState/BrnGameStateModule.h"               // BrnGameState::GameStateModule (online mode / last-active-race-car)
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // BrnGameState::TriggerQueryManager (GetTriggerData)
#include "GameSource/GameState/BrnGameActions.h"                   // GameStateModuleIO::CarSelectChangeColourAction (typed payload)

namespace BrnGameState
{
// The game-action queue the callers hand us is an GameStateModuleIO::GameActionQueue* (X360 == the
// OutputBuffer's embedded queue). Reinterpret it to the real backing type and post events via
// AddEvent (same precedent the sibling FSM-core TU uses).
typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueueImpl;

static inline GameActionQueueImpl* AsActionQueue(GameStateModuleIO::GameActionQueue* lpQueue)
{
    // IDENTITY. GameStateModuleIO::GameActionQueue IS CgsModule::VariableEventQueue<13312,16>
    // (BrnGameStateSharedIO.h). The helper survives only as the call-site vocabulary.
    return lpQueue;
}

// The verbatim X360-baked source paths the asserts reference (shared with the sibling TU; redeclared
// here as this is a separate translation unit).
static const char* const KAC_CSM_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/CarSelect/BrnCarSelectManager.cpp";
static const char* const KAC_SPAWNLOC_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\sharedclasses\\trigger\\BrnSpawnLocation.h";
static const char* const KAC_BRNPROFILE_FILE =
    "..\\..\\..\\GameSource\\GameState/Progression/BrnProfile.h";

// X360-baked tuning constant the DEBUG unlock path stamps onto a force-unlocked trophy car's deform
// field (CarData +0x0C). flt_82029BB8 == 0.85 (1062836634 == 0x3F59999A) -- the "start-of-game /
// liveryselect" deform amount the sibling TU names KF_LIVERYSELECT_DELAY_BEFORE_CHANGE.
static const f32 KF_DEBUG_TROPHY_CAR_DEFORM = 0.85f;   // flt_82029BB8 (1062836634)

// ----------------------------------------------------------------------------
// SetupSpawnLocations nearest-drive-in geometry helpers (file-local).
//
// FLAG: these reproduce the X360 control flow for choosing which of the two type-0 "drive-in" spawn
// slots is nearer the player's last car. The actual reads (the active-car validity byte at interface
// +0x2860, the player position from sub_82310398, and each SpawnLocation's position vector) live
// behind sub-offsets NOT modelled on the minimal slices in scope. They return the X360-default taken
// when the player has no valid last car (don't swap -> keep discovery order). Replace the bodies when
// the RaceCar output interface + SpawnLocation position members are reconstructed.
// ----------------------------------------------------------------------------
static inline bool CsmHasValidLastPlayerCar(
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* /*lpInterface*/)
{
    // X360: mePlayerActiveRaceCarIndex (interface +0x2858) != -1 && per-car valid byte (+0x2860) != 0.
    return false;   // FLAG: interface validity sub-offsets not modelled -> X360 "no valid car" path.
}

static inline bool CsmIsSecondSpawnLocationCloser(
    const BrnTrigger::SpawnLocation* /*lpSlot0*/,
    const BrnTrigger::SpawnLocation* /*lpSlot4*/,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* /*lpInterface*/)
{
    // X360: dist2(playerPos, slot4Pos) compared (vcmpgtfp) against slot0Pos.w -> true == slot4 nearer.
    return false;   // FLAG: position sub-offsets not modelled -> never swap (keep discovery order).
}

// ============================================================================
// GetProfileCarData (X360 0x82365760).
// Resolve the player Profile's CarData record whose car id == lrCarID, or NULL on miss. Used by the
// unlock-display / change-car / exit / spawn paths to fetch the deform amount + id of the car being
// shown or spawned. (lrCarID is taken by reference to mirror the DWARF signature; only its value is
// read here.)
// ============================================================================
const BrnProgression::CarData* CarSelectManager::GetProfileCarData(CgsID& lrCarID) const
{
    if (mpProgressionManager.Get() == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpProgressionManager", KAC_CSM_FILE, 1341);
        CgsDev::Assert::EndAssert();
    }

    BrnProgression::Profile* lpProfile = mpProgressionManager.Get()->GetProfile();
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProfile", KAC_CSM_FILE, 1347);
        CgsDev::Assert::EndAssert();
    }

    const BrnProgression::CarData* lpCarData = 0;
    const s32 liCarCount = lpProfile->GetCarCount();
    for (s32 li = 0; li < liCarCount; ++li)
    {
        const BrnProgression::CarData* lpCandidate = lpProfile->GetCarData(li);
        if (lpCandidate->GetId() == lrCarID)
        {
            lpCarData = lpCandidate;
            break;
        }
    }

    if (lpCarData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpCarData", KAC_CSM_FILE, 1351);
        CgsDev::Assert::EndAssert();
    }
    return lpCarData;
}

// ============================================================================
// IsThisCarInCurrentUnlockSequence (X360 0x823799E8).
// Predicate used while building the unlock list (and walking GetNextUnlockCarID): does this profile
// car belong in the set of cars whose unlock is being shown right now?
//
// CarData fields the X360 reads:
//   +0x0A  bool  -- a per-car "hidden / already-shown" flag (true => excluded from BOTH paths).
//   +0x10  s32   -- the car's UNLOCK-TYPE enumerator (1/2/3 == various trophy/event unlocks, 5 == a
//                   rank-gated unlock, etc.).
// Two modes, selected by mbShutdownUnlockSequence (X360 *(this+0x74)):
//   * shutdown-unlock list (mbShutdownUnlockSequence != 0): in iff unlock-type == 3.
//   * normal list (mbShutdownUnlockSequence == 0):
//       - unlock-type == 3  -> NOT in this list (it belongs to the shutdown list).
//       - unlock-type == 5  -> in iff the player's progression rank >= the car's required rank
//                              (read from the VehicleListEntry gameplay data, +0x99).
//       - any other type    -> in.
// The hidden-flag at +0x0A short-circuits everything to "false".
// ============================================================================
bool CarSelectManager::IsThisCarInCurrentUnlockSequence(const BrnProgression::CarData* lpProfileCar) const
{
    if (lpProfileCar->WasUnlockSequenceAlreadyShown())   // X360 *(lpProfileCar + 0x0A) -- FLAG: CarData +0x0A bool
        return false;

    const s32 leUnlockType = lpProfileCar->GetUnlockType();   // X360 *(lpProfileCar + 0x10) -- FLAG: CarData +0x10 s32

    if (mbShutdownUnlockSequence)   // X360 *(this + 0x74) == mbShutdownUnlockSequence (the +0x70 select)
        return leUnlockType == 3;

    if (leUnlockType == 5)
    {
        // Rank-gated unlock: in iff the player's progression rank has reached the car's requirement.
        const s32 liVehicleIndex = mpVehicleList.Get()->GetVehicleIndex(lpProfileCar->GetId());
        const BrnResource::VehicleListEntry* lpEntry =
            (liVehicleIndex < 0) ? 0 : mpVehicleList.Get()->GetVehicleData(liVehicleIndex);
        if (lpEntry == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpVehicleListEntry != NULL", KAC_CSM_FILE, 1169);
            CgsDev::Assert::EndAssert();
        }
        // X360 takes &entry->mGamePlayData (entry + 0x90) and asserts it non-NULL (it never is for a
        // by-value sub-object, but the baked assert is reproduced for fidelity), then reads the
        // required-rank byte at +0x09 of that sub-object.
        if (lpEntry == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpVehicleListEntryGamePlayData != NULL", KAC_CSM_FILE, 1172);
            CgsDev::Assert::EndAssert();
        }
        const s32 liRequiredRank = lpEntry->GetUnlockRank();   // X360 *(entry + 0x90 + 0x09) -- FLAG
        if (mpProgressionManager.Get()->GetProgressionRank() >= liRequiredRank)
            return true;
        return false;
    }
    else
    {
        // Every non-rank-gated unlock is in the normal list -- except the shutdown-only type 3.
        if (leUnlockType != 3)
            return true;
        return false;
    }
}

// ============================================================================
// SetupNormalUnlockList (X360 0x82379AD8).
// Recount how many profile cars belong in the NORMAL unlock-display sequence: clear the count + the
// shutdown-select flag, then (offline only, and only when unlock is enabled) walk every profile car
// and tally the ones IsThisCarInCurrentUnlockSequence accepts. Finally latch mbNoNormalUnlockCars =
// (count == 0). muUnlockCount drives the transition-in min-time + the unlock-state branch.
// ============================================================================
void CarSelectManager::SetupNormalUnlockList()
{
    if (mpProgressionManager.Get() == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpProgressionManager", KAC_CSM_FILE, 1199);
        CgsDev::Assert::EndAssert();
    }
    BrnProgression::Profile* lpProfile = mpProgressionManager.Get()->GetProfile();
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProfile", KAC_CSM_FILE, 1203);
        CgsDev::Assert::EndAssert();
    }

    muUnlockCount            = 0;   // X360 *(this + 0x5C) = 0
    mbShutdownUnlockSequence = false;   // X360 *(this + 0x74) = 0 (select the normal predicate)

    // Offline only, and only when car-unlock is enabled (X360 *(this + 0x75) == mbCarUnlockEnabled).
    if (!mpGameStateModule.Get()->IsOnlineGameMode() && mbCarUnlockEnabled)
    {
        const s32 liCarCount = lpProfile->GetCarCount();
        for (s32 li = 0; li < liCarCount; ++li)
        {
            const BrnProgression::CarData* lpProfileCar = lpProfile->GetCarData(li);
            if (lpProfileCar == 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpProfileCar", KAC_CSM_FILE, 1217);
                CgsDev::Assert::EndAssert();
            }
            if (IsThisCarInCurrentUnlockSequence(lpProfileCar))
                ++muUnlockCount;
        }
    }

    mbNoNormalUnlockCars = (muUnlockCount == 0);   // X360 *(this + 0x70) = (count == 0)
}

// ============================================================================
// SetupShutdownUnlockList (X360 0x82379C08).
// The "Burning Route / shutdown rival" variant: arm the shutdown-select flag, recount how many
// profile cars are in the shutdown sequence (offline + unlock-enabled), leaving muUnlockCount set.
// Unlike the normal list it does NOT touch mbNoNormalUnlockCars (it only sets the +0x70 select to 1
// up front). Used when entering the junkyard after taking down a rival who drops a car.
// ============================================================================
void CarSelectManager::SetupShutdownUnlockList()
{
    if (mpProgressionManager.Get() == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpProgressionManager", KAC_CSM_FILE, 1239);
        CgsDev::Assert::EndAssert();
    }
    BrnProgression::Profile* lpProfile = mpProgressionManager.Get()->GetProfile();
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProfile", KAC_CSM_FILE, 1243);
        CgsDev::Assert::EndAssert();
    }

    // X360 0x82379C08 writes BOTH bytes: *(this + 0x70) = 1 (mbNoNormalUnlockCars -- the "this junkyard
    // entry should run a shutdown-rival unlock display" select StartTransitionInState/StartUnlockState
    // read) AND *(this + 0x74) = 1 (mbShutdownUnlockSequence -- the IsThisCarInCurrentUnlockSequence
    // shutdown-predicate select). They are set together here but read at different points, so they are
    // kept DISTINCT (folding them is unsafe: SetupNormalUnlockList clears only 0x74 while leaving 0x70 live).
    mbNoNormalUnlockCars     = true;    // X360 *(this + 0x70) = 1
    mbShutdownUnlockSequence = true;    // X360 *(this + 0x74) = 1
    muUnlockCount            = 0;       // X360 *(this + 0x5C) = 0

    if (!mpGameStateModule.Get()->IsOnlineGameMode() && mbCarUnlockEnabled)
    {
        const s32 liCarCount = lpProfile->GetCarCount();
        for (s32 li = 0; li < liCarCount; ++li)
        {
            const BrnProgression::CarData* lpProfileCar = lpProfile->GetCarData(li);
            if (lpProfileCar == 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpProfileCar", KAC_CSM_FILE, 1258);
                CgsDev::Assert::EndAssert();
            }
            if (IsThisCarInCurrentUnlockSequence(lpProfileCar))
                ++muUnlockCount;
        }
    }
}

// ============================================================================
// GetNextUnlockCarID (X360 0x82379D28).
// Given the car id currently being shown (lCurrentID, or kCGSID_NULL to start), return the id of the
// NEXT profile car in the unlock sequence. Finds lCurrentID's index in the profile car array (asserts
// it is present when non-null), then scans forward from index+1 for the first car
// IsThisCarInCurrentUnlockSequence accepts. Asserts "Can't find next unlock!" + returns kCGSID_NULL if
// none remains.
// ============================================================================
CgsID CarSelectManager::GetNextUnlockCarID(CgsID lCurrentID)
{
    BrnProgression::Profile* lpProfile = mpProgressionManager.Get()->GetProfile();
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProfile", KAC_CSM_FILE, 1372);
        CgsDev::Assert::EndAssert();
    }

    s32 liCurrentIndex = -1;

    if (lCurrentID != 0)
    {
        s32 liCarCount = lpProfile->GetCarCount();
        s32 liCarIndex = 0;
        if (liCarCount > 0)
        {
            for (;;)
            {
                if (liCarIndex < 0 || liCarIndex >= liCarCount)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "liCarIndex >= 0 && liCarIndex < miCarCount", KAC_BRNPROFILE_FILE, 1923);
                    CgsDev::Assert::EndAssert();
                }
                if (lpProfile->GetCarData(liCarIndex)->GetId() == lCurrentID)
                {
                    liCurrentIndex = liCarIndex;
                    break;
                }
                liCarCount = lpProfile->GetCarCount();
                ++liCarIndex;
                if (liCarIndex >= liCarCount)
                    break;
            }
        }
        if (liCurrentIndex < 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("liCurrentIndex >=0", KAC_CSM_FILE, 1386);
            CgsDev::Assert::EndAssert();
        }
    }

    s32 liCarCount = lpProfile->GetCarCount();
    s32 liNextIndex = liCurrentIndex + 1;
    if (liNextIndex >= liCarCount)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Can't find next unlock!\n", KAC_CSM_FILE, 1401);
        CgsDev::Assert::EndAssert();
        return 0;
    }

    for (;;)
    {
        if (liNextIndex < 0 || liNextIndex >= liCarCount)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "liCarIndex >= 0 && liCarIndex < miCarCount", KAC_BRNPROFILE_FILE, 1923);
            CgsDev::Assert::EndAssert();
        }
        const BrnProgression::CarData* lpProfileCar = lpProfile->GetCarData(liNextIndex);
        if (lpProfileCar == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpProfileCar", KAC_CSM_FILE, 1393);
            CgsDev::Assert::EndAssert();
        }
        if (IsThisCarInCurrentUnlockSequence(lpProfileCar))
            return lpProfileCar->GetId();

        liCarCount = lpProfile->GetCarCount();
        ++liNextIndex;
        if (liNextIndex >= liCarCount)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Can't find next unlock!\n", KAC_CSM_FILE, 1401);
            CgsDev::Assert::EndAssert();
            return 0;
        }
    }
}

// ============================================================================
// RequestChangeCar (X360 0x82387990).
// Public entry: the player asked to switch to lCarId. Asserts a non-null id. If a car change is
// already in flight (state 7/8), it just stashes lCarId in mCacheDuringChangeCarId (it will be picked
// up when the in-flight swap finishes). Otherwise it records the chosen livery for the new car (from
// the live selection when in the car-mod screen, else from the saved per-car choice), stores lCarId
// as mDesiredCarId, resets the timer, and enters E_STATE_REQUEST_CAR_CHANGE so Update can drive the
// teleport/swap on the next ticks.
// ============================================================================
void CarSelectManager::RequestChangeCar(const CgsID& lCarId)
{
    if (lCarId == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lCardId != kCGSID_NULL", KAC_CSM_FILE, 569);
        CgsDev::Assert::EndAssert();
    }

    if (meState == E_STATE_CHANGING_CAR || meState == E_STATE_STARTING_CHANGING_CAR)
    {
        // A swap is mid-flight; cache the request for when it completes.
        mCacheDuringChangeCarId = lCarId;
        return;
    }

    BrnProgression::Profile* lpProfile = mpProgressionManager.Get()->GetProfile();
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProfile", KAC_CSM_FILE, 580);
        CgsDev::Assert::EndAssert();
    }

    if (mbInCarModScreen)   // X360 *(this + 0x73) == mbInCarModScreen
    {
        // In the car-mod screen the player picked a livery live -- persist that choice for lCarId.
        SaveChosenLiveryForCar(lCarId);
        mDesiredCarId = lCarId;
    }
    else
    {
        // Otherwise look up the livery already chosen for this base car.
        mDesiredCarId = lpProfile->GetChosenLiveryIdForBaseCar(lCarId);
    }

    mfStateTimer = 0.0f;
    meState      = E_STATE_REQUEST_CAR_CHANGE;

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        // X360 also streams the chosen car id via a CgsID-to-stream helper (sub_82203EE8); that
        // formatting helper is not modelled, so only the literal prefix is reproduced.
        *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: RequestChangeCar, CarId: ";
    }
}

// ============================================================================
// RequestStreamingForUnlock (X360 0x82387E78).
// Kick the world to start streaming the car(s) involved in the current unlock step, then latch
// mbWaitingForStreaming so the state machine stalls until StreamingFinished clears it.
//   * muUnlockCount == 0 -> nothing to stream (returns without arming the wait).
//   * muUnlockCount == 1 -> stream just the current unlock car (one CarUnlockEndAction entry).
//   * muUnlockCount >  1 -> stream the current unlock car AND prefetch the NEXT one (two entries; the
//                          second entry's "is-stream-target" flag layout differs for the prefetch).
// The action posted is CarUnlockEndAction (type 69 == 0x45, size 88 == 0x58).
// ============================================================================
void CarSelectManager::RequestStreamingForUnlock(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    if (muUnlockCount == 0)
        return;

    // CarUnlockEndAction payload (88B). The X360 builds two {CgsID id} slots (base+0x00 / base+0x08),
    // a count word (base+0x40 == 1 or 2), and four flag bytes interleaved at base+0x44/+0x4C (entry[0])
    // and base+0x45/+0x4D (entry[1]). Only those seeded fields are reproduced; the rest of the action
    // layout is not in the exports (FLAG). Stack offsets recovered from the X360 frame:
    //   v8[0]=var_90(+0x00) v8[1]=var_88(+0x08) count=var_50(+0x40)
    //   v10=var_4C(+0x44) v12=var_44(+0x4C) v11=var_4B(+0x45) v13=var_43(+0x4D)
    u8 lacUnlock[88];
    std::memset(lacUnlock, 0, sizeof(lacUnlock));

    s32 liEntryCount = 1;
    const CgsID lCurrent = mCurrentCarToUnlock;
    std::memcpy(lacUnlock + 0x00, &lCurrent, sizeof(lCurrent));       // entry[0].id == current unlock car

    lacUnlock[0x44] = 4;   // entry[0] stream-type sentinel (X360 v10 == 4)
    lacUnlock[0x4C] = 0;   // entry[0] trailing flag        (X360 v12 == 0)

    if (muUnlockCount != 1)
    {
        // More than one unlock car remains -> also prefetch the next one.
        const CgsID lNext = GetNextUnlockCarID(mCurrentCarToUnlock);
        std::memcpy(lacUnlock + 0x08, &lNext, sizeof(lNext));        // entry[1].id == next unlock car
        if (lNext != 0)
        {
            liEntryCount = 2;
            // The prefetch entry's stream/active flags depend on whether we are still in TRANSITION_IN
            // (state 1) -- the X360 sets two booleans (v11/v13) differently in each case.
            if (meState != E_STATE_TRANSITION_IN)
            {
                lacUnlock[0x45] = 4;   // entry[1] stream-type (X360 v11 == 4)
                lacUnlock[0x4D] = 0;   // entry[1] flag        (X360 v13 == 0)
            }
            else
            {
                lacUnlock[0x45] = 0;   // entry[1] stream-type (X360 v11 == 0)
                lacUnlock[0x4D] = 1;   // entry[1] flag        (X360 v13 == 1)
            }
        }
    }

    std::memcpy(lacUnlock + 0x40, &liEntryCount, sizeof(liEntryCount));   // count word (1 or 2)

    AsActionQueue(lpActionQueue)->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(lacUnlock), 69 /*KI_ACTION_CAR_UNLOCK_STREAM*/, 88);

    mbWaitingForStreaming = true;   // X360 *(this + 0x58) = 1
}

// ============================================================================
// StreamingFinished (X360 0x82387210).
// Streaming-complete callback (the world finished streaming the car we asked for). Only acts when the
// completed car id (lActiveCarZeroId) matches the car we were waiting on (mDesiredCarId), or when no
// car was pending. Clears mbWaitingForStreaming. If we were EXITING and have held the exit long enough
// (timer >= 2.0), posts the CarSelectExitAction (type 74) to actually leave the junkyard.
// ============================================================================
void CarSelectManager::StreamingFinished(CgsID lActiveCarZeroId, GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    const CgsID lDesired = mDesiredCarId;   // X360 *(this + 0x48) [the +0x48 std the change path writes]
    if (lActiveCarZeroId == lDesired || lDesired == 0)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: StreamingFinished\n";

        const bool lbWasExiting = (meState == E_STATE_EXITING);
        mbWaitingForStreaming = false;   // X360 *(this + 0x58) = 0

        if (lbWasExiting && mfStateTimer >= 2.0f /*flt_82001D9C*/)
        {
            u8 lacExit[1] = { 0 };
            AsActionQueue(lpActionQueue)->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(lacExit), 74 /*KI_ACTION_CAR_SELECT_EXIT*/, 1);
        }
    }
}

// ============================================================================
// SpawnInStartCar (X360 0x82387CD8).
// Place the player's start car at the chosen "drive-in" spawn location when the junkyard transition
// begins. Asserts maSpawnLocations[0] is set (the X360 reads the +0x2C slot == maSpawnLocations[1] in
// the asm: this+0x2C; see note), marks streaming pending, resolves the start car's CarData (for the
// deform amount), then posts three game actions:
//   - ResetPlayerCarAction      (type 0,  80B): the start car id + deform + in-car-select flags.
//   - CarSelectionChangedAction (type 64, 64B): the start car id + spawn-location transform + type bit.
//   - CarSelectionDropInAction  (type 65, 16B): the start car id + spawn-location type bit.
// The spawn-location transform is read from the chosen SpawnLocation; its type bit (a single bool ==
// "is this a generic / drive-in spawn") is derived from SpawnLocation::GetType().
// ============================================================================
void CarSelectManager::SpawnInStartCar(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    // X360 reads the spawn-location handle at this+0x2C == maSpawnLocations[1] (byte 0x28 + 4). The
    // frozen layout summary maps byte 0x2C to maSpawnLocations[1]; that is the "drive-in" slot the
    // start car is placed at.
    const BrnTrigger::SpawnLocation* lpSpawnLocation = maSpawnLocations[1].Get();
    if (lpSpawnLocation == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpSpawnLocation", KAC_CSM_FILE, 1281);
        CgsDev::Assert::EndAssert();
    }

    // ⛔⛔ CORRECTED 2026-08-01 (car-select hand-off wave) -- THIS LINE USED TO READ
    //     mbWaitingForStreaming = true;
    // behind a FLAG that admitted the console's store is at +0x3C and then bound it to the
    // member at +0x58 anyway ("behaviourally this arms the spawn-pending latch"). It does not.
    // The console instruction is
    //     0x82387D18  li  r27, 1
    //     0x82387D28  stw r27, 0x3C(r30)          ; a 4-BYTE store at CarSelectManager + 0x3C
    // and +0x3C is meLastSpawnLocationType -- pinned by this very function's FIRST instruction,
    // `lwz r29, 0x2C(r30)` == maSpawnLocations[1], because the 5-pointer array runs
    // 0x28..0x3B and +0x3C is the next member. mbWaitingForStreaming is a BOOL at +0x58; a
    // `stw` cannot be writing it.
    //
    // ⚠️ IT WAS NOT A HARMLESS RENAME. mbWaitingForStreaming is the gate on
    // CarSelectManager::Update's TRANSITION_IN arm (`lfTimer >= lfMinTime &&
    // !mbWaitingForStreaming`), and the ONLY thing that clears it is StreamingFinished, whose
    // console caller (GameStateModule::ProcessStreamingCompleteEvent @0x8239xxxx) is not
    // reconstructed. So the start-of-game junkyard entry armed a latch nothing could ever
    // clear and the transition-in NEVER ENDED: measured on this build, meState sat at
    // E_STATE_TRANSITION_IN with mfStateTimer climbing past 18 s, EndTransitionInState never
    // ran, its action 73 {0,hasCars} was never posted, and GameState::meJunkyardState stayed
    // at E_JY_INTRO_NO_CARS for the whole session.
    meLastSpawnLocationType = 1;   // X360 stw 1, 0x3C(this)

    const BrnProgression::CarData* lpCarData = GetProfileCarData(mStartCarId);
    const f32 lfDeform = (lpCarData != 0) ? lpCarData->GetUnlockDeformationAmount() : 0.0f;

    // The spawn-location type bit: true iff GetType() != E_TYPE_NONE/0 (a non-default drive-in slot).
    const u32 luSpawnType = static_cast<u32>(lpSpawnLocation->GetType());
    if (luSpawnType >= 4u)   // E_TYPE_COUNT == 4
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("muType < E_TYPE_COUNT", KAC_SPAWNLOC_FILE, 152);
        CgsDev::Assert::EndAssert();
    }
    // ⛔ CORRECTED 2026-08-01 -- the committed line was `(luSpawnType != 0) ? 0 : 1`, described as
    // "1 iff type==0". It is 1 iff type == 1. The X360 idiom (0x82387DF0..0x82387E14) is
    //     r11 = cntlzw(type - 1);  bit = extrwi(r11, 1, 26)      // == bit 5 of the count
    // cntlzw(0) == 32 (0b100000, bit 5 set -> 1); cntlzw(-1) == 0; cntlzw(1) == 31 (0b011111 -> 0).
    // So only type - 1 == 0, i.e. type == E_TYPE_CAR_SELECT_LEFT (1), yields 1 -- which is exactly
    // what the consumer calls it: MainDirector::ProcessInputQueue case 64 stores it into
    // maGameState.mbJunkyardPosIsLeft. The old form was inverted AND tested the wrong enumerator.
    // (CarSelectManager::EndUnlockState in the sibling TU already had `(luSpawnType == 1)`, so the
    // tree carried both spellings of the same bit.)
    const u8 lbSpawnTypeBit = (luSpawnType == 1u) ? 1 : 0;

    // --- ResetPlayerCarAction (type 0, 80B) ---
    // ⛔⛔ CORRECTED 2026-08-01 -- THIS PAYLOAD WAS BUILT AT THE WRONG OFFSETS, ALL OF THEM, and
    // the spawn transform (the part that actually PUTS THE CAR IN THE JUNKYARD) was dismissed as
    // "opaque". It is not opaque: it is SpawnLocation::mPosition / ::mDirection, already named in
    // BrnSpawnLocation.h at exactly the +0x00/+0x10 the X360 lvx128's from.
    //
    // Recovered field by field from 0x82387D38..0x82387D98 (payload base == stack var_90, size 0x50;
    // each var_XX below is var_90 + (0x90 - XX)):
    //   var_90 +0x00  stvx128 v0  <- lvx128(r29+0x00)   SpawnLocation::mPosition   (16B)
    //   var_80 +0x10  stvx128 v0  <- lvx128(r29+0x10)   SpawnLocation::mDirection  (16B)
    //   var_70 +0x20  std r10     <- *(this+0x40)       mStartCarId                (8B)
    //   var_68 +0x28  std r31     <- 0                                             (8B)
    //   var_60 +0x30  stw r10     <- 8                  (s32 -- the car-select type)
    //   var_5C +0x34  stfs f0     <- CarData+0x0C       deform amount              (f32)
    //   var_58 +0x38  stw r27     <- 1                  (s32 -- in-car-select)
    //   var_54 +0x3C  stw r31     <- 0                  (s32 -- the in-car-MOD bit; 0 on this path)
    //   var_50 +0x40  stb r27     <- 1
    //   var_4F +0x41  stb r31     <- 0     (TeleportCurrentVehicle writes 1 here; this path 0)
    //   var_4E +0x42  stb r31     <- 0
    //   var_4D +0x43  stb r31     <- 0
    //
    // ⚠️ NOTHING ON PC CONSUMES ACTION 0 YET: there is no BridgeGameStateToWorld, so this record is
    // built correctly and then goes nowhere. Say it plainly -- fixing the offsets does NOT by itself
    // move a car. It stops the record being wrong when the world bridge lands. (Action 64 below IS
    // consumed, by MainDirector::ProcessInputQueue -- see the correction there.)
    {
        u8 lacReset[80];
        std::memset(lacReset, 0, sizeof(lacReset));
        const CgsID lStartCar     = mStartCarId;
        const s32   liCarSelType  = 8;
        const s32   liInCarSelect = 1;
        const s32   liInCarMod    = 0;
        std::memcpy(lacReset + 0x00, &lpSpawnLocation->mPosition,  sizeof(Vector3));
        std::memcpy(lacReset + 0x10, &lpSpawnLocation->mDirection, sizeof(Vector3));
        std::memcpy(lacReset + 0x20, &lStartCar,     sizeof(lStartCar));
        std::memcpy(lacReset + 0x30, &liCarSelType,  sizeof(liCarSelType));
        std::memcpy(lacReset + 0x34, &lfDeform,      sizeof(lfDeform));
        std::memcpy(lacReset + 0x38, &liInCarSelect, sizeof(liInCarSelect));
        std::memcpy(lacReset + 0x3C, &liInCarMod,    sizeof(liInCarMod));
        lacReset[0x40] = 1;
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacReset), 0 /*KI_ACTION_RESET_PLAYER_CAR*/, 80);
    }

    // --- CarSelectionChangedAction (type 64, 64B) ---
    // ⛔⛔ CORRECTED 2026-08-01 -- the spawn-type bit was written at +0x10 and
    // MainDirector::ProcessInputQueue case 64 reads it at +0x30 (BrnMainDirector.cpp:753,
    // `mbJunkyardPosIsLeft = (lpacPayload[0x30] != 0)`). BOTH ENDS ARE IN THIS TREE AND THEY
    // DISAGREED: the producer was stamping the bit on top of what the console puts the spawn
    // POSITION in, and the consumer was reading a byte nobody wrote. Recovered from
    // 0x82387D9C..0x82387E18 (payload base == var_90, size 0x40):
    //   +0x00  std r10      <- *(this+0x20)      mJunkyardId              (8B)
    //   +0x10  stvx128 v0   <- SpawnLocation::mPosition                   (16B)
    //   +0x20  stvx128 v13  <- SpawnLocation::mDirection                  (16B)
    //   +0x30  stb r11      <- the spawn-location type bit
    //   +0x31  stb r31      <- 0
    {
        u8 lacChanged[64];
        std::memset(lacChanged, 0, sizeof(lacChanged));
        const CgsID lJunkyard = mJunkyardId;
        std::memcpy(lacChanged + 0x00, &lJunkyard,                 sizeof(lJunkyard));
        std::memcpy(lacChanged + 0x10, &lpSpawnLocation->mPosition,  sizeof(Vector3));
        std::memcpy(lacChanged + 0x20, &lpSpawnLocation->mDirection, sizeof(Vector3));
        lacChanged[0x30] = lbSpawnTypeBit;
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacChanged), 64 /*KI_ACTION_CAR_SELECTION_CHANGED*/, 64);
    }

    // --- CarSelectionDropInAction (type 65, 16B) ---
    {
        u8 lacDropIn[16];
        std::memset(lacDropIn, 0, sizeof(lacDropIn));
        const CgsID lStartCar = mStartCarId;
        std::memcpy(lacDropIn, &lStartCar, sizeof(lStartCar));         // X360 v18 = *(this+0x40) (start car id)
        lacDropIn[0x08] = lbSpawnTypeBit;    // spawn-location type bit
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacDropIn), 65 /*KI_ACTION_CAR_SELECTION_DROPIN*/, 16);
    }
}

// ============================================================================
// TeleportCurrentVehicle (X360 0x82392EF0).
// Move the player's car to the spawn location for the FINISHED swap. Indexes the spawn-location array
// by meLastSpawnLocationType (this+0x3C; maSpawnLocations[type]) -- the slot the previous flow recorded
// -- and asserts it is set. Resolves the desired car's CarData (deform). Posts three actions:
//   - ResetPlayerCarAction      (type 0,  80B): desired car id + deform + the in-car-mod flag.
//   - CarSelectionDropInAction  (type 65, 16B): desired car id + spawn-location type bit.
//   - CarUnlockAction           (type 79,  8B): the desired car's colour + palette indices.
// ============================================================================
void CarSelectManager::TeleportCurrentVehicle(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    // X360 indexes maSpawnLocations[meLastSpawnLocationType] (this + 4*(type+10)).
    const s32 liSpawnIndex = meLastSpawnLocationType;
    const BrnTrigger::SpawnLocation* lpSpawnLocation =
        (liSpawnIndex >= 0 && liSpawnIndex < static_cast<s32>(KU_CARSELECT_SPAWNLOCATION_COUNT))
            ? maSpawnLocations[liSpawnIndex].Get()
            : 0;
    if (lpSpawnLocation == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpSpawnLocation", KAC_CSM_FILE, 1473);
        CgsDev::Assert::EndAssert();
    }

    const BrnProgression::CarData* lpCarData = GetProfileCarData(mDesiredCarId);
    if (lpCarData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpCarData", KAC_CSM_FILE, 1478);
        CgsDev::Assert::EndAssert();
    }
    const f32 lfDeform = lpCarData->GetUnlockDeformationAmount();   // CarData +0x0C

    const u32 luSpawnType = static_cast<u32>(lpSpawnLocation->GetType());
    if (luSpawnType >= 4u)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("muType < E_TYPE_COUNT", KAC_SPAWNLOC_FILE, 152);
        CgsDev::Assert::EndAssert();
    }
    // ⛔ CORRECTED 2026-08-01 -- same inverted/wrong-enumerator bit as SpawnInStartCar; see the
    // full derivation there. 1 iff type == E_TYPE_CAR_SELECT_LEFT.
    const u8 lbSpawnTypeBit = (luSpawnType == 1u) ? 1 : 0;

    // --- ResetPlayerCarAction (type 0, 80B) ---
    // ⛔ CORRECTED 2026-08-01, same defect as SpawnInStartCar's copy: every field was at the wrong
    // offset and the spawn transform was missing. Recovered from 0x82392F78..0x82392FEC (payload
    // base == var_90, size 0x50). It is the IDENTICAL record shape SpawnInStartCar builds, with
    // exactly two differences, both marked below:
    //   +0x00  stvx128 v0  <- lvx128(r31+0x00)   SpawnLocation::mPosition   (16B)
    //   +0x10  stvx128 v0  <- lvx128(r31+0x10)   SpawnLocation::mDirection  (16B)
    //   +0x20  std r10     <- *(this+0x48)       mDesiredCarId              (8B)   [DIFFERENT id]
    //   +0x28  std r11     <- 0                                             (8B)
    //   +0x30  stw r11     <- 8                  (s32)
    //   +0x34  stfs f0     <- CarData+0x0C       deform amount              (f32)
    //   +0x38  stw r11     <- 1                  (s32)
    //   +0x3C  stw r10     <- extrwi(cntlzw(*(this+0x73)),1,26)             (s32)  [DIFFERENT: the
    //          in-car-MOD bit. cntlzw(0)==32 -> bit 26 set -> 1; cntlzw(1)==31 -> 0. So the value is
    //          `mbInCarModScreen == 0`, which the old body had RIGHT -- at the wrong offset (+0x1C)
    //          and the wrong width (u8 vs s32).]
    //   +0x40  stb r11 <- 1     +0x41 stb r11 <- 1     +0x42 <- 0     +0x43 <- 0
    {
        u8 lacReset[80];
        std::memset(lacReset, 0, sizeof(lacReset));
        const CgsID lDesired      = mDesiredCarId;
        const s32   liCarSelType  = 8;
        const s32   liInCarSelect = 1;
        const s32   liNotInCarMod = mbInCarModScreen ? 0 : 1;
        std::memcpy(lacReset + 0x00, &lpSpawnLocation->mPosition,  sizeof(Vector3));
        std::memcpy(lacReset + 0x10, &lpSpawnLocation->mDirection, sizeof(Vector3));
        std::memcpy(lacReset + 0x20, &lDesired,      sizeof(lDesired));
        std::memcpy(lacReset + 0x30, &liCarSelType,  sizeof(liCarSelType));
        std::memcpy(lacReset + 0x34, &lfDeform,      sizeof(lfDeform));
        std::memcpy(lacReset + 0x38, &liInCarSelect, sizeof(liInCarSelect));
        std::memcpy(lacReset + 0x3C, &liNotInCarMod, sizeof(liNotInCarMod));
        lacReset[0x40] = 1;
        lacReset[0x41] = 1;
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacReset), 0 /*KI_ACTION_RESET_PLAYER_CAR*/, 80);
    }

    // --- CarSelectionDropInAction (type 65, 16B) ---
    {
        u8 lacDropIn[16];
        std::memset(lacDropIn, 0, sizeof(lacDropIn));
        const CgsID lDesired = mDesiredCarId;
        std::memcpy(lacDropIn, &lDesired, sizeof(lDesired));        // X360 v14 = *(this+0x48) (desired car id)
        lacDropIn[0x08] = lbSpawnTypeBit;
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacDropIn), 65 /*KI_ACTION_CAR_SELECTION_DROPIN*/, 16);
    }

    // --- CarSelectChangeColourAction (type 79, 8B): the desired car's palette + colour ---
    // X360 0x82392EF0: `GetCarColourAndPalette(.., &v14 + 4, &v14)` then `AddEvent(&v14, 79, 8)`
    // -- the COLOUR lands at payload+4 and the PALETTE at payload+0.
    {
        GameStateModuleIO::CarSelectChangeColourAction lColour;
        s32 liColour  = 0;
        s32 liPalette = 0;
        // X360 reads *(this + 0x4C) == the cache-during-change car id (mCacheDuringChangeCarId region)
        // for the colour query; behaviourally it is the car just teleported in. Use mDesiredCarId,
        // which is the active car at this point of the swap.
        mpProgressionManager.Get()->GetCarColourAndPalette(mDesiredCarId, &liColour, &liPalette);
        lColour.muPaletteIndex = static_cast<u32>(liPalette);
        lColour.muColourIndex  = static_cast<u32>(liColour);
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lColour),
            GameStateModuleIO::E_ACTION_CAR_SELECT_CHANGE_COLOUR,
            static_cast<s32>(sizeof(lColour)));
    }
}

// ============================================================================
// SetupSpawnLocations (X360 0x8236EA98).
// Build the junkyard's spawn-location array from the loaded track TriggerData: clear all five slots,
// then for every SpawnLocation whose junkyard id matches mJunkyardId, file it by type:
//   * type 1..3 -> maSpawnLocations[type]  (asserts the slot is empty; one of each).
//   * type 0    -> the two "generic drive-in" slots: the first found goes to maSpawnLocations[0], the
//                  second to maSpawnLocations[4].
// Then, when the player has a valid last-active race car, it measures which of the two type-0 slots is
// nearer the car's last position and swaps them so maSpawnLocations[0] is the nearest (the player
// drives back in next to where they were). Finally asserts all five slots are filled.
// ============================================================================
void CarSelectManager::SetupSpawnLocations()
{
    if (mJunkyardId == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mJunkyardId != kCGSID_NULL", KAC_CSM_FILE, 1063);
        CgsDev::Assert::EndAssert();
    }
    if (mpTriggerQueryManager.Get() == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpTriggerQueryManager", KAC_CSM_FILE, 1064);
        CgsDev::Assert::EndAssert();
    }

    for (u32 lu = 0; lu < KU_CARSELECT_SPAWNLOCATION_COUNT; ++lu)
        maSpawnLocations[lu].Set(0);

    const BrnTrigger::TriggerData* lpTriggerData = mpTriggerQueryManager.Get()->GetTriggerData();
    if (lpTriggerData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpTriggerData", KAC_CSM_FILE, 1082);
        CgsDev::Assert::EndAssert();
    }

    const s32 liSpawnCount = lpTriggerData->GetSpawnLocationCount();
    for (s32 li = 0; li < liSpawnCount; ++li)
    {
        const BrnTrigger::SpawnLocation* lpSpawnLocation = lpTriggerData->GetSpawnLocation(li);
        if (lpSpawnLocation == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpSpawnLocation", KAC_CSM_FILE, 1088);
            CgsDev::Assert::EndAssert();
        }

        if (lpSpawnLocation->GetJunkyardId() == mJunkyardId)
        {
            const u32 luType = static_cast<u32>(lpSpawnLocation->GetType());
            if (luType >= 4u)   // E_TYPE_COUNT == 4
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("muType < E_TYPE_COUNT", KAC_SPAWNLOC_FILE, 152);
                CgsDev::Assert::EndAssert();
            }

            if (luType != 0)
            {
                // type 1..3 -> the matching dedicated slot (exactly one of each).
                const u32 luSlot = luType;
                if (maSpawnLocations[luSlot].Get() != 0)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "maSpawnLocations[lSpawnLocationType] == NULL", KAC_CSM_FILE, 1110);
                    CgsDev::Assert::EndAssert();
                }
                maSpawnLocations[luSlot].Set(lpSpawnLocation);
            }
            else if (maSpawnLocations[0].Get() != 0)
            {
                // second generic drive-in slot -> index 4.
                maSpawnLocations[4].Set(lpSpawnLocation);
            }
            else
            {
                // first generic drive-in slot -> index 0.
                maSpawnLocations[0].Set(lpSpawnLocation);
            }
        }
    }

    // --- Pick the nearer of the two generic drive-in slots (0 and 4) to the player's last car -------
    // FLAG: the X360 reaches the player's last race-car position through mpGameStateModule's cached
    // "last active race car interface" (this+0x39820) and its active-car index (+0x2858) / position
    // (sub_82310398). Those sub-offsets are NOT cleanly modelled on the minimal GameStateModule slice,
    // so the distance compare is reconstructed behaviourally and the deep reads are flagged.
    {
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpLastActiveCar =
            mpGameStateModule.Get()->GetLastActiveRaceCarInterface();
        if (lpLastActiveCar == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpLastActiveRaceCarInterface", KAC_CSM_FILE, 1119);
            CgsDev::Assert::EndAssert();
        }

        // X360 asserts mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT (8) and only does the
        // distance compare when the player has a valid active car (index != -1 && a per-car "valid"
        // byte set at interface +0x2860). When valid, it computes the squared distance from the
        // player's last position to maSpawnLocations[4]'s position and compares it against
        // maSpawnLocations[0]'s position's W component (vmsum3fp128 dot vs a splatted scalar); when
        // [4] is the nearer it swaps 0<->4 so maSpawnLocations[0] ends up the nearest drive-in slot.
        //
        // FLAG: the active-car validity byte, the player position, and the spawn-location position
        // vector all live behind interface / SpawnLocation sub-offsets that are NOT modelled on the
        // minimal slices in scope. The helper reproduces the *control flow* (only swap when the player
        // car is valid AND slot 4 is closer); its two opaque reads return the X360-default
        // "not-valid / not-closer" so the array keeps its discovery order (the X360 path taken when the
        // player has no valid last car). Replace with the real geometry when the RaceCar output
        // interface + SpawnLocation position members are reconstructed.
        if (CsmHasValidLastPlayerCar(lpLastActiveCar) &&
            CsmIsSecondSpawnLocationCloser(maSpawnLocations[0].Get(),
                                           maSpawnLocations[4].Get(),
                                           lpLastActiveCar))
        {
            const BrnTrigger::SpawnLocation* lpTmp = maSpawnLocations[0].Get();
            maSpawnLocations[0].Set(maSpawnLocations[4].Get());
            maSpawnLocations[4].Set(lpTmp);
        }
    }

    // --- All five slots must now be filled --------------------------------------------------------
    for (u32 lu = 0; lu < KU_CARSELECT_SPAWNLOCATION_COUNT; ++lu)
    {
        if (maSpawnLocations[lu].Get() == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "The junkyard does not have all of the spawn locations setup.", KAC_CSM_FILE, 1140);
            CgsDev::Assert::EndAssert();
        }
    }
}

// ============================================================================
// DEBUG_UnlockCarsForTesting (X360 0x82387F18).
// Cheat: force-add cars to the player profile for testing. Walks the whole VehicleList; for each
// vehicle that is a trophy/special car (VehicleListEntry +0x94 bit0) and that the profile does NOT yet
// own, it calls ProgressionManager::AddCar. Two passes, each capped at 3 newly-counted cars:
//   * Pass 1 == trophy pass (entered when mbDEBUG_UnlockTrophyCarsForTesting != 0): AddCar(id, 0), no
//     deform stamp.
//   * Pass 2 == shutdown pass (entered after the cap iff mbDEBUG_UnlockShutdownCarsForTesting != 0):
//     AddCar(id, 3) AND stamp a 0.85 deform onto the new CarData (CarData +0x0C) so it reads as "won".
// The X360 carries a single byte `v4` that is BOTH the pass selector and the AddCar-type selector:
//   v4 == 0  -> trophy pass     -> AddCar(id, 0), no deform.
//   v4 != 0  -> shutdown pass   -> AddCar(id, 3), 0.85 deform.
// v4 starts as (mbDEBUG_UnlockTrophyCarsForTesting == 0); when the cap is hit in the trophy pass it is
// reloaded from mbDEBUG_UnlockShutdownCarsForTesting (which, if armed, is non-zero -> the shutdown
// pass). An already-owned trophy car that is NOT hidden (CarData +0x0A == 0) still counts toward the
// cap without being re-added.
// ============================================================================
void CarSelectManager::DEBUG_UnlockCarsForTesting()
{
    BrnProgression::Profile* lpProfile = mpProgressionManager.Get()->GetProfile();
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProfile", KAC_CSM_FILE, 1681);
        CgsDev::Assert::EndAssert();
    }
    if (!mbDEBUG_UnlockTrophyCarsForTesting && !mbDEBUG_UnlockShutdownCarsForTesting)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "mbDEBUG_UnlockTrophyCarsForTesting || mbDEBUG_UnlockShutdownCarsForTesting",
            KAC_CSM_FILE, 1682);
        CgsDev::Assert::EndAssert();
    }

    // X360 v4: 0 == trophy pass (AddCar type 0), non-zero == shutdown pass (AddCar type 3 + deform).
    u8  lucPass = (mbDEBUG_UnlockTrophyCarsForTesting == 0) ? 1 : 0;
    u32 luAddedThisPass = 0;

    const s32 liVehicleCount = mpVehicleList.Get()->GetVehicleCount();
    for (s32 liVehicle = 0; liVehicle < liVehicleCount; ++liVehicle)
    {
        const BrnResource::VehicleListEntry* lpVehicleData = mpVehicleList.Get()->GetVehicleData(liVehicle);
        const CgsID lCarId = lpVehicleData->GetId();   // X360 *(VehicleData) (the leading car id)

        // Does the profile already own this car?
        const BrnProgression::CarData* lpOwned = 0;
        const s32 liCarCount = lpProfile->GetCarCount();
        for (s32 li = 0; li < liCarCount; ++li)
        {
            const BrnProgression::CarData* lpCandidate = lpProfile->GetCarData(li);
            if (lpCandidate->GetId() == lCarId)
            {
                lpOwned = lpCandidate;
                break;
            }
        }

        // Only trophy/special cars are eligible (the +0x94 bit0 flag).
        if (lpVehicleData->IsTrophyCar())
        {
            if (lpOwned == 0)
            {
                // Not owned -> add it. Pass selector chooses the unlock type + deform stamp.
                const s32 leUnlockType = (lucPass != 0) ? 3 : 0;
                BrnProgression::CarData* lpNewCar = mpProgressionManager.Get()->AddCar(lCarId, leUnlockType);
                if (lucPass != 0)
                    lpNewCar->SetUnlockDeformationAmount(KF_DEBUG_TROPHY_CAR_DEFORM);   // CarData +0x0C = 0.85
                ++luAddedThisPass;
            }
            else if (!lpOwned->WasUnlockSequenceAlreadyShown())   // X360 *(lpOwned + 0x0A) == 0 (not hidden)
            {
                // Already owned and visible -> still counts toward the cap, no re-add.
                ++luAddedThisPass;
            }
        }

        // Cap each pass at 3. When the trophy pass tops out, switch to the shutdown pass iff armed.
        if (luAddedThisPass >= 3)
        {
            if (lucPass != 0)
                return;   // shutdown pass done
            lucPass         = (mbDEBUG_UnlockShutdownCarsForTesting != 0) ? 1 : 0;
            luAddedThisPass = 0;
            if (lucPass == 0)
                return;   // no shutdown pass requested
        }
    }
}
}
