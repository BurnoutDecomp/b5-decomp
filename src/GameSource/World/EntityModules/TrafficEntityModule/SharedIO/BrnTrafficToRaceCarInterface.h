#pragma once

// Traffic -> race-car shared-IO element payloads. Reconstructed from the DecFIGS DWARF
// (GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h).
//
// This is the element-type home for the traffic-to-race-car interface records. The X360
// build packs NearMissData into an Array<NearMissData,16> (the near-miss traffic collection)
// inside TrafficToRaceCarInterface_PreScene; the Array<NearMissData,16>::Append instantiation
// (X360 0x82709C00) is the per-instantiation .cpp alongside this header.
//
// Element sizes are X360-authoritative (the Array<NearMissData,16>::Append @ 0x82709C00 reads
// the live count at byte offset 0x80 == 16 * sizeof(NearMissData) and writes 2 dwords per
// element with a 3-bit << stride, i.e. sizeof(NearMissData) == 8).
#include "types.hpp"                          // u32/f32/s8/s32
#include "BrnCommonTypes.h"                   // Vector3, EntityId
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<400>
#include "GameShared/GameClasses/Containers/CgsArray.h"      // Array<NearMissData,N>

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    using CgsContainers::BitArray;   // (repo spells BitArray unqualified per CgsBitArray.h note)

    // DWARF :48 -- a traffic vehicle the player could "stomp" (drive over). sizeof == 32
    // (Vector3 SIMD(16) + EntityId(4) + f32(4) -> 24, padded to 32 by the Vector3 16-align).
    struct alignas(16) VehicleStompingData
    {
        Vector3  mStompeePosition;   // :50
        EntityId mStompeeEntityId;   // :51
        f32      mfDistanceSquared;  // :52
    };

    // DWARF :57 -- one nearby traffic/race-car proximity record. sizeof == 8 (X360-authoritative:
    // Array<NearMissData,16>::Append @ 0x82709C00 writes 2 dwords per element with an 8-byte stride).
    struct NearMissData
    {
        u32 muCarId;     // :59
        f32 mfDistance;  // :60
    };

    // DWARF :77 -- the traffic-module -> race-car PRE-SCENE interface. Records the traffic vehicles
    // the player could stomp (drive over) this frame, the sympathetic-crash bit set, and the
    // nearby-traffic near-miss collections + parked-traffic proximity scalars. Reconstructed from
    // BURNOUT_X360_ARTIST.XEX; member offsets verified against AddPotentialStompee
    // (mPotentialStompees@64, miPotentialStompeeCount@520) and SetSympatheticCrasher
    // (mSympatheticCrashers@0):
    //   mSympatheticCrashers        BitArray<400>              @   0  (7 u64 = 56 bytes)
    //   mPotentialStompees          VehicleStompingData[8]     @  64  (alignas(16) pads 56->64; 8*32=256)
    //   mNearMissTrafficCollection  Array<NearMissData,16>     @ 320  (16*8 + 4 = 132)
    //   mNearMissRaceCarCollection  Array<NearMissData,8>      @ 452  (8*8 + 4 = 68)
    //   miPotentialStompeeCount     s32                        @ 520
    //   muNearbyStaticVehicleCount  u32                        @ 524
    //   mfClosestDistanceSq         f32                        @ 528
    //   mfSecondClosestDistanceSq   f32                        @ 532
    //   mfClosestAngleDiff          f32                        @ 536
    //   mfClosestPerpendicularDist  f32                        @ 540
    struct TrafficToRaceCarInterface_PreScene
    {
        typedef Array<NearMissData, 16> NearMissTrafficCollection;   // DWARF :65
        typedef Array<NearMissData,  8> NearMissRaceCarCollection;   // DWARF :66

        static const s32 KI_MAX_POTENTIAL_STOMPEES = 8;   // asm `cmpwi count,8`

        // X360 0x82710848: set/clear the sympathetic-crash flag for a standard-traffic vehicle.
        void SetSympatheticCrasher(s32 liTrafficCarIndex, bool lbIsCrasher);            // :88 (0x82710848)
        // X360 0x82706028: record a traffic vehicle the player could stomp.
        void AddPotentialStompee(u32 luEntityIndex, Vector3 lPosition, f32 lfDistanceSquared); // :118 (0x82706028)

    private:
        BitArray<400>              mSympatheticCrashers;        // :150 @0
        VehicleStompingData        mPotentialStompees[8];       // :151 @64 (alignas(16) via VehicleStompingData)
        NearMissTrafficCollection  mNearMissTrafficCollection;  // :153 @320
        NearMissRaceCarCollection  mNearMissRaceCarCollection;  // :154 @452
        s32                        miPotentialStompeeCount;     // :156 @520
        u32                        muNearbyStaticVehicleCount;  // :158 @524
        f32                        mfClosestDistanceSq;         // :159 @528
        f32                        mfSecondClosestDistanceSq;   // :160 @532
        f32                        mfClosestAngleDiff;          // :161 @536
        f32                        mfClosestPerpendicularDist;  // :162 @540
    };
}
}
