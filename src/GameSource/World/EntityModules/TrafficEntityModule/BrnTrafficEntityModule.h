#pragma once

// =============================================================================
// BrnTrafficEntityModule.h  (NEW OWNING HEADER -- partial: element-type home)
//
// DWARF home (references/DecFIGS/dwarfdump/GameSource/World/EntityModules/
// TrafficEntityModule/BrnTrafficEntityModule.h) of the BrnTraffic entity-module
// value types. This slice owns ONLY the two small per-frame record types that the
// module keeps in fixed-capacity Array<> collections (the Array<T,N> instantiation
// .cpps live alongside this header):
//
//   BrnTraffic::TrafficCrashInfo    (struct @ BrnTrafficEntityModule.h:129)
//       -> Array<TrafficCrashInfo,160>::Erase @ 0x8270B230, ::GetI @ 0x8270CF08
//   BrnTraffic::FiredKillZoneInfo   (struct @ BrnTrafficEntityModule.h:240)
//       -> Array<FiredKillZoneInfo,8>::Append @ 0x8270B548, ::Erase @ 0x8270B670
//
// The huge remainder of BrnTrafficEntityModule (the module class, its sibling
// record types, the IO interfaces) belongs to other not-yet-reconstructed slices;
// when they land they should GROW this header additively, never redefine these two.
//
// Element sizes are X360-authoritative:
//   * TrafficCrashInfo == 16 bytes: Array<TrafficCrashInfo,160>::Erase @ 0x8270B230
//     reads the live count at byte +0xA00 == 160 * 16, and shifts each element with a
//     16-byte stride (`slwi r11,r31,4`) copying 4 dwords; ::GetI @ 0x8270CF08 returns
//     16*index + base.
//   * FiredKillZoneInfo == 16 bytes: Array<FiredKillZoneInfo,8>::Append @ 0x8270B548
//     reads the live count at byte +0x80 == 8 * 16, and copies each element as two
//     qwords (`ld/std` x2) at a 16-byte stride (`slwi r11,r11,4`).
// =============================================================================

#include "types.hpp"        // u8/u32/s32/u64
#include "BrnCommonTypes.h" // EntityId, CgsID
#include "GameShared/GameClasses/Graphics/CgsModel.h" // CgsGraphics::Model::State (VehicleRenderInfo::mLOD)

namespace BrnTraffic
{
    // BrnTrafficEntityModule.h:129 -- one pending traffic-crash record. sizeof == 16
    // (X360-authoritative: Array<TrafficCrashInfo,160> count word sits at +0xA00 == 160*16,
    // and the per-element copy moves 4 dwords / a 16-byte stride).
    //
    // meCrashTrafficType is BrnPhysics::Vehicle::eCrashTrafficType
    // (BrnTrafficPhysicsConstants.h:32). That enum's home is not reconstructed in this
    // slice, so the field is stored here as its underlying 32-bit value (the X360 packs
    // the enum in a 4-byte slot); the enumerator names are documented for reference:
    //   Standard=0, Checked=1, Spontaneous=2, Slammed=3, Invalid=255.
    struct TrafficCrashInfo
    {
        EntityId mVictimId;                   // :123  +0x00
        EntityId mCauserId;                   // :124  +0x04
        u32      muCrashTrafficType;          // :125  +0x08  (eCrashTrafficType, 4-byte)
        bool     mbNeedsToBeSentToCrashModule;// :126  +0x0C  (+3 trailing pad -> 16)
    };

    // BrnTrafficEntityModule.h:240 -- a kill-zone the module fired and must remember for
    // a few frames. sizeof == 16 (X360-authoritative: Array<FiredKillZoneInfo,8> count word
    // sits at +0x80 == 8*16, and the per-element copy moves two qwords / a 16-byte stride).
    // mKillZoneId is TrafficData::KillZoneId == uint64_t (BrnTrafficData.h:40); its 8-byte
    // width + the trailing int32 + pad gives the proven 16-byte footprint.
    struct FiredKillZoneInfo
    {
        u64 mKillZoneId;            // :241  +0x00  (TrafficData::KillZoneId == uint64_t)
        s32 miFramesLeftToRemember; // :242  +0x08  (+4 trailing pad -> 16)
    };

    // DWARF home BrnTrafficMiscRuntimeClasses.h:94 -- one purgatory-list record: a vehicle
    // index plus a countdown of decision frames left. sizeof == 4 (X360-authoritative: the
    // Array<PurgatoryInfo,N> instantiations put their live-count word at byte N*4 and copy
    // each element as two halfwords -- Array<PurgatoryInfo,1>::Append @ 0x8270AAC0 stores
    // `sth muIndex@+0` then `sth muDecisionFramesLeft@+2`, count word @ +0x4 == 1*4;
    // Array<PurgatoryInfo,400> count word @ +0x640 == 400*4 (Erase @ 0x8270A770);
    // Array<PurgatoryInfo,1>::GetItem @ 0x8270CA28 returns 4*index + base). Homed here with
    // the module's other small record types; grow this header (never redefine) when the
    // BrnTrafficMiscRuntimeClasses slice lands.
    struct PurgatoryInfo
    {
        u16 muIndex;              // :96  +0x00
        u16 muDecisionFramesLeft; // :97  +0x02  (-> 4)
    };

    // DWARF home BrnTrafficVehicle.h:159 -- one per-frame vehicle-render record (the dispatch
    // list the module hands the renderer). sizeof == 12 (X360-authoritative:
    // Array<VehicleRenderInfo,64>::Append @ 0x8270A148 copies three dwords (`stw` x3) at a
    // 12-byte stride (index*12 == `slwi r,1; add; slwi r,2`), count word @ +0x300 == 64*12;
    // ::Get @ 0x827BA2A0 returns 12*index + base). mLOD is CgsGraphics::Model::State (a
    // 4-byte enum, committed in CgsModel.h). Homed here with the module's other record types.
    struct VehicleRenderInfo
    {
        u32                      muEntityIndex; // :161  +0x00
        f32                      mfDistanceSq;  // :162  +0x04
        CgsGraphics::Model::State mLOD;         // :163  +0x08  (4-byte enum -> 12)
    };
}
