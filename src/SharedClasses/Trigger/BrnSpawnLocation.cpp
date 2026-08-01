// ===================================================================================
// BrnTrigger::SpawnLocation -- method bodies
//   b5-decomp/src/SharedClasses/Trigger/BrnSpawnLocation.cpp
//
// GetType (X360 0x823547C8). Read the u8 type discriminator muType at SpawnLocation +0x28,
// assert it is a valid enumerator (< E_TYPE_COUNT == 4), and return it as Type. The car-select
// junkyard flow (CarSelectManager::SetupSpawnLocations / SpawnInStartCar / TeleportCurrentVehicle
// / UpdateRequestCarChangeState) calls this to file / seed each spawn point by kind.
// ===================================================================================

#include "SharedClasses/Trigger/BrnSpawnLocation.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CgsDev::Assert Begin/Fire/EndAssert

namespace BrnTrigger
{
// The verbatim X360-baked source path this TU's assert references (rodata aDP4B5MainBurno_547).
// Same string/line the sibling car-select callers assert against (BrnCarSelectManager_CarChange.cpp).
static const char* const KAC_SPAWNLOC_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\sharedclasses\\trigger\\BrnSpawnLocation.h";

// ============================================================================
// GetType (X360 0x823547C8).
// X360: lbz muType(+0x28); cmplwi 4 / blt-skip; assert muType < E_TYPE_COUNT; return lbz muType(+0x28).
// ============================================================================
SpawnLocation::Type SpawnLocation::GetType() const
{
    if (static_cast<u32>(muType) >= static_cast<u32>(E_TYPE_COUNT))   // X360 cmplwi r11,4 / blt
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("muType < E_TYPE_COUNT", KAC_SPAWNLOC_FILE, 152);
        CgsDev::Assert::EndAssert();
    }
    return static_cast<Type>(muType);   // X360 re-loads the same u8 at +0x28
}

// ============================================================================
// GetJunkyardId.
// No standalone symbol exists in the X360 image -- it is an inlined 8-byte load of the CgsID at
// SpawnLocation +0x20 (CarSelectManager::SetupSpawnLocations matches it against mJunkyardId to
// claim each junkyard's spawn points; EnterJunkyardAtStartOfGame then takes maSpawnLocations[1]).
// Kept out-of-line here because BrnSpawnLocation.h declares it that way.
// ============================================================================
CgsID SpawnLocation::GetJunkyardId() const
{
    return mJunkyardId;
}
}
