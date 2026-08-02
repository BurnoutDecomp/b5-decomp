#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // GameStateModuleIO::EPlayerScoringIndex (committed home)

#include <cstdint>            // uintptr_t

// Owning header for BrnGameState::CarSelectManager -- the junkyard car-select state machine (the
// player drove into a junkyard; this runs the car-swap / car-unlock-sequence / car-modification
// flow). DWARF: BrnCarSelectManager.h:59 (non-polymorphic struct). GROWN to the full DWARF member
// layout + all methods; bodies land across two .cpp TUs.
//
// PARITY-BY-NAMED-MEMBER (AGENTS.md): on the X360 every raw pointer is 4 bytes and CgsID is 8
// bytes, so the console class is ~0x7C bytes. On the x64 gate the pointers are HOST pointers held
// in a HostPointer<T> wrapper (8 bytes) -- the class is WIDER here and that is fine, because what
// is pinned is member ORDER + the ABI-stable structural facts (relative offsetof asserts in a
// never-called _AssertLayout()), NOT absolute byte offsets.
// ⛔ It did not always say that: until 2026-08-01 the wrapper stored the host pointer in a u32 to
// hold the 4-byte spacing, which silently truncated every pointer on x64. See the banner on
// HostPointer below -- that is the single most important comment in this header.
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
    // CarSelectionChangedAction is TYPED as of 2026-08-01 -- its full 64-byte layout is in
    // BrnGameActions.h (both ends of action 64 agree on every offset). The forward declaration
    // stays so this header does not have to pull the whole action family; the .cpp includes it.
    struct CarSelectionChangedAction;
    struct ControllerInput;                                    // Update() param (passed through; never deref'd here)
}

// ⛔⛔ CORRECTED 2026-08-01 -- THIS WAS A WILD-POINTER BUG, PROVEN AT RUNTIME.
//
// This template used to be:
//     struct Pointer32 { u32 muAddress;
//         void Set(T* p) { muAddress = static_cast<u32>(reinterpret_cast<uintptr_t>(p)); }
//         T*   Get() const { return reinterpret_cast<T*>(static_cast<uintptr_t>(muAddress)); } };
// i.e. it stored a HOST pointer in 32 bits to keep the X360's 4-byte pointer spacing. The game
// builds x64 (build_game_exe.bat line 19, "VS 2022 x64 toolchain"), so every Set() DISCARDED THE
// TOP 32 BITS and every Get() handed back a wild address. Measured, first boot after
// GameStateModule finally constructed one:
//     [CarSelectManager::Prepare] in veh=140699359625488 (0x7FF7586AC110)
//                                out veh=525975824       (0x00000000586AC110)   roundtrip=0
// All EIGHT members below were affected, mpGameStateModule included -- so the first
// `mpGameStateModule.Get()->...` in Update() would have been an access violation, and
// maSpawnLocations[1].Get()->GetType() (the junkyard start-of-game spawn) likewise.
//
// ⚠️⚠️ AND THE OBVIOUS DIAGNOSTIC HIDES IT: CgsStrStream.cpp:83 renders operator<<(void*) as
// `(unsigned)(size_t)p` -- ONLY THE LOW 32 BITS. Printing both ends as void* shows two IDENTICAL
// addresses for a pointer that was in fact truncated. Print pointers as u64 in this engine.
//
// THE FIX, and why it is faithful: the header's own contract (see the PARITY-BY-NAMED-MEMBER
// banner above) is that absolute X360 byte offsets are NOT baked here -- _AssertLayout pins only
// ABI-stable structural facts. So the 4-byte spacing bought nothing and cost correctness. The
// wrapper survives only as the .Set()/.Get() call-site vocabulary; it now holds a real pointer.
// Renamed off "Pointer32" because that name is no longer true (and because
// BrnOnlineCarSelectManager.h declares its own BrnGameState::Pointer32 with the SAME name in the
// SAME namespace -- a redefinition the moment both headers meet in one TU).
//
// ⚠️ THE SAME TRUNCATING Pointer32 IS STILL COPY-PASTED IN THREE OTHER PLACES, all currently
// UNMOUNTED (so latent, not live): BrnOnlineCarSelectManager.h:25, BrnGameStateFlybyManager.cpp:7,
// BrnGameStateOnlineFlybyManager.h:46. Whoever mounts any of them must fix it there too.
template <typename T>
struct HostPointer
{
    T* mpPointer;
    void Set(T* lpPointer) { mpPointer = lpPointer; }
    T*   Get() const       { return mpPointer; }
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

