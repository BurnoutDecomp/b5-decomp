#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // GameStateModuleIO::EPlayerScoringIndex (committed home)

#include <cstdint>            // uintptr_t (Pointer32)

// Owning header for BrnGameState::CarSelectManager -- the junkyard car-select state machine (the
// player drove into a junkyard; this runs the car-swap / car-unlock-sequence / car-modification
// flow). DWARF: BrnCarSelectManager.h:59 (non-polymorphic struct). GROWN to the full DWARF member
// layout + all methods; bodies land across two .cpp TUs.
//
// PARITY-BY-NAMED-MEMBER (AGENTS.md): on the X360 every raw pointer is 4 bytes (Pointer32<T>) and
// CgsID is 8 bytes, so the console class is ~0x7C bytes; on the x64 gate the pointers are stored as
// Pointer32<T> (4-byte handle, the committed BrnGameStateFlybyManager precedent) so the member
// ORDER + the CgsID 8-byte spacing survive. We pin the structural facts that are ABI-stable with
// relative offsetof asserts in a never-called _AssertLayout() and do NOT bake absolute byte offsets.
//
// X360 member byte offsets (console, 4-byte ptrs) the bodies bind to, for reference:
//   +0x00 meState (s32)            +0x04 mfStateTimer (f32)
//   +0x08 mpGameStateModule        +0x0C mpTriggerQueryManager   +0x10 mpProgressionManager
//   +0x14 mpVehicleList            +0x18 mpWheelList
//   +0x20 mJunkyardId (CgsID 8B)
//   +0x28..+0x3B maSpawnLocations[5] (5 x 4B ptr)
//   +0x3C meLastSpawnLocationType (s32)
//   +0x40 mStartCarId  +0x48 mDesiredCarId  +0x50 mCacheDuringChangeCarId  (CgsID 8B each)
//   +0x58 mbWaitingForStreaming     +0x5C muUnlockCount (u32)
//   +0x60 mCurrentCarToUnlock (CgsID 8B)
//   +0x68 mbCurrentCarTickerVisible +0x6C mfCarUnlockFadedOutTargetTime (f32)
//   +0x70..+0x78 the nine trailing bool flags (see members below)

namespace BrnGameState     { class GameStateModule; class TriggerQueryManager; }
namespace BrnProgression   { class ProgressionManager; struct CarData; }
namespace BrnResource      { struct VehicleList; struct WheelList; }
namespace BrnTrigger       { struct SpawnLocation; }

namespace BrnGameState
{
// EnterJunkyardAtStartOfGame's DWARF signature names GameStateModuleIO::EPlayerScoringIndex (its
// committed home is BrnGameStateSharedIO.h, #included above -- do NOT redefine it here) and a
// CarSelectionChangedAction*. The action structs are GameStateModuleIO game-action payloads whose
// field layouts are NOT in this TU's exports; they are posted as opaque sized buffers (the
// DriveThruManager precedent). Forward-declare the two payload/param names so the signature type-checks.
namespace GameStateModuleIO
{
    struct CarSelectionChangedAction;                          // opaque game-action payload
    struct ControllerInput;                                    // Update() param (passed through; never deref'd here)
}

// X360 32-bit pointer storage (mirrors BrnGameStateFlybyManager.cpp).
template <typename T>
struct Pointer32
{
    u32 muAddress;
    void Set(T* lpPointer) { muAddress = static_cast<u32>(reinterpret_cast<uintptr_t>(lpPointer)); }
    T*   Get() const       { return reinterpret_cast<T*>(static_cast<uintptr_t>(muAddress)); }
};

class CarSelectManager
{
public:
    // DWARF BrnCarSelectManager.h:66.
    enum State
    {
        E_STATE_NONE                           = 0,
        E_STATE_TRANSITION_IN                  = 1,
        E_STATE_DISPLAY_UNLOCKED_CARS          = 2,
        E_STATE_DISPLAY_UNLOCKED_SHUTDOWN_CARS = 3,
        E_STATE_CAR_SELECT                     = 4,
        E_STATE_CAR_MODIFICATION               = 5,
        E_STATE_REQUEST_CAR_CHANGE             = 6,
        E_STATE_STARTING_CHANGING_CAR          = 7,
        E_STATE_CHANGING_CAR                   = 8,
        E_STATE_EXITING                        = 9,
    };

