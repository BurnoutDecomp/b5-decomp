#ifndef SHAREDCLASSES_DATALISTS_VEHICLELISTENTRY_H
#define SHAREDCLASSES_DATALISTS_VEHICLELISTENTRY_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID (GetId return)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h" // Attribute::Key

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

// DecFIGS VehicleListEntry.h:44; ARTIST HandleCarStatsUpdate @0x822A4700
// compares the word-valued action member against 0/1/2 and maps it to boost
// strategies 2/3/5 respectively.
enum ECarType : int
{
    E_CARTYPE_DANGER     = 0,
    E_CARTYPE_AGGRESSION = 1,
    E_CARTYPE_STUNTS     = 2,
    E_CARTYPE_COUNT      = 3,
    E_CARTYPE_INVALID    = E_CARTYPE_COUNT
};

struct VehicleListEntry
{
    // Livery-kind tag for a derived car's livery list (the muLiveryType byte @+0xE9,
    // widened to a 4-byte word in the livery arrays -- Array<ELiveryType,8>::Append
    // @0x8235C6A8 uses stwx).
    // ⭐ [map arm 2026-08-27] VALUES CORRECTED against the three X360 readers (the old
    // colour=0/pattern=1 model was flagged wrong by BrnDerivedCars.h's own tail note):
    //     ConstructColourLiveryList  @0x82374F60: colour set == kind ∈ {1, 3, 4}
    //     ConstructPatternLiveryList @0x823751C0: pattern     == kind == 2
    //     ProgressionManager::AddCar @0x8237A970: kind == 4   == the "silver" cars
    // 0 is the no-livery base car (never matched by any builder). FLAG: the value-1 and
    // value-3 names are role-named only ("colour kinds" is all the asm attests -- which
    // is paint vs which is a colour variant is not recovered); 2 and 4 carry attested
    // roles; the enum has no attested COUNT sentinel, so none is fabricated.
    enum ELiveryType
    {
        E_LIVERY_TYPE_NONE       = 0,   // FLAG role-named: the base (non-livery) car
        E_LIVERY_TYPE_COLOUR     = 1,   // FLAG role-named: colour-set member
        E_LIVERY_TYPE_PATTERN    = 2,   // the pattern builders' kind
        E_LIVERY_TYPE_COLOUR_ALT = 3,   // FLAG role-named: colour-set member
        E_LIVERY_TYPE_SILVER     = 4,   // colour-set member; AddCar's ==4 "silver" arm
    };

    // [map arm 2026-08-27] the colour-set predicate both livery builders test (and whose
    // negation the pattern builder asserts by name: "!lpVehicleListEntry->IsLiveryColour()",
    // BrnDerivedCars.h:164). Written as the SET the compiler emitted ({1, 3, 4}), not a
    // range compare -- the GenericRegion::IsDriveThru precedent: the set and any range
    // coincide only while the enumerators happen to be contiguous.
    bool IsLiveryColour() const
    {
        const u8 lu8Kind = GetLiveryType();
        return lu8Kind == E_LIVERY_TYPE_COLOUR
            || lu8Kind == E_LIVERY_TYPE_COLOUR_ALT
            || lu8Kind == E_LIVERY_TYPE_SILVER;
    }

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

    // ADDITIVE GROW (reset-player-car wave 2026-08-01). The car's ATTRIBSYS COLLECTION KEY,
    // hashed. X360-attested: RaceCarEntityModule::SpawnRaceCar @0x822FE5D8 does
    //     addi r3, entry, 0xA0
    //     bl   CgsAttribSys::AttribSysCollectionKey::GetHashKey
    // -- so the eight bytes at +0xA0 ARE a CgsAttribSys::AttribSysCollectionKey (DWARF
    // {s64 miAssetGuid}); this header's local 8-byte BaseCollisionGenerator forward shape is
    // the SAME storage under the name VehicleListResourceType::FixUp's destruct chain gave it.
    // The console has NO accessor symbol (it inlines the address-of + call at each site); this
    // one exists so callers do not reinterpret_cast the member by hand.
    // FLAG: PC-only accessor, name provisional. Retire it when the +0xA0 member is retyped to
    // AttribSysCollectionKey (blocked today: that type's Destruct() is declaration-only, and
    // VehicleListResourceType::FixUp calls Destruct on this member).
    // WIDENED to 64 bits 2026-08-01 (physics wave 1) -- see CgsAttribSysCollectionKey.cpp.
    u64 GetAttribCollectionKeyHash() const;

    // ADDITIVE GROW (drivable wave 2026-08-01). The car's STRENGTH RATING byte at +0x9B.
    // X360-attested: RaceCarEntityModule::ResetActiveRaceCar @0x822F4880 does
    // `lbz r26, 0x9B(lpVehicleListEntry)` and forwards it to ActiveRaceCar::AddHandlingModel,
    // which passes it to VehicleInputInterface::CreateRaceCar as liCarStrengthStat. The
    // burnout.wiki Vehicle-List page independently names +0x9B "Strength Rating" -- it is the
    // last byte of the 12-byte mGamePlayData block at +0x90 (GamePlayData +0xB). NAME-ONLY
    // adoption per the wiki rule; the width (u8) is the one the asm loads.
    u8 GetStrengthStat() const { return mu8StrengthRating; }

