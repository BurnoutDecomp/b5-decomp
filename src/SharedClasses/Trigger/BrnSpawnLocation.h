#pragma once

// ===================================================================================
// BrnTrigger::SpawnLocation -- minimal owning slice
//   b5-decomp/src/SharedClasses/Trigger/BrnSpawnLocation.h
//
// One spawn-location record inside a junkyard's TriggerData (TriggerData::GetSpawnLocation, the
// SpawnLocation* array at TriggerData+0x6C). The car-select junkyard flow caches up to
// KU_CARSELECT_SPAWNLOCATION_COUNT (5) of these and reads each one's Type to seed the spawn / drop-in
// game actions.
//
// Layout fact proven from BURNOUT_X360_ARTIST.XEX (CarSelectManager::EndUnlockState 0x82392C58,
// UpdateRequestCarChangeState 0x82387AB8, EnterJunkyardAtStartOfGame 0x82393080):
//   * the type discriminator muType is a u8 at SpawnLocation +0x28; GetType() reads it and the
//     callers assert `muType < E_TYPE_COUNT` (E_TYPE_COUNT == 4) at BrnSpawnLocation.h:152.
// Only the Type discriminator + GetType() are modelled (the rest of the record -- the spawn
// transform the X360 vector-copies into the reset/car-selection actions -- belongs to the full
// SpawnLocation TU). All access is by name; GetType()'s body lands with that TU (declare-only here
// is sufficient for the `cl /c` compile gate, mirroring TriggerData::GetSpawnLocation).
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID (GetJunkyardId return)

namespace BrnTrigger
{
    struct SpawnLocation
    {
        // DWARF / X360: the spawn-location kind discriminator. E_TYPE_COUNT (4) is the sentinel the
        // callers assert against; the concrete enumerator names are not in this TU's exports, so only
        // the count sentinel + numeric meaning are pinned.
        enum Type
        {
            E_TYPE_0     = 0,   // FLAG: enumerator names not in exports (only E_TYPE_COUNT is attested)
            E_TYPE_1     = 1,
            E_TYPE_2     = 2,
            E_TYPE_3     = 3,
            E_TYPE_COUNT = 4,   // BrnSpawnLocation.h:152 assert bound
        };

        // X360: read the u8 at this+0x28 and return it as Type. Declare-only here (body in the
        // SpawnLocation TU); the asserting callers reproduce the `muType < E_TYPE_COUNT` guard.
        Type GetType() const;

        // X360: read the 8-byte junkyard id at SpawnLocation +0x24. SetupSpawnLocations matches it
        // against mJunkyardId to claim each junkyard's spawn points. Declare-only here (body in the
        // SpawnLocation/TriggerData TU).
        CgsID GetJunkyardId() const;
    };
}