    static const u32 KU_CARSELECT_SPAWNLOCATION_COUNT = 5;   // DWARF :43
    static const u32 KU_CARSELECT_MAX_LIVERIES        = 5;   // DWARF :44
    // BrnTrigger::SpawnLocation::Type::E_TYPE_COUNT (the "no spawn location" sentinel == 4).
    static const s32 KI_SPAWNLOCATIONTYPE_NONE = 4;

    // ---- public API (DWARF :88-159) ----------------------------------------
    void Construct(const TriggerQueryManager* lpTriggerQueryManager,
                   GameStateModule* lpGameStateModule,
                   BrnProgression::ProgressionManager* lpProgressionManager);   // X360 0x823564D0
    bool IsInJunkyard() const;
    bool IsWaitingForStreaming() const;
    void Prepare(const BrnResource::VehicleList* lpVehicleList,
                 const BrnResource::WheelList* lpWheelList);
    void Update(GameStateModuleIO::GameActionQueue* lpActionQueue,
                const GameStateModuleIO::ControllerInput* lpControllerInput,
                f32 lfGameTimestep);                                            // X360 0x8239C218
    void EnterJunkyard(GameStateModuleIO::GameActionQueue* lpActionQueue, CgsID lJunkyardId); // X360 0x82398508
    void EnterCarSelect(GameStateModuleIO::GameActionQueue* lpActionQueue);
    void EnterModification(GameStateModuleIO::GameActionQueue* lpActionQueue);
    void ExitJunkyard(GameStateModuleIO::GameActionQueue* lpActionQueue);             // X360 0x82387880
    void ForceExitJunkyard(GameStateModuleIO::GameActionQueue* lpActionQueue, bool lbToOnlineEvent); // X360 0x8239C418
    void RequestChangeCar(const CgsID& lCarId);
    void StreamingFinished(CgsID lActiveCarZeroId, GameStateModuleIO::GameActionQueue* lpActionQueue);
    void EnterJunkyardAtStartOfGame(GameStateModuleIO::GameActionQueue* lpActionQueue,
                                    CgsID lJunkyardId, CgsID lCarModelId, CgsID lWheelId,
                                    GameStateModuleIO::EPlayerScoringIndex leScoringIndex,
                                    GameStateModuleIO::CarSelectionChangedAction* lpCarSelectChangedAction); // X360 0x82393080
    void ReallyEnterJunkyardAtStartOfGame(GameStateModuleIO::GameActionQueue* lpActionQueue); // X360 0x823931F8
    void OnCarUnlockTickerComplete();
    void SetCarUnlockEnabled(bool lbEnabled);

private:
    // ---- private helpers (DWARF :166-285) ----------------------------------
    void UpdateCarColour(CgsID lCarId, GameStateModuleIO::GameActionQueue* lpActionQueue) const;
    void SaveChosenLiveryForCar(CgsID lCarId);
    void StartTransitionInState(GameStateModuleIO::GameActionQueue* lpActionQueue);      // X360 0x823929D0
    void EndTransitionInState(GameStateModuleIO::GameActionQueue* lpActionQueue);        // X360 0x82392B30
    void StartCarSelectState(GameStateModuleIO::GameActionQueue* lpActionQueue);         // X360 0x823872D0
    void StartCarModificationState(GameStateModuleIO::GameActionQueue* lpActionQueue);
    void StartUnlockState(GameStateModuleIO::GameActionQueue* lpActionQueue);            // X360 0x82387730
    void UpdateRequestCarChangeState(GameStateModuleIO::GameActionQueue* lpActionQueue); // X360 0x82387AB8
    void UpdateChangeCarState(GameStateModuleIO::GameActionQueue* lpActionQueue);        // X360 0x823986D0
    void UpdateUnlockState(GameStateModuleIO::GameActionQueue* lpActionQueue);           // X360 0x82398920
    void EndUnlockState(GameStateModuleIO::GameActionQueue* lpActionQueue);              // X360 0x82392C58
    void UpdateExitState(GameStateModuleIO::GameActionQueue* lpActionQueue);             // X360 0x82398C20
    void SetupSpawnLocations();
    void SetupNormalUnlockList();
    bool IsThisCarInCurrentUnlockSequence(const BrnProgression::CarData* lpProfileCar) const;
    void SetupShutdownUnlockList();
    void SpawnInStartCar(GameStateModuleIO::GameActionQueue* lpActionQueue);
    void GetCurrentPlayerVehicle(CgsID& lrCarID) const;
    const BrnProgression::CarData* GetProfileCarData(CgsID& lrCarID) const;
    void RequestStreamingForUnlock(GameStateModuleIO::GameActionQueue* lpActionQueue);
    void TeleportCurrentVehicle(GameStateModuleIO::GameActionQueue* lpActionQueue);
    CgsID GetNextUnlockCarID(CgsID lCurrentID);

public:
    void DEBUG_UnlockCarsForTesting();