    // ADDITIVE GROW (car-select carousel wave 2026-08-02). The two GUI GAUGE ratings the
    // car-select stats bars display. X360-attested: BrnGui::CarSelectVehicle::
    // SetupStatsComponent @0x824C1200 pushes, in this exact order,
    //     mSpeedStatsBar.SetCar(*(entry + 0xEC), colour)
    //     mBoostStatsBar.SetCar(*(entry + 0xED), colour)
    //     mStrengthStatsBar.SetCar(*(entry + 0x9B), colour)
    // (`lbz r4, 0xEC(r3)` / `lbz r4, 0xED(r11)`, with +0x9B already named GetStrengthStat
    // above). The burnout.wiki Vehicle List page independently names +0xEC
    // muTopSpeedNormalGUIStat ("Speed Rating") and +0xED muTopSpeedBoostGUIStat ("Boost
    // Rating") -- two independent sources agreeing on both the offsets and the roles.
    // NAME-ONLY adoption per the wiki rule; the width (u8) is the one the asm loads. Both
    // bytes live in the recovered maPad224 span, exactly like GetCarType (+0xE8) and
    // GetLiveryType (+0xE9) above.
    u8 GetSpeedStat() const;
    u8 GetBoostStat() const;

    // ADDITIVE GROW (CarSelectLivery wave 2026-08-02). The car's FACTORY paint pair -- the
    // colour index at +0xEE and the paint-finish (BrnWorld::EPalettesTypes) index at +0xEF.
    // X360-attested twice in BrnGui::CarSelectLivery:
    //   UpdateComponents @0x824C7CB0 gates the "$GENERAL_OPTION_RESTORE" help item on
    //     `paintFinishToggle.miHighlightedIndex == LOBYTE(entry[59])` and
    //     `colourPicker.miHighlightedIndex == BYTE2(entry[59])` -- entry[59] is the BIG-ENDIAN
    //     word at +0xEC, so LOBYTE is +0xEF and BYTE2 is +0xEE;
    //   HandleControllerInput @0x824D6D10 case 0x34 (the RESTORE press) reads the same two
    //     bytes as `*(entry + 239)` and `*(entry + 238)` and re-seats both toggles onto them.
    // Two independent sites, the same two offsets, and the restore semantics name the roles.
    // FLAG: offsets recovered from the asm; the burnout.wiki Vehicle List table does not name
    // this pair, so the names are role-derived. Both live in the recovered maPad224 span,
    // exactly like GetCarType (+0xE8) / GetLiveryType (+0xE9) / GetSpeedStat (+0xEC).
    u8 GetDefaultPaintColour() const;
    u8 GetDefaultPaintFinish() const;

    // BrnResource::VehicleListEntryAudioData (DWARF h:154..164).  The player
    // vehicle path consumes the component ids/keys directly; the AI manager
    // uses the three exhaust choices when assigning the six shared AI voices.
    CgsID GetExhaustName() const { return mExhaustName; }
    CgsID GetEngineName() const { return mEngineName; }
    u64 GetExhaustKey() const;
    u64 GetEngineKey() const;
    u8 GetAIExhaustIndex() const { return muiAIExhaustIndex; }
    u8 GetAIExhaustIndex2ndPick() const { return muiAIExhaustIndex2ndPick; }
    u8 GetAIExhaustIndex3rdPick() const { return muiAIExhaustIndex3rdPick; }
    u32 GetAIMusicLoop() const { return muiMusicLoopContentSpec; }

    // ---- on-disk layout (recovered from FixUp's key destructs); sizeof == 0xF0 (240) ----
    u8 maPad0[0x9B];                                                      // +0x00
    u8 mu8StrengthRating;                                                 // +0x9B
    u8 maPad9C[0xA0 - 0x9C];                                              // +0x9C (padding)
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
    u32 muiMusicLoopContentSpec;                                          // +0xE0
    u8  muiAIExhaustIndex;                                                // +0xE4
    u8  muiAIExhaustIndex2ndPick;                                         // +0xE5
    u8  muiAIExhaustIndex3rdPick;                                         // +0xE6
    u8  mu8AudioPad;                                                       // +0xE7
    u8  mu8CarType;                                                        // +0xE8
    u8  mu8LiveryType;                                                     // +0xE9
    u8  mau8TailPad[2];                                                    // +0xEA
    u8  mu8TopSpeedNormalGUIStat;                                          // +0xEC
    u8  mu8TopSpeedBoostGUIStat;                                           // +0xED
    u8  mu8DefaultPaintColour;                                             // +0xEE
    u8  mu8DefaultPaintFinish;                                             // +0xEF
};

} // namespace BrnResource

#endif // SHAREDCLASSES_DATALISTS_VEHICLELISTENTRY_H
