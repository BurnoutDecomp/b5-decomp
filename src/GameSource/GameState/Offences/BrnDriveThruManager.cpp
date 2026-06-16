#include "GameSource/GameState/Offences/BrnDriveThruManager.h"

namespace BrnGameState
{
// X360: BrnGameState::DriveThruManager::GetTotalDriveThrusOfType (0x82356468). Returns the
// total number of drive-thrus of a given GenericRegion::Type in the loaded world. The X360
// switch indexed distinct word slots in `this`; the DWARF layout maps them to the six
// per-category counters. Every unhandled type returns 0.
s32 DriveThruManager::GetTotalDriveThrusOfType(BrnTrigger::GenericRegion::Type leTriggerType)
{
    s32 liResult;
    switch (leTriggerType)
    {
        case BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD:   liResult = miTotalJunkYards;    break;
        case BrnTrigger::GenericRegion::E_TYPE_GAS_STATION: liResult = miTotalGasStations;  break;
        case BrnTrigger::GenericRegion::E_TYPE_BODY_SHOP:   liResult = miTotalBodyShops;    break;
        case BrnTrigger::GenericRegion::E_TYPE_PAINT_SHOP:  liResult = miTotalPaintShops;   break;
        case BrnTrigger::GenericRegion::E_TYPE_CAR_PARK:    liResult = miTotalCarParks;     break;
        default:                                            liResult = 0;                   break;
    }
    return liResult;
}
}
