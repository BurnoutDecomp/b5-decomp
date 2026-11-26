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
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT_AT (GetType's range assert)

namespace BrnTrigger
{
    struct SpawnLocation
    {
        // DWARF: the spawn-location kind discriminator (references/DecFIGS/dwarfdump/.../
        // BrnSpawnLocation.h). E_TYPE_COUNT (4) is the sentinel GetType and the car-select
        // callers assert against.
        enum Type
        {
            E_TYPE_PLAYER_SPAWN     = 0,
            E_TYPE_CAR_SELECT_LEFT  = 1,
            E_TYPE_CAR_SELECT_RIGHT = 2,
            E_TYPE_CAR_UNLOCK       = 3,
            E_TYPE_COUNT            = 4,   // BrnSpawnLocation.h:152 assert bound
        };

        // DWARF h:66..75: the accessors are defined in this header. ARTIST inlines them (the
        // car-select callers read +0x28 and assert at BrnSpawnLocation.h:152 in line, e.g.
        // CarSelectManager::EnterJunkyardAtStartOfGame 0x823930D8); the one out-of-line GetType,
        // 0x823547C8, is the copy a caller that did not inline it emitted.
        Vector3 GetPosition() const  { return mPosition; }
        Vector3 GetDirection() const { return mDirection; }
        CgsID GetJunkyardId() const  { return mJunkyardId; }
        Type GetType() const
        {
            CGS_ASSERT_AT(muType < E_TYPE_COUNT, "muType < E_TYPE_COUNT",
                          "d:\\p4\\b5_main\\burnout\\main\\code\\sharedclasses\\trigger\\BrnSpawnLocation.h", 152);
            return static_cast<Type>(muType);
        }

        // --- layout (DWARF-named; muType pinned by the X360 lbz at +0x28) --------------
        Vector3 mPosition;    // +0x00 (16B slot)
        Vector3 mDirection;   // +0x10 (16B slot)
        CgsID   mJunkyardId;  // +0x20 (8B)
        u8      muType;       // +0x28 the Type discriminator GetType reads
    };
}
