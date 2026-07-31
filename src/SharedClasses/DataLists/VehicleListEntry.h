#ifndef SHAREDCLASSES_DATALISTS_VEHICLELISTENTRY_H
#define SHAREDCLASSES_DATALISTS_VEHICLELISTENTRY_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID (GetId return)

// VehicleListEntry.h
// Single home of BrnResource::VehicleListEntry, the per-vehicle record inside a
// serialised VehicleListResource (sizeof == 0xF0 / 240, the stride
// GetSerialisedResourceDescriptor @0x8267B540 multiplies the vehicle count by).
//
// The on-disk layout (160-byte opaque header, then the embedded attrib/voice-over
// collision keys with their inter-key padding) is recovered from
// VehicleListResourceType::FixUp @0x8267DD60, which destructs each key in turn. This
// header is the one definition; VehicleListResourceType.cpp includes it rather than
// re-declaring the struct.
//
// ELiveryType (nested enum) is the livery-kind tag used by the per-car derived-livery
// lists -- BrnProgression::DerivedCarArray::ConstructColourLiveryList /
// ConstructPatternLiveryList build an Array<VehicleListEntry::ELiveryType, 8> of the
// livery versions a car supports (X360 Append @0x8235C6A8 stores a 4-byte enum value).
// The enum is 4 bytes (the Append stores with stwx / a word).

namespace CgsSceneManager { namespace CgsCollision
{
    // The embedded attrib/voice-over key handle (8-byte storage + Destruct). Declared here
    // so VehicleListEntry can hold its keys by name; the full collision-generator type lands
    // with its own TU (GROW this forward shape then, do not fork it).
    struct BaseCollisionGenerator
    {
        void Destruct();
        u8   maStorage[8];
    };
}}

namespace BrnResource
{

struct VehicleListEntry
{
    // Livery-kind tag for a derived car's livery list. The X360 stores it as a 4-byte word
    // (Array<ELiveryType,8>::Append @0x8235C6A8 uses stwx). The concrete enumerator names/values
    // are data-driven (not literal in the asm); modelled with the two kinds the derived-livery
    // builders distinguish (colour vs pattern) plus the count sentinel.
    enum ELiveryType
    {
        E_LIVERY_TYPE_COLOUR  = 0,
        E_LIVERY_TYPE_PATTERN = 1,
        E_LIVERY_TYPE_COUNT   = 2,
    };

    // Destruct the embedded collision keys (VehicleListResourceType::FixUp @0x8267DD60).
    void FixUp();

    // ADDITIVE GROW (declare-only; body in the VehicleListEntry/VehicleList TU).
    // The auto-repair capability bit DriveThruManager::HandleDriveThru's body-shop pre-check reads.
    // X360 asm: `lwz r11,0x94(r3); extrwi r11,r11,1,25` == `(word @+0x94 >> 6) & 1` (LSB-numbered
    // bit 6) within the leading opaque record (maPad0). FLAG: the field/bit name is recovered from
    // the HandleDriveThru asm, not from the exports -- named conservatively.
    bool CanAutoRepair() const;

    // ADDITIVE GROW (declare-only; bodies in the VehicleList/VehicleListEntry TU) for the
    // CarSelectManager IsThisCarInCurrentUnlockSequence / DEBUG_UnlockCarsForTesting paths. Read inside
    // the leading opaque header (maPad0/+0x90 gameplay-data region) -- NO layout change; precedent =
    // CanAutoRepair() above reading +0x94 bit6.
    //   GetId()        -> the leading car id the X360 reads at entry+0x00.
    //   IsTrophyCar()  -> X360 `(*(entry+0x94) & 1)` (bit0 of the same flags word CanAutoRepair reads
    //                     bit6 of); true == trophy/special car.
    //   GetUnlockRank()-> X360 byte at entry+0x90+0x09 (the embedded gameplay-data sub-object's
    //                     required-progression-rank field).
    // FLAG: the +0x94 bit0 / +0x99 byte offsets are recovered from the asm, not the exports.
    CgsID GetId() const;
    bool  IsTrophyCar() const;
    u8    GetUnlockRank() const;

    // ADDITIVE GROW (declare-only; body in the VehicleList/VehicleListEntry TU) for the
    // BrnGameState::DeveloperChallengeManager::CheckCarID parent-chain walk. X360 CheckCarID reads
    // the entry's parent-car id and chases it until it matches the target (or runs out). Per the
    // Vehicle-List wiki the parent id sits at entry+0x08 (CgsID mParentId, 0 == no parent).
    CgsID GetParentId() const;

