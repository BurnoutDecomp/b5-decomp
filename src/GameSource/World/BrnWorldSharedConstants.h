#ifndef BRN_WORLD_SHARED_CONSTANTS_H
#define BRN_WORLD_SHARED_CONSTANTS_H

#include "types.hpp"

// =============================================================================
// BrnWorldSharedConstants.h  (OWNING HEADER for the BrnWorld race-car population limits)
//
// DWARF home: GameSource/World/BrnWorldSharedConstants.h (references/DecFIGS/dwarfdump/...,
// :32-:35). MINIMAL OWNING SLICE, landed 2026-09-02 (rival-spawn wave R) because
// RaceEventData::GetStartGridSlot's console assert names KI_MAX_RIVALS_IN_MODE verbatim
// ("muStartGridCount <= (uint32_t)KI_MAX_RIVALS_IN_MODE", BrnRaceEventData.h:1166, compared
// as `cmplwi r11, 7` @0x823294B4) and GameModeParams' start-location accessors name
// KI_MAX_ACTIVE_RACE_CARS ("liOpponentIndex >= 0 && liOpponentIndex <
// BrnWorld::KI_MAX_ACTIVE_RACE_CARS", BrnGameModeParams.h:1177/:1185, `cmpwi r30, 8`).
//
// [!] ONLY THE TWO CONSTANTS NOTHING ELSE OWNS ARE DEFINED HERE. The DWARF file's neighbours
// KI_MAX_ACTIVE_RACE_CARS (:32, == 8) and KI_MAX_OUT_OF_RANGE_RACE_CARS (:35, == 35) were
// already defined in namespace BrnWorld by earlier slices -- GameSource/World/CrashModule/
// SharedIO/BrnCrashModuleNetworkIOInterfaces.h:15 and GameSource/World/AI/SharedIO/
// BrnAICarOutputInterface.h:40 respectively -- and redefining them here is a C2374 in any TU
// that sees both. Consolidating those two into this owner is a separate, cross-TU move; until
// then they stay where they are. The remaining DWARF constants (KI_INVALID_OPPONENT,
// KU_MAX_LAPS, ...) grow here when a caller needs them. Single owner -- do not fork.
// =============================================================================

namespace BrnWorld
{
    const s32 KI_MAX_RIVALS_IN_MODE  = 7;    // DWARF :33 -- Array<OpponentData,7> / the start-grid table width
    const s32 KI_MAX_RIVALS_IN_WORLD = 34;   // DWARF :34

    // [takedown wave 2026-09-02] Who drives an active race car. DWARF BrnWorldSharedConstants.h:101.
    // X360-attested by the SetPlayerCarDriverAction producers: DriveThruManager::SetPlayerCarDriver
    // @0x823867A0 posts 1 for "player driving" / 2 otherwise, and RaceCarEntityModule::HandleGameActions
    // @0x8230BE08 case 7 tests `== 1` for the entity-module (player) arm.
    enum CarControl
    {
        E_CAR_CONTROL_NONE          = 0,
        E_CAR_CONTROL_ENTITY_MODULE = 1,
        E_CAR_CONTROL_AI_MODULE     = 2,
    };
}

#endif // BRN_WORLD_SHARED_CONSTANTS_H
