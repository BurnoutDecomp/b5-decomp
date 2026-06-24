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
}
