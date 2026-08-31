// VehicleListEntry.cpp
// BrnResource::VehicleListEntry -- the accessors VehicleListEntry.h declares.
//
// The record is a SERIALISED image (sizeof == 0xF0 / 240, the stride
// VehicleListResourceType::GetSerialisedResourceDescriptor @0x8267B540 multiplies the
// vehicle count by), so every field position below is a property of the shipped data,
// not of a host struct: each accessor reads inside the named opaque region declared in
// the header (maPad0 / maPad224) at the offset its own X360 reader uses. The offsets
// carried here are exactly the ones VehicleListEntry.h already documents per accessor;
// this TU only supplies the bodies those declarations promised.
//
// Attestation for each offset (all recorded in the header, repeated at the body):
//   +0x00 CgsID mId          -- VehicleList::GetVehicleIndex @0x822188C8 (`ld r11,0(r3)`)
//   +0x08 CgsID mParentId    -- DeveloperChallengeManager::CheckCarID parent-chain walk
//   +0x30 char macVehicleName[64] -- ResetPlayerDebugComponent::OnChangeCarFilter (`entry+0x30`)
//   +0x94 u32   flags        -- DriveThruManager::HandleDriveThru (`lwz r11,0x94(r3)`)
//                               bit0 = trophy car, bit6 = can auto-repair
//   +0x99 u8    unlock rank  -- CarSelectManager::IsThisCarInCurrentUnlockSequence
//   +0xE8 u8    car type     -- ChallengeManager::CheckCurrentCar (`lbz r11,0xE8(entry)`)
//   +0xE9 u8    livery type  -- LeaderboardTableComponent::SetCell (`lbz r11,0xE9(entry)`)
//
// x64 note: the record is read straight out of the streamed bundle payload, so the reads
// are done through memcpy into a host-typed local rather than by casting an interior
// pointer -- same value, no alignment assumption.

#include "SharedClasses/DataLists/VehicleListEntry.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysCollectionKey.h" // CgsAttribSys::AttribSysCollectionKey (GetAttribCollectionKeyHash)

#include <cstring>   // memcpy

namespace BrnResource
{

namespace
{
    // Offsets INSIDE the serialised record's leading opaque header (maPad0, +0x00..0x9F).
    const u32 KU_OFFSET_ID           = 0x00;
    const u32 KU_OFFSET_PARENT_ID    = 0x08;
    const u32 KU_OFFSET_WHEEL_NAME   = 0x10;
    const u32 KU_OFFSET_VEHICLE_NAME = 0x30;
    const u32 KU_OFFSET_FLAGS        = 0x94;
    const u32 KU_OFFSET_UNLOCK_RANK  = 0x99;

    // Offsets inside the trailing opaque region (maPad224, +0xE0..0xEF).
    const u32 KU_OFFSET_CAR_TYPE     = 0xE8 - 0xE0;
    const u32 KU_OFFSET_LIVERY_TYPE  = 0xE9 - 0xE0;
    // The two car-select gauge ratings (wiki muTopSpeedNormalGUIStat / muTopSpeedBoostGUIStat).
    const u32 KU_OFFSET_SPEED_STAT   = 0xEC - 0xE0;
    const u32 KU_OFFSET_BOOST_STAT   = 0xED - 0xE0;
    const u32 KU_OFFSET_DEF_COLOUR   = 0xEE - 0xE0;
    const u32 KU_OFFSET_DEF_FINISH   = 0xEF - 0xE0;

    // The two flag bits the X360 readers extract from the +0x94 word.
    const u32 KU_FLAG_TROPHY_CAR     = 1u << 0;   // `extrwi r11,r11,1,31` (bit 0)
    const u32 KU_FLAG_CAN_AUTOREPAIR = 1u << 6;   // `extrwi r11,r11,1,25` (bit 6)

    u64 ReadU64(const u8* lpBytes)
    {
        u64 luValue = 0;
        std::memcpy(&luValue, lpBytes, sizeof(luValue));
        return luValue;
    }

