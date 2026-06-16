#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID, Vector3

#include <cstdint>            // uintptr_t (Pointer32)

// Minimal owning slice for BrnGameState::OnlineCarSelectManager (DWARF: BrnOnlineCarSelectManager.h:61,
// non-polymorphic struct). Only the members the four reconstructed functions of this TU touch are
// named; the full ~0x6C layout is reconstructed by this TU's .cpp. The DWARF member ORDER is preserved
// (it matches the X360 store order exactly: see the per-member X360 offset comments). Pointers are
// 32-bit on the X360 and stored via Pointer32<T> -- the committed BrnCarSelectManager.h /
// BrnGameStateFlybyManager.cpp precedent; semantic parity is by named member (x64 byte offsets differ
// per AGENTS.md), so no byte-exact inter-member padding is forced here.

namespace BrnGameState   { class GameStateModule; }
namespace BrnProgression { class ProgressionManager; }
namespace BrnResource    { struct VehicleList; struct WheelList; }
namespace InputBuffer    { struct GameActionQueue; }

namespace BrnGameState
{
// X360 32-bit pointer storage (mirrors BrnCarSelectManager.h / BrnGameStateFlybyManager.cpp).
template <typename T>
struct Pointer32
{
    u32 muAddress;
    void Set(T* lpPointer) { muAddress = static_cast<u32>(reinterpret_cast<uintptr_t>(lpPointer)); }
    T*   Get() const       { return reinterpret_cast<T*>(static_cast<uintptr_t>(muAddress)); }
};

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
    void EnterModification(InputBuffer::GameActionQueue* lpActionQueue);        // X360 0x8238EEA0
    void EnterWaitForHost(InputBuffer::GameActionQueue* lpActionQueue);         // X360 0x82356650
    void StreamingFinished(CgsID lActiveCarZeroId,
                           InputBuffer::GameActionQueue* lpActionQueue);        // X360 0x82358AC8

    // Called by EnterModification; reconstructed by its own slice (declared-only here). DWARF spells
    // it StartCarModificationState(InputBuffer::GameActionQueue*); the X360 forwards the queue arg.
    void StartCarModificationState(InputBuffer::GameActionQueue* lpActionQueue);

private:
    EInternalState                                meInternalState;          // X360 this+0
    Pointer32<GameStateModule>                    mpGameStateModule;        // X360 this+4
    Pointer32<BrnProgression::ProgressionManager> mpProgressionManager;     // X360 this+8
    Pointer32<const BrnResource::VehicleList>     mpVehicleList;            // X360 this+12
    Pointer32<const BrnResource::WheelList>       mpWheelList;              // X360 this+16
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
