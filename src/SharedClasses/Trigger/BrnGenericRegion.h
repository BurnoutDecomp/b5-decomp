// SharedClasses/Trigger/BrnGenericRegion.h  (NEW owning header)
//
// Owning header for BrnTrigger::GenericRegion -- a TriggerRegion-derived trigger
// box that carries a 32-value region "type" (junk yard, gas station, stunt
// enclosures, sound enclosures, ...) plus stunt-camera / group / one-way info.
//
// SHAPE is gated on the X360 DecFIGS DWARF
// (references/DecFIGS/dwarfdump/SharedClasses/Trigger/BrnGenericRegion.h), which
// is authoritative: GenericRegion : public TriggerRegion, then (after the 44-byte
// TriggerRegion subobject ending at 0x2C) the seven members below, laid out
// contiguously with no gaps -- meType lands at offset 0x36 == 54, exactly the
// `*(a1 + 54)` byte read in the X360 GetTypeName pseudocode @ 0x82354760.
//
// This header replaces the MINIMAL STUB struct currently in
// GameSource/GameState/Offences/BrnDriveThruManager.h (which declared only
// GenericRegion::Type values 0..4). INTEGRATOR: delete that stub and have
// BrnDriveThruManager.h include this header instead; the enum here is a superset
// (E_TYPE_JUNK_YARD..E_TYPE_CAR_PARK keep the same 0..4 values, so the existing
// DriveThruManager switch is unaffected).
//
// Methods other than GetTypeName() are declaration-only: each has (or will have)
// its own X360 TU (Construct/FixDown/FixUp) or is an inlined accessor whose body
// is not reached by this TU's reconstruction goal.
//
// Declared `struct GenericRegion` to match the DWARF, the `struct TriggerRegion`
// base, and the three committed `struct GenericRegion` forward declarations
// (BrnKillzone.h, BrnTriggerData.h, BrnStuntManagerDebugComponent.h) -- avoids a
// C4099 struct/class tag mismatch.

#ifndef BRN_GENERIC_REGION_H
#define BRN_GENERIC_REGION_H

#include "types.hpp"                                  // fixed-width ints
#include "BrnCommonTypes.h"                           // CgsID
#include "SharedClasses/Trigger/BrnTriggerBase.h"     // TriggerRegion (base) + BoxRegion

namespace BrnTrigger
{

struct GenericRegion : public TriggerRegion
{
public:
    // BrnGenericRegion.h:49 (DWARF) -- the GenericRegion-specific region category.
    // 32 values; meType (offset 0x36) stores one of these. E_TYPE_COUNT == 0x20,
    // which is exactly the `>= 0x20u` bound the X360 GetTypeName asserts against.
    enum Type
    {
        E_TYPE_JUNK_YARD                = 0,
        E_TYPE_GAS_STATION              = 1,
        E_TYPE_BODY_SHOP                = 2,
        E_TYPE_PAINT_SHOP               = 3,
        E_TYPE_CAR_PARK                 = 4,
        E_TYPE_SIGNATURE_TAKEDOWN       = 5,
        E_TYPE_KILLZONE                 = 6,
        E_TYPE_JUMP                     = 7,
        E_TYPE_SMASH                    = 8,
        E_TYPE_SIGNATURE_CRASH          = 9,
        E_TYPE_SIGNATURE_CRASH_CAMERA   = 10,
        E_TYPE_ROAD_LIMIT               = 11,
        E_TYPE_OVERDRIVE_BOOST          = 12,
        E_TYPE_OVERDRIVE_STRENGTH       = 13,
        E_TYPE_OVERDRIVE_SPEED          = 14,
        E_TYPE_OVERDRIVE_CONTROL        = 15,
        E_TYPE_TIRE_SHOP                = 16,
        E_TYPE_TUNING_SHOP              = 17,
        E_TYPE_PICTURE_PARADISE         = 18,
        E_TYPE_TUNNEL                   = 19,
        E_TYPE_OVERPASS                 = 20,
        E_TYPE_BRIDGE                   = 21,
        E_TYPE_WAREHOUSE                = 22,
        E_TYPE_LARGE_OVERHEAD_OBJECT    = 23,
        E_TYPE_NARROW_ALLEY             = 24,
        E_TYPE_PASS_TUNNEL              = 25,
        E_TYPE_PASS_OVERPASS            = 26,
        E_TYPE_PASS_BRIDGE              = 27,
        E_TYPE_PASS_WAREHOUSE           = 28,
        E_TYPE_PASS_LARGEOVERHEADOBJECT = 29,
        E_TYPE_PASS_NARROWALLEY         = 30,
        E_TYPE_RAMP                     = 31,

        E_TYPE_COUNT                    = 32
    };

    // BrnGenericRegion.h:92 (DWARF)
    enum StuntCameraType
    {
        E_STUNT_CAMERA_TYPE_NO_CUTS = 0,
        E_STUNT_CAMERA_TYPE_CUSTOM  = 1,
        E_STUNT_CAMERA_TYPE_NORMAL  = 2,
    };

    // BrnGenericRegion.h:99 (DWARF): static const char* [E_TYPE_COUNT] type-name
    // table. This is `off_820A3A18` in the X360 image -- the array GetTypeName()
    // indexes with meType. Definition lives in BrnGenericRegion.cpp.
    static const char* KAPC_GENERIC_REGION_TYPE_STRINGS[E_TYPE_COUNT];

    // BrnGenericRegion.h:111 -- own TU; declaration only.
    void Construct( CgsID lId, Type leType, const BoxRegion* lpBoxRegion, CgsID lGroupId,
                    int16_t liCameraCut1, int16_t liCameraCut2,
                    int8_t leCameraType1, int8_t leCameraType2, bool lbOneWay );