    // ADDITIVE GROW (declare-only; body in the VehicleList/VehicleListEntry TU) for the
    // BrnGui::LeaderboardTableComponent::SetCell car-name path. The livery/"Finish Type" tag
    // the leaderboard uses to decide whether to display a livery variant under its own id or
    // its parent car's id. X360 SetCell reads it as `lbz r11,0xE9(entry)`; the +0xE9 offset is
    // the wiki-named muLiveryType byte (Vehicle List / Burnout Paradise). FLAG: offset recovered
    // from the asm; the field name is taken from the burnout.wiki VehicleListEntry table.
    u8 GetLiveryType() const;

    // ADDITIVE GROW (declare-only; body in the VehicleList/VehicleListEntry TU) for the
    // ChallengeManager keystone (wave C). The car's boost class / car type. X360
    // ChallengeManager::CheckCurrentCar (0x823336E8) reads it as `lbz r11,0xE8(entry)` and maps
    // 0/1/2 onto the challenge car-restriction gate (ChallengeListEntry::ECarRestrictionType
    // DANGER(1)/AGGRESSION(2)/STUNT(3)). The +0xE8 offset is the wiki-named muCarType byte
    // (Vehicle List / Burnout Paradise, this-era layout; enum BrnResource::ECarType
    // E_CARTYPE_DANGER=0 / E_CARTYPE_AGGRESSION=1 / E_CARTYPE_STUNTS=2). FLAG: offset recovered
    // from the asm; the field/enum names are taken from the burnout.wiki VehicleListEntry table
    // (same precedent as GetLiveryType above). Returned as the raw byte.
    u8 GetCarType() const;

    // ADDITIVE GROW (declare-only; body in the VehicleList/VehicleListEntry TU) for the
    // BrnGameState::ResetPlayerDebugComponent change-car menu label. The car's display name
    // C-string. Per the Vehicle-List wiki macVehicleName is the char[64] at entry+0x30 (the X360
    // OnChangeCarFilter streams `entry+0x30` into the menu label). Returns nullptr/empty-safe.
    const char* GetName() const;

    // ADDITIVE GROW (declare-only; body in the VehicleList/VehicleListEntry TU) for the
    // race-car streaming path. The car's default wheel-set NAME -- a char[32] at entry+0x10.
    // X360-attested: RaceCarEntityModule::HandleSelectionRequestStreamingAction @0x822E9918
    // and SpawnRaceCar @0x822FE5D8 both feed `entry + 0x10` straight into
    // WheelList::FindWheelIndexFromName (@0x822CD4D8, which stricmp's against wheel entry+0x08).
    // The burnout.wiki Vehicle-List table names it mDefaultWheelName, char[32] @0x10 --
    // independent agreement, same NAME-ONLY adoption as mExhaustName/mEngineName below.
    const char* GetDefaultWheelName() const;

    // ---- on-disk layout (recovered from FixUp's key destructs); sizeof == 0xF0 (240) ----
    u8 maPad0[160];                                                       // +0x00
    CgsSceneManager::CgsCollision::BaseCollisionGenerator mAttribCollectionKey;        // +0xA0
    // +0xA8 / +0xC0: the first and fourth members of the 0x40-byte mAudioData block
    // (BrnResource::VehicleListEntryAudioData). Both are CgsIDs that decode to an engine
    // asset name -- e.g. the Hunter Cavalry's "DRAG2_EX" and "DRAG2_ENG". Named from two
    // independent sources that agree: GameDataModule::ProcessLoadVehicleRequest @0x8266EB98
    // does `ld r3, 0xA8(entry)` and ProcessGetVehicleRequest @0x8266FDA0 does
    // `ld r3, 0xC0(entry)` (both feeding "Engines\%08x.bundle"), and the burnout.wiki
    // Vehicle List page anchors mAudioData at +0xA8 with mExhaustName@+0x00 /
    // mEngineName@+0x18. NAME-ONLY adoption: the surrounding widths stay as recovered.
    CgsID mExhaustName;                                                   // +0xA8
    CgsSceneManager::CgsCollision::BaseCollisionGenerator mExhaustEntityKey;           // +0xB0
    CgsSceneManager::CgsCollision::BaseCollisionGenerator mEngineEntityKey;            // +0xB8
    CgsID mEngineName;                                                    // +0xC0
    u8 maPad200[8];                                                       // +0xC8
    CgsSceneManager::CgsCollision::BaseCollisionGenerator mWonCarVoiceOverKey;         // +0xD0
    CgsSceneManager::CgsCollision::BaseCollisionGenerator mRivalReleasedVoiceOverKey;  // +0xD8
    u8 maPad224[16];                                                      // +0xE0..0xEF
};

} // namespace BrnResource

#endif // SHAREDCLASSES_DATALISTS_VEHICLELISTENTRY_H