    // MOVED OUT OF THE PRIVATE BLOCK 2026-08-02 (car-select handover wave). The DWARF groups it
    // with the private helpers, but GameStateModule::ProcessGameEvents @0x823A0A18 calls it
    // DIRECTLY from the case-94 junkyard arm (xref in the ARTIST export set, alongside
    // EnterModification and ExitJunkyard which the DWARF already has public). Its three other
    // callers (StartUnlockState / EndTransitionInState / EndUnlockState) are internal.
    void StartCarSelectState(GameStateModuleIO::GameActionQueue* lpActionQueue);         // X360 0x823872D0

    // [FLAG PC bring-up] NOT A CONSOLE FUNCTION -- the stand-in for the streaming-complete
    // signal that closes a junkyard exit. ExitJunkyard sets mbWaitingForStreaming and the ONLY
    // thing that clears it is StreamingFinished, whose one console caller is
    // GameStateModule::ProcessStreamingCompleteEvent @0x82390200 -- reached from a world
    // StreamingCompleteEvent through ProcessGameEvents, none of which exists on this build. With
    // no clear, Update's case-9 arm (UpdateExitState) returns early for ever and the junkyard
    // exit never finishes. This calls the console's own StreamingFinished with mDesiredCarId (so
    // its `lActiveCarZeroId == mDesiredCarId` arm is the one that runs), and ONLY while the
    // manager is actually EXITING and actually waiting -- it can therefore never disturb the
    // car-change states, which are the other users of that latch.
    // DELETE-WHEN ProcessStreamingCompleteEvent + the world StreamingCompleteEvent are real.
    void UpdateExitStreamingBringUp(GameStateModuleIO::GameActionQueue* lpActionQueue);

private:
    // ---- private helpers (DWARF :166-285) ----------------------------------
    void UpdateCarColour(CgsID lCarId, GameStateModuleIO::GameActionQueue* lpActionQueue) const;
    void SaveChosenLiveryForCar(CgsID lCarId);
    void StartTransitionInState(GameStateModuleIO::GameActionQueue* lpActionQueue);      // X360 0x823929D0
    void EndTransitionInState(GameStateModuleIO::GameActionQueue* lpActionQueue);        // X360 0x82392B30
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
    // absolute byte offsets (which shift under the x64 4-byte vs 8-byte pointer delta).
    static void _AssertLayout();

private:
    State                                          meState;                       // +0x00  :172
    f32                                            mfStateTimer;                  // +0x04  :173
    HostPointer<GameStateModule>                   mpGameStateModule;             // +0x08  :175
    HostPointer<const TriggerQueryManager>         mpTriggerQueryManager;         // +0x0C  :176
    HostPointer<BrnProgression::ProgressionManager> mpProgressionManager;          // +0x10  :177
    HostPointer<const BrnResource::VehicleList>    mpVehicleList;                 // +0x14  :179
    HostPointer<const BrnResource::WheelList>      mpWheelList;                   // +0x18  :180
    CgsID                                          mJunkyardId;                   // +0x20  :182
    HostPointer<const BrnTrigger::SpawnLocation>   maSpawnLocations[KU_CARSELECT_SPAWNLOCATION_COUNT]; // +0x28 :184
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
