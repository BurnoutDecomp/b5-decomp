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

    // One predicted race-car-hull change record: the module keeps a list of pending hull
    // (collision-shape) swaps to apply to nearby race cars (producers:
    // TrafficEntityModule::AddPredictedHullChange / ::UpdateRaceCarHulls; consumer/debug dump
    // TrafficEntityModule::DEBUGDumpHullPredictions). sizeof == 8 (X360-authoritative): the
    // Array<HullChangeInfo,400> instantiations put their live-count word at byte N*8 and copy
    // each element as two dwords --
    //   Array<HullChangeInfo,400>::Append   @ 0x8270ACA8 stores `stw r10,0(slot)` then
    //     `stw r10,4(slot)`, count word @ +0xC80 == 400*8;
    //   Array<HullChangeInfo,400>::EraseFast @ 0x8270ADD0 overwrites slot[index] with the last
    //     live element via a two-dword copy at an 8-byte stride (`slwi r,count,3`);
    //   Array<HullChangeInfo,400>::GetItem   @ 0x8270CE00 returns 8*index + base.
    //
    // FLAG (opaque interior): the 8-byte record's internal field split is not recovered by this
    // slice -- every observed body (Append/EraseFast/GetItem) treats the element only as two
    // 4-byte words. Modelled as exactly two 4-byte words so the asm-attested 8-byte stride and the
    // +0xC80 inline-buffer offset are exact; the interior (the use sites suggest a target race-car
    // entity index plus a frame/hull-state word, but that is not asm-attested) is honestly opaque.
    // Grow this struct in place (never redefine/reorder) when the BrnTrafficHullRuntime slice
    // recovers the field names.
    struct HullChangeInfo
    {
        u32 muWord0; // +0x00  (interior opaque -- see FLAG)
        u32 muWord1; // +0x04  (-> 8)
    };

    // DWARF home BrnTrafficEntityModule.h:255 -- one "crashing thing" the module tracks while
    // building the per-frame list of things nearby traffic should react to (producers/consumers:
    // TrafficEntityModule::UpdateParams_BuildListOfCrashingThings / _TryStartSympatheticCrashing /
    // _TryAvoidCrashing, all operating on Array<CrashingThingData,168>). sizeof == 32
    // (X360-authoritative: the Array<CrashingThingData,168>::operator[] @ 0x8270BE68 returns
    // 32*index + base -- `slwi r,index,5` -- and reads the live-count word at byte +0x1500 ==
    // 168*32). The 32-byte footprint is the 16-byte/16-aligned Vector3 (mPosition) followed by
    // the 4-byte EntityId and the bool, rounded up to the Vector3's 16-byte alignment.
    // DWARF field order/types: Vector3 mPosition (:275), EntityId mEntityId (:276),
    // bool mbShowtimeCrashMagnet (:278).
    struct CrashingThingData
    {
        Vector3  mPosition;            // :275  +0x00  (16, 16-aligned)
        EntityId mEntityId;            // :276  +0x10
        bool     mbShowtimeCrashMagnet;// :278  +0x14  (+pad -> 32, 16-aligned)
    };

    // NB: BrnTraffic::PhysicalVehicleInfo (the Array<PhysicalVehicleInfo,33> element) is NOT
    // homed here -- it has its own committed home BrnTrafficPhysicalVehicleInfo.h; do not
    // redefine it (ODR). Its DWARF fields were grown into that header in this slice.

    // DWARF home BrnTrafficEntityModule.h:293 -- a SIMD "structure-of-arrays" packet holding four
    // collidable vehicles at once (the trailing "4" in the type name = four vehicles per record),
    // cached in mCachedCollidableList (Array<CollidableVehicleInfo4,16> -> up to 64 vehicles ==
    // KU_MAX_COLLIDABLE_CACHED_TRAFFIC). Each Vector4 lane holds the same field for all four
    // vehicles. sizeof == 128 (X360-authoritative: the Array<CollidableVehicleInfo4,16>::operator[]
    // @ 0x8270D260 returns (index<<7) + base -- `slwi r,index,7` == 128*index -- and reads the
    // live-count word at byte +0x800 == 16*128). The 128-byte footprint is exactly eight
    // 16-byte/16-aligned Vector4 registers. DWARF field order/types (:295-303):
    //   Vector4 mPosition_X / _Y / _Z, mLinearVelocity_X / _Y / _Z, mHalfLengths, mHalfWidths.
    // All eight lanes (128 bytes) are DWARF-named at BrnTrafficEntityModule.h:295-303.
    struct CollidableVehicleInfo4
    {
        Vector4 mPosition_X;       // :295  +0x00
        Vector4 mPosition_Y;       // :296  +0x10
        Vector4 mPosition_Z;       // :297  +0x20
        Vector4 mLinearVelocity_X; // :298  +0x30
        Vector4 mLinearVelocity_Y; // :299  +0x40
        Vector4 mLinearVelocity_Z; // :300  +0x50
        Vector4 mHalfLengths;      // :302  +0x60
        Vector4 mHalfWidths;       // :303  +0x70
    };
}
