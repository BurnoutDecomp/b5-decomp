// ===== Owning header: GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h =====
// BrnGameState::TriggerQueryManager — the per-frame trigger-query cache owned by
// GameStateModule (DWARF BrnGameStateModule.h:772 -> `TriggerQueryManager mTriggerQueryManager`).
// Plain value type (no base, no vtable: none of the three X360 bodies touch a vptr on `this`).
//
// MINIMAL SLICE (BrnDriveThruManager.h / BrnGameStateModule.h precedent): only the members the
// three reconstructed functions touch are declared, placed at their X360 byte offsets by leading
// reserved blobs. The full class preamble (offsets 0..1807) is NOT modelled — grow these reserved
// blobs into real members from the full DWARF layout when the whole TriggerQueryManager TU lands.
// Offsets are X360-attested:
//   AddLandmarkIndexForGameMode      : this+1808 (byte store 0 = mbTriggersUpdated), array this+1832
//   GetPlayerCurrentTrafficLightId   : this+1809 (bool), this+1812 (id)
//   ClearLandmarkIndexesForGameMode  : array this+1832 (count word this+1864 == array.miCount)
#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsArray.h"                             // Array<T,N> (generic, committed)
#include "GameSource/GameState/BrnGameStateTypes.h"                                 // BrnGameState::LandmarkIndex
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"           // BrnGameState::LightTriggerId (committed typedef home)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                            // CgsModule::EventQueue<T,N>
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h" // BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent

namespace BrnGameState
{
// X360-attested fixed array of the landmark/region indexes currently armed for the active game
// mode. The element home (BrnGameState::LandmarkIndex) and the generic Array<T,N> body are both
// committed; the leaf explicit instantiation lives in Array_LandmarkIndex_16.cpp.
typedef Array<LandmarkIndex, 16> LandmarkIndexArray;

class TriggerQueryManager
{
public:

    // ---- the three functions this TU owns (bodies in BrnTriggerQueryManager.cpp) ----

    // X360 0x82326538. Drain every armed landmark index back out as a "remove trigger" event onto
    // the trigger-entity module's remove queue, then empty the landmark array.
    void ClearLandmarkIndexesForGameMode(
        CgsModule::EventQueue<BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent, 256>& lrRemoveTriggerQueue);

    // X360 0x823265E8. Arm one landmark index for the active mode if it is not already present.
    // Returns whether the index is now present in the array (always true on the X360 path).
    bool AddLandmarkIndexForGameMode(LandmarkIndex lLandmarkIndex);

    // X360 0x82355D78. Return the trigger id of the traffic-light region the player is currently
    // in. Asserts IsPlayerInTrafficLightRegion() first (BrnTriggerQueryManager.h:318).
    LightTriggerId GetPlayerCurrentTrafficLightId() const;

    // Query attested by the GetPlayerCurrentTrafficLightId assert string; declared-only here
    // (its body is elsewhere in the full TU). Reads mbPlayerInTrafficLightRegion.
    bool IsPlayerInTrafficLightRegion() const;

private:

    // ---- leading preamble (offsets 0..1807) not yet reconstructed; reserved to place the
    //      touched members at their X360 byte offsets. Grow into real members when the full TU lands.
    u8 mauReserved_Head[1808];                       // this+0 .. this+1807

    // this+1808 (byte). "Triggers updated" dirty flag (DWARF BrnTriggerQueryManager.h:234).
    // AddLandmarkIndexForGameMode clears it (sets false) whenever a new landmark index is armed,
    // marking the cached trigger-query state stale until the next refresh.
    bool mbTriggersUpdated;                          // this+1808  (DWARF h:234)

    // this+1809 (byte). The flag IsPlayerInTrafficLightRegion() returns / the
    // GetPlayerCurrentTrafficLightId assert guards.
    bool mbPlayerInTrafficLightRegion;               // this+1809

    u8   mau8Pad_180A[2];                            // this+1810 .. this+1811 (align to u32)

    // this+1812. The traffic-light trigger handle handed back by GetPlayerCurrentTrafficLightId.
    LightTriggerId mPlayerCurrentTrafficLightId;     // this+1812

    // this+1816 .. this+1831: other preamble members not yet reconstructed (reserved so the array
    // lands at this+1832).
    u8 mauReserved_Mid[16];                          // this+1816 .. this+1831

    // this+1832. Element store this+1832..this+1863 (16 * 2B), live-count word this+1864
    // (== v5[8] / v2[466] in the X360 bodies).
    LandmarkIndexArray maLandmarkIndexes;            // this+1832
};
}
