#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID, Vector3

#include "GameSource/GameState/BrnGameStateSharedIO.h" // GameStateModuleIO::GameActionQueue (real typedef)
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h" // BrnGameState::HostPointer

// Minimal owning slice for BrnGameState::OnlineCarSelectManager (DWARF: BrnOnlineCarSelectManager.h:61,
// non-polymorphic struct). Only the members the four reconstructed functions of this TU touch are
// named; the full ~0x6C layout is reconstructed by this TU's .cpp. The DWARF member ORDER is preserved
// (it matches the X360 store order exactly: see the per-member X360 offset comments). Pointers are
// 32-bit on the console and held as full host pointers here (HostPointer<T>, as BrnCarSelectManager.h
// does); semantic parity is by named member (x64 byte offsets differ per AGENTS.md), so no
// byte-exact inter-member padding is forced here.

namespace BrnGameState   { class GameStateModule; }
namespace BrnProgression { class ProgressionManager; }
namespace BrnResource    { struct VehicleList; struct WheelList; }

namespace BrnGameState
{
// The owner and list pointers are held in BrnCarSelectManager.h's HostPointer (a full host
// pointer). The 32-bit copy this header used to carry truncated x64 addresses; see the note on
// HostPointer for the measured failure.

struct OnlineCarSelectManager
{
    // DWARF BrnOnlineCarSelectManager.h:65.
    enum EInternalState
    {
        E_INTERNAL_STATE_NONE                    = 0,
        E_INTERNAL_STATE_CAR_SELECT              = 1,
        E_INTERNAL_STATE_CAR_MODIFICATION        = 2,
        E_INTERNAL_STATE_WAIT_FOR_HOST_TO_CHOOSE = 3,
        E_INTERNAL_STATE_WAIT_FOR_ONLINE         = 4,
        E_INTERNAL_STATE_COUNT                   = 5,
    };

    // DWARF BrnOnlineCarSelectManager.h:77.
    enum ECarChangeState
    {
        E_CAR_CHANGE_NONE    = 0,
        E_CAR_CHANGE_REQUEST = 1,
        E_CAR_CHANGE_BUSY    = 2,
        E_CAR_CHANGE_COUNT   = 3,
    };

    void Construct(GameStateModule* lpGameStateModule,
                   BrnProgression::ProgressionManager* lpProgressionManager);  // X360 0x823565C0
    void EnterModification(GameStateModuleIO::GameActionQueue* lpActionQueue);        // X360 0x8238EEA0
    void EnterWaitForHost(GameStateModuleIO::GameActionQueue* lpActionQueue);         // X360 0x82356650
    void StreamingFinished(CgsID lActiveCarZeroId,
                           GameStateModuleIO::GameActionQueue* lpActionQueue);        // X360 0x82358AC8

    // Reference Prepare(const VehicleList*, const WheelList*), inlined on the console into
    // GameStateModule::Prepare's terminal stage (the two `stw` into +0x0C / +0x10).
    void Prepare(const BrnResource::VehicleList* lpVehicleList, const BrnResource::WheelList* lpWheelList)
    {
        mpWheelList.Set(lpWheelList);
        mpVehicleList.Set(lpVehicleList);
    }

    // Reference inline accessor; ProcessGameEvents' case 123 tests it (`lbzx` of +0x14).
    bool IsInOnlineCarSelect() const { return mbIsInOnlineCarSelect; }

    // Reference inline accessor; case 123 reads it (`ld` of +0x48) to hand the free-burn car back.
    CgsID GetFreeburnCarId() const { return mFreeburnCarId; }

    // Leave online car select: reset the state words, post actions 76 / 77 / 7
    // and lift the car-select pause. Body in BrnOnlineCarSelectManager_wN3_01.cpp.
    void ExitOnlineCarSelect(GameStateModuleIO::GameActionQueue* lpActionQueue);

    // Called by EnterModification; reconstructed by its own slice (declared-only here). DWARF spells
    // it StartCarModificationState(GameStateModuleIO::GameActionQueue*); the X360 forwards the queue arg.
    void StartCarModificationState(GameStateModuleIO::GameActionQueue* lpActionQueue);

private:
    EInternalState                                meInternalState;          // X360 this+0
    HostPointer<GameStateModule>                    mpGameStateModule;        // console +0x04
    HostPointer<BrnProgression::ProgressionManager> mpProgressionManager;     // console +0x08
    HostPointer<const BrnResource::VehicleList>     mpVehicleList;            // console +0x0C
    HostPointer<const BrnResource::WheelList>       mpWheelList;              // console +0x10
    bool                                          mbIsInOnlineCarSelect;    // X360 this+20
    f32                                           mfTimeLeftInCarSelect;    // X360 this+24 (NOT zeroed by Construct)
    Vector3                                       mSpawnPosition;           // X360 this+32 (stvx128 zero)
    Vector3                                       mSpawnDirection;          // X360 this+48 (stvx128 zero)
    CgsID                                         mStartCarId;              // X360 this+64
    CgsID                                         mFreeburnCarId;           // X360 this+72
    CgsID                                         mDesiredCarId;            // X360 this+80
    CgsID                                         mCacheDuringChangeCarId;  // X360 this+88
    bool                                          mbWaitingForStreaming;    // X360 this+96 (StreamingFinished clears)
    bool                                          mbHostChoiceAndNotHost;   // X360 this+97
    ECarChangeState                               meStateOfChangingCars;    // X360 this+100
    s32                                           miVehicleClassLimit;      // DWARF :193 (NOT zeroed by Construct)
};
}
