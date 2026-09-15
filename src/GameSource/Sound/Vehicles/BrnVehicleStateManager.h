#ifndef BRN_SOUND_VEHICLES_VEHICLE_STATE_MANAGER_H
#define BRN_SOUND_VEHICLES_VEHICLE_STATE_MANAGER_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"

namespace BrnResource { struct VehicleListEntry; }

// =============================================================================
// BrnSound::Vehicles::VehicleStateManager
//   GameSource/Sound/Vehicles/BrnVehicleStateManager.h (assert-cited home) +
//   GameSource/Sound/Vehicles/BrnVehicleStateManager.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// VehicleStateManager owns the per-active-race-car sound state. AI cars are
// assigned to a fixed bank of engine-sound "voices"; GetAIEngineAssignment maps an
// active-race-car index (0..7) to its engine-voice slot through a static lookup
// table. The result is consumed by the engine effect attach path
// (BrnSound::Vehicles::Engines::DualGinsuEffect::Attach).
//
// This TU bodies exactly ONE ledger function:
//   GetAIEngineAssignment  @ 0x82682050  (BrnVehicleStateManager.h:143 assert site)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
class VehicleStateManager : public BrnSound::Logic::BrnStateManager
{
public:

// BrnVehicleStateManager.h (assert-cited region). Number of active race-car slots;
// the asm bounds-checks the index against this (cmpwi r31, 8 ; the assert text is
// "liVehicleIndex >= 0 && liVehicleIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT").
    static const s32 KI_ACTIVE_RACE_CAR_COUNT = 8;

    // @ 0x82683D50 (console StateManager vtable slot 10, +40):
    //     return a2 == 1 && *(this+20) == 2 || *(this+20) == a2;
    // The AI vehicle manager (meMapState 2) treats the PLAYER registrations (state 1
    // descriptors: PlayerVehicleState, PhysicsControl, DualGinsuExhaustEffect, ...) as
    // aliases of its own, so CreateState / CreateEffectObject / CreateEffectControl
    // fall back to the player family wherever no AI-specific class (0x2xxxx) exists.
    // For the player manager (meMapState 1) this is the base compare.
    virtual bool IsStateAlias(s32 liState) const
    {
        return (liState == 1 && meMapState == 2) || meMapState == liState;
    }

// BrnVehicleStateManager.h:143 (assert site). Map an active-race-car index to its
// AI engine-voice assignment via the static lookup table. Asserts the index is in
// [0, E_ACTIVE_RACE_CAR_INDEX_COUNT). Returns the assigned voice (a u8 from the
// table). @ 0x82682050.
    static u8 GetAIEngineAssignment( u32 luVehicleIndex );
    static bool AddEntry(CgsID lAssetId,
                         const BrnResource::VehicleListEntry* lpVehicleEntry,
                         u64 luUserId, bool lbIsPlayer);
    static bool RemoveEntry(CgsID lAssetId, u64 luUserId);
    static const BrnResource::VehicleListEntry* GetLoadedVehicleEntry(u32 luUserId);
    static CgsID GetLoadedAssetId(u32 luUserId);
    static bool IsLoadedEntryPlayer(u32 luUserId);
    static bool IsEntryAdded(u32 luUserId);
    static bool IsDesiredEntryPlayer(u32 luUserId);
    static bool IsAssetAttached(u32 luUserId);
    // The two remaining module-wide tables AIVehicleStateManager::UpdateVehicleLoading
    // @0x826B1C70 reads by name: the attached-as-player bit array (qword_82FFB378)
    // and the attached asset ids (qword_82FFB3C8), both written by OnAssetLoaded /
    // OnAssetUnloaded below.
    static bool  IsAttachedEntryPlayer(u32 luUserId);
    static CgsID GetAttachedAssetId(u32 luUserId);

    void OnAssetLoaded(CgsID lAssetId, u32 luUserId, bool lbIsPlayer);
    void OnAssetUnloaded(CgsID lAssetId, u32 luUserId);
    void AddRegistry(const char* lpcEngineName, bool lbUseFilePath); // @ 0x826CA070

private:
    static u8 GenerateAIEngineAssignment(const BrnResource::VehicleListEntry* lpVehicleEntry,
                                         u32 luVehicleIndex);
};
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_VEHICLE_STATE_MANAGER_H