    // Never-called layout pin (complete-class, private access). PARITY-BY-NAMED-MEMBER: pins the
    // ABI-stable structural facts (CgsID == 8B spacing, spawn-array length) rather than the X360
    // absolute byte offsets (which shift under the x64 Pointer32 vs native-pointer delta).
    static void _AssertLayout();

private:
    State                                          meState;                       // +0x00  :172
    f32                                            mfStateTimer;                  // +0x04  :173
    Pointer32<GameStateModule>                     mpGameStateModule;             // +0x08  :175
    Pointer32<const TriggerQueryManager>           mpTriggerQueryManager;         // +0x0C  :176
    Pointer32<BrnProgression::ProgressionManager>  mpProgressionManager;          // +0x10  :177
    Pointer32<const BrnResource::VehicleList>      mpVehicleList;                 // +0x14  :179
    Pointer32<const BrnResource::WheelList>        mpWheelList;                   // +0x18  :180
    CgsID                                          mJunkyardId;                   // +0x20  :182
    Pointer32<const BrnTrigger::SpawnLocation>     maSpawnLocations[KU_CARSELECT_SPAWNLOCATION_COUNT]; // +0x28 :184
    s32                                            meLastSpawnLocationType;       // +0x3C  :185
    CgsID                                          mStartCarId;                   // +0x40  :187
    CgsID                                          mDesiredCarId;                 // +0x48  :188
    CgsID                                          mCacheDuringChangeCarId;       // +0x50  :189
    bool                                           mbWaitingForStreaming;         // +0x58  :191
    u32                                            muUnlockCount;                 // +0x5C  :193
    CgsID                                          mCurrentCarToUnlock;           // +0x60  :194
    bool                                           mbCurrentCarTickerVisible;     // +0x68  :195
    f32                                            mfCarUnlockFadedOutTargetTime; // +0x6C  :196
    // The nine trailing bool flags (DWARF :198..:208 then :290..:292). NOTE: parity-by-named-member
    // binds each NAME to the X360 byte the bodies touch (offsets below); the residual flag<->slot
    // uncertainty is FLAGGED -- two X360 byte slots (0x72/0x74) are never read, and the DWARF
    // declaration order does not 1:1 fix which gameplay bool owns the remaining slots. The spine
    // flags (mbWaitingForStreaming, mbInCarModScreen, mbTransitionInRequestStreaming,
    // mbShutdownUnlockSequence) ARE unambiguous; the rest are best-effort behavioural bindings.
    bool                                           mbNoNormalUnlockCars;          // :198 (Construct-only; layout)
    bool                                           mbTransitionInRequestStreaming;// +0x71 :200 (Update case 1 streaming kick)
    bool                                           mbNeedToTeleportTrick;         // +0x75 :202 (exit-pending latch -- FLAG)
    bool                                           mbInCarModScreen;              // +0x73 :204 (CarMod vs CarSelect return state)
    bool                                           mbShutdownUnlockSequence;      // +0x70 :206 (SetupShutdownUnlockList gate / state 3)
    bool                                           mbCarUnlockEnabled;            // :208 (Construct=true; public SetCarUnlockEnabled)
    bool                                           mbDEBUG_DisableUnlock;         // +0x76 :290 (zeroes the unlock count when set -- FLAG)
    bool                                           mbDEBUG_UnlockTrophyCarsForTesting;   // +0x77 :291
    bool                                           mbDEBUG_UnlockShutdownCarsForTesting; // +0x78 :292
};
}