    // BrnGenericRegion.h:114 -- inlined accessor in X360 build. Now given its inline body: it is
    // the `lbz r9, 0x36(region)` that GameStateModule::FindNearestJunkyardID @0x8236BB84 does to
    // keep only E_TYPE_JUNK_YARD (0) regions. Same read, no offset poke; still no out-of-line
    // symbol, exactly as the console has none.
    Type            GetType() const { return static_cast<Type>( meType ); }

    // BrnGenericRegion.h:117 / body @ :231 -- THIS TU (X360 0x82354760).
    const char*     GetTypeName() const;

    // BrnGenericRegion.h:120-138 -- inlined accessors.
    //
    // [gateui] 2026-08-20: the six below were declaration-only and were LINK-DEAD
    // (`GetGroupId` / `GetCameraCut1` / `GetCameraCut2` / `GetCameraType1` /
    // `GetCameraType2` were measured as UNDEF externals in BrnStuntManager.obj,
    // StuntManager_gUI_00.obj and BrnTriggerQueryManager.obj). They are given their
    // inline bodies here, which is what the console has: NONE of them owns a
    // standalone X360 symbol (`progress/identity.json` lists only
    // `GenericRegion::GetTypeName` @0x82354760 for this class), i.e. every one is a
    // header-inlined member-read in the original, exactly like the `GetType()`
    // above. Each reads its named member -- no offset poke.
    //
    // OFFSET ATTESTATION (why these members and not neighbours): the TriggerRegion
    // base subobject is 44 bytes (BoxRegion 36 + mId 4 + miRegionIndex 2 + meType 1
    // + 1 pad), so miGroupID lands at +0x2C and meType at +0x36 -- and the X360
    // reads confirm both: `lbz r11, 0x36(region)` is the sub-type test in
    // StuntManager::OnPropHit @0x8236EE18 (8 -> SMASH, 12 -> BILLBOARD) and
    // ProcessStuntElement @0x8239CDB0 keys the stunt element on the +0x2C word,
    // falling back to TriggerRegion::GetId() (+0x24) when it is zero.
    bool            IsDriveThru() const;   // still declaration-only -- see the FLAG below

    // The stunt-element KEY. X360 ProcessStuntElement @0x8239CDB0 does
    // `lwz r11, 0x2C(region); extsw r11, r11` -- a SIGN-EXTENDING word read into the
    // 64-bit CgsID. `static_cast<CgsID>(int32_t)` reproduces that exactly (the
    // int32 -> uint64 conversion is 2's-complement, i.e. sign extension), and it is
    // the same idiom TriggerRegion::GetId() already uses for its own int32 storage.
    CgsID           GetGroupId() const     { return static_cast<CgsID>( miGroupID ); }

    // Stunt-camera cut indices. DWARF returns int32_t over int16_t storage, so the
    // console read is the sign-extending `lha`; the implicit int16 -> int32
    // promotion below is that sign extension. StuntManager::UpdateJumps
    // (BrnStuntManager.cpp :: UpdateJumps) compares `GetCameraCut1() > 0`, which is
    // only meaningful signed.
    int32_t         GetCameraCut1() const  { return miCameraCut1; }
    int32_t         GetCameraCut2() const  { return miCameraCut2; }

    StuntCameraType GetCameraType1() const { return static_cast<StuntCameraType>( miCameraType1 ); }
    StuntCameraType GetCameraType2() const { return static_cast<StuntCameraType>( miCameraType2 ); }

    bool            IsOneWay() const       { return miIsOneWay != 0; }

    // BrnGenericRegion.h:141-144 -- own TUs (FixDown @ BrnGenericRegion.cpp:89);
    // declaration only.
    void            FixDown();
    void            FixUp();

    // FLAG [gateui]: `IsDriveThru()` is deliberately LEFT declaration-only. Unlike the
    // six above it is not a member read -- the console tests meType against the
    // drive-thru type SET, and that set is not attested by anything in this wave's
    // evidence (the only consumer, DriveThruManager::ProcessDriveThru @0x8239B6E8,
    // is an unmounted TU and inlines the test inside a 1300-line body). Guessing
    // "meType <= E_TYPE_CAR_PARK" would be a fabrication with a live behavioural
    // consequence (every junkyard/gas-station/body-shop/paint-shop/car-park trigger
    // gates on it), so it is parked for the owner of the DriveThruManager TU.

private:
    // Layout (DWARF, after the 44-byte TriggerRegion base subobject @ 0x00..0x2B):
    int32_t miGroupID;       // 0x2C  BrnGenericRegion.h:148
    int16_t miCameraCut1;    // 0x30  BrnGenericRegion.h:149
    int16_t miCameraCut2;    // 0x32  BrnGenericRegion.h:150
    int8_t  miCameraType1;   // 0x34  BrnGenericRegion.h:151  (stores StuntCameraType)
    int8_t  miCameraType2;   // 0x35  BrnGenericRegion.h:152  (stores StuntCameraType)
    uint8_t meType;          // 0x36  BrnGenericRegion.h:153  (stores Type) == *(a1+54)
    int8_t  miIsOneWay;      // 0x37  BrnGenericRegion.h:154
};

// BrnGenericRegion.h:160-161 (DWARF): sound-enclosure type range (E_TYPE_TUNNEL
// .. E_TYPE_RAMP). At NAMESPACE scope per DWARF (lines 51-56 / 129-135), so a
// future caller written as BrnTrigger::KI_FIRST_SOUND_ENCLOSURE resolves.
const int32_t KI_FIRST_SOUND_ENCLOSURE = 19;   // E_TYPE_TUNNEL
const int32_t KI_LAST_SOUND_ENCLOSURE  = 31;   // E_TYPE_RAMP

} // namespace BrnTrigger

#endif // BRN_GENERIC_REGION_H
