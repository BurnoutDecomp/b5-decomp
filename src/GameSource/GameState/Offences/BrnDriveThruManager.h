#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // CgsID
#include "SharedClasses/Trigger/BrnGenericRegion.h"          // BrnTrigger::GenericRegion (REAL home; replaces the old local stub)

// Forward declarations of the types HandleDriveThru routes through (pointers only).
namespace BrnResource { struct VehicleList; }
namespace BrnGameState { namespace GameStateModuleIO { struct OutputBuffer; } }
namespace BrnWorld { namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; } }

namespace BrnGameState
{
// Minimal owning slice for BrnGameState::DriveThruManager (DWARF: a plain struct, no base).
// GetTotalDriveThrusOfType is owned by the DriveThruManager TU; it touches the six per-category
// total counters, so a leading reserved blob places them at the DWARF word offsets (a1[554..558]).
// HandleDriveThru (X360 0x8239B010) is added DECLARATION-ONLY for the TriggerQueryManager
// dispatcher (its body lives in the DriveThruManager TU).
// Replace mauReserved_Head with the real leading members (maDriveThruTriggerData[46] +
// maDriveThroughClosed) when the full DriveThruManager TU lands.
struct DriveThruManager
{
    u32 mauReserved_Head[554];   // maDriveThruTriggerData[46] (552) + maDriveThroughClosed (2)

    s32 miTotalCarParks;     // word 554
    s32 miTotalGasStations;  // word 555
    s32 miTotalBodyShops;    // word 556
    s32 miTotalPaintShops;   // word 557
    s32 miTotalJunkYards;    // word 558
    s32 miTotalDriveThrus;   // word 559

    s32 GetTotalDriveThrusOfType(BrnTrigger::GenericRegion::Type leTriggerType);

    // X360 0x8239B010. Routes a player-entered drive-thru region (junk yard / gas station / body
    // shop / paint shop / car park) into the drive-thru / shop flow: checks the player car against
    // the vehicle list, tallies discovered drive-thrus, and posts the appropriate game actions onto
    // the output buffer. Declaration-only here; body in the DriveThruManager TU.
    void HandleDriveThru(const BrnTrigger::GenericRegion*                                            lpRegion,
                         const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                         const BrnResource::VehicleList*                                             lpVehicleList,
                         GameStateModuleIO::OutputBuffer*                                            lpOutput);
};
}
