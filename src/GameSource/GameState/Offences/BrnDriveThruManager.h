#pragma once

#include "types.hpp"

namespace BrnTrigger
{
// MINIMAL STUB for BrnTrigger::GenericRegion::Type. The real GenericRegion type is not yet
// reconstructed (no committed home in b5-decomp/src). Only the five region categories the X360
// GetTotalDriveThrusOfType switch keys on are declared, with the values it compares (case 0..4).
// Replace with the real owning header when GenericRegion lands.
struct GenericRegion
{
    enum Type
    {
        E_TYPE_JUNK_YARD   = 0,
        E_TYPE_GAS_STATION = 1,
        E_TYPE_BODY_SHOP   = 2,
        E_TYPE_PAINT_SHOP  = 3,
        E_TYPE_CAR_PARK    = 4,
    };
};
}

namespace BrnGameState
{
// Minimal owning slice for BrnGameState::DriveThruManager (DWARF: a plain struct, no base).
// Only GetTotalDriveThrusOfType is owned by this TU; it touches the six per-category total
// counters, so a leading reserved blob places them at the DWARF word offsets (a1[554..558]).
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
};
}
