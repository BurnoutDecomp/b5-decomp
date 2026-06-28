#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID

#include <cstdint>            // uintptr_t (Pointer32)

// Minimal owning slice for BrnGameState::CarSelectManager (DWARF: BrnCarSelectManager.h:59,
// non-polymorphic). Only the members Construct / EnterModification touch are named; the full
// ~0x1E0 layout is reconstructed by the CarSelectManager TU. Pointers are 32-bit on the X360 and
// stored via Pointer32<T> (the committed BrnGameStateFlybyManager.cpp precedent); semantic parity
// is by named member (x64 byte offsets differ -- per AGENTS.md), so no byte-exact padding here.

namespace BrnGameState     { class GameStateModule; class TriggerQueryManager; }
namespace BrnProgression   { class ProgressionManager; }
namespace BrnResource      { struct VehicleList; struct WheelList; }
namespace BrnTrigger       { struct SpawnLocation; }
namespace InputBuffer      { struct GameActionQueue; }

namespace BrnGameState
{
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

    static const u32 KU_CARSELECT_SPAWNLOCATION_COUNT = 5;
    // BrnTrigger::SpawnLocation::Type::E_TYPE_COUNT (the "no spawn location" sentinel == 4).
    static const s32 KI_SPAWNLOCATIONTYPE_NONE = 4;

    void Construct(const TriggerQueryManager* lpTriggerQueryManager,
                   GameStateModule* lpGameStateModule,
                   BrnProgression::ProgressionManager* lpProgressionManager);   // X360 0x823564D0
    void EnterModification(InputBuffer::GameActionQueue* lpActionQueue);        // X360 (EnterModification)

    // Called by EnterModification; reconstructed by its own slice (declared-only here).
    void StartCarModificationState(InputBuffer::GameActionQueue* lpActionQueue);

    // ADDITIVE GROW (declare-only; bodied by its Wave-C TU @ X360 0x82398508).
    // DriveThruManager::ProcessDriveThru case 0 (junk yard) hands the cached junkyard id to the
    // car-select junkyard flow. Declare-only here.
    void EnterJunkyard(InputBuffer::GameActionQueue* lpActionQueue, CgsID lJunkyardId);

private:
    State                                          meState;
    f32                                            mfStateTimer;
    Pointer32<GameStateModule>                     mpGameStateModule;
    Pointer32<const TriggerQueryManager>           mpTriggerQueryManager;
    Pointer32<BrnProgression::ProgressionManager>  mpProgressionManager;
    Pointer32<const BrnResource::VehicleList>      mpVehicleList;
    Pointer32<const BrnResource::WheelList>        mpWheelList;
    CgsID                                          mJunkyardId;
    Pointer32<const BrnTrigger::SpawnLocation>     maSpawnLocations[KU_CARSELECT_SPAWNLOCATION_COUNT];
    s32                                            meLastSpawnLocationType;
    CgsID                                          mStartCarId;
    CgsID                                          mDesiredCarId;
    CgsID                                          mCacheDuringChangeCarId;
    bool                                           mbWaitingForStreaming;
    u32                                            muUnlockCount;
    CgsID                                          mCurrentCarToUnlock;           // DWARF :194 (not written by Construct)
    bool                                           mbCurrentCarTickerVisible;     // DWARF :195 (not written by Construct)
    f32                                            mfCarUnlockFadedOutTargetTime; // DWARF :196 (not written by Construct)
    bool                                           mbNoNormalUnlockCars;
    bool                                           mbTransitionInRequestStreaming;
    bool                                           mbNeedToTeleportTrick;
    bool                                           mbInCarModScreen;
    bool                                           mbShutdownUnlockSequence;
    bool                                           mbCarUnlockEnabled;
    bool                                           mbDEBUG_DisableUnlock;
    bool                                           mbDEBUG_UnlockTrophyCarsForTesting;
    bool                                           mbDEBUG_UnlockShutdownCarsForTesting;
};
}