    u32 ReadU32(const u8* lpBytes)
    {
        u32 luValue = 0;
        std::memcpy(&luValue, lpBytes, sizeof(luValue));
        return luValue;
    }
}

// The leading car id (X360 reads it at entry+0x00 as an 8-byte load).
CgsID VehicleListEntry::GetId() const
{
    return static_cast<CgsID>(ReadU64(&maPad0[KU_OFFSET_ID]));
}

// The parent/derived-from car id (0 == no parent).
CgsID VehicleListEntry::GetParentId() const
{
    return static_cast<CgsID>(ReadU64(&maPad0[KU_OFFSET_PARENT_ID]));
}

// The display name C-string (macVehicleName, char[64] at +0x30).
const char* VehicleListEntry::GetName() const
{
    return reinterpret_cast<const char*>(&maPad0[KU_OFFSET_VEHICLE_NAME]);
}

// The default wheel-set name C-string (mDefaultWheelName, char[32] at +0x10) -- what the
// streaming path hands to WheelList::FindWheelIndexFromName.
const char* VehicleListEntry::GetDefaultWheelName() const
{
    return reinterpret_cast<const char*>(&maPad0[KU_OFFSET_WHEEL_NAME]);
}

// X360 `lwz r11,0x94(r3); extrwi r11,r11,1,31` -- bit 0 of the +0x94 flags word.
bool VehicleListEntry::IsTrophyCar() const
{
    return (ReadU32(&maPad0[KU_OFFSET_FLAGS]) & KU_FLAG_TROPHY_CAR) != 0;
}

// X360 `lwz r11,0x94(r3); extrwi r11,r11,1,25` -- bit 6 of the same flags word.
bool VehicleListEntry::CanAutoRepair() const
{
    return (ReadU32(&maPad0[KU_OFFSET_FLAGS]) & KU_FLAG_CAN_AUTOREPAIR) != 0;
}

// The required-progression-rank byte inside the gameplay-data sub-object (+0x99).
u8 VehicleListEntry::GetUnlockRank() const
{
    return maPad0[KU_OFFSET_UNLOCK_RANK];
}

// The car's boost class (+0xE8) -- raw byte, mapped by the caller.
u8 VehicleListEntry::GetCarType() const
{
    return mu8CarType;
}

// The livery/"Finish Type" tag (+0xE9) -- raw byte, mapped by the caller.
u8 VehicleListEntry::GetLiveryType() const
{
    return mu8LiveryType;
}

// The car-select speed gauge rating (+0xEC) -- raw byte, scaled by the caller's stats bar.
u8 VehicleListEntry::GetSpeedStat() const
{
    return mu8TopSpeedNormalGUIStat;
}

// The car-select boost gauge rating (+0xED) -- raw byte.
u8 VehicleListEntry::GetBoostStat() const
{
    return mu8TopSpeedBoostGUIStat;
}

// The car's FACTORY paint colour index (+0xEE) -- raw byte. See the header note: read by
// BrnGui::CarSelectLivery::UpdateComponents (as BYTE2 of the big-endian word at +0xEC) and
// by its HandleControllerInput restore arm (as *(entry + 238)).
u8 VehicleListEntry::GetDefaultPaintColour() const
{
    return mu8DefaultPaintColour;
}

// The car's FACTORY paint FINISH index (+0xEF, a BrnWorld::EPalettesTypes value) -- raw byte.
// Read by the same two CarSelectLivery sites (LOBYTE of +0xEC / *(entry + 239)).
u8 VehicleListEntry::GetDefaultPaintFinish() const
{
    return mu8DefaultPaintFinish;
}

u64 VehicleListEntry::GetExhaustKey() const
{
    CgsAttribSys::AttribSysCollectionKey lKey;
    std::memcpy(&lKey, mExhaustEntityKey.maStorage, sizeof(lKey));
    return lKey.GetHashKey();
}

u64 VehicleListEntry::GetEngineKey() const
{
    CgsAttribSys::AttribSysCollectionKey lKey;
    std::memcpy(&lKey, mEngineEntityKey.maStorage, sizeof(lKey));
    return lKey.GetHashKey();
}


// The car's ATTRIBSYS COLLECTION KEY, hashed (+0xA0). X360-attested by
// RaceCarEntityModule::SpawnRaceCar @0x822FE5D8, which does `addi r3, entry, 0xA0` then
// `bl CgsAttribSys::AttribSysCollectionKey::GetHashKey` -- the eight bytes at +0xA0 are that
// type's single s64 miAssetGuid. See the header for why the member itself still carries the
// BaseCollisionGenerator forward shape (FixUp destructs it under that name).
u64 VehicleListEntry::GetAttribCollectionKeyHash() const
{
    CgsAttribSys::AttribSysCollectionKey lKey;
    std::memcpy(&lKey, &mAttribCollectionKey, sizeof(lKey));
    return lKey.GetHashKey();
}

} // namespace BrnResource
