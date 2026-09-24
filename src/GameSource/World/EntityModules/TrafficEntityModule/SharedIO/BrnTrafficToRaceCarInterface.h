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

        // DWARF :122 -- the consumer half of the stompee buffer. No out-of-line body in either
        // build; both callers inline it, and the inlined shape is the declaration's:
        //   the count goes out through the pointer, the record array comes back.
        // Consumer RaceCarEntityModule::ProcessLeapedAndStompedCars @0x822BD5B8:
        //   0x822BD62C  lwz  r10, 0x208(r3)   ; miPotentialStompeeCount
        //   0x822BD640  stw  r10, 0(r8)       ; *lpiNumStompees (r8 = &miStoredStompeeCount)
        //   0x822BD638  addi r11, r3, 0x40    ; mPotentialStompees (+64), 32-byte records:
        //               position @+0 (lvx128), EntityId @+0x10 (lwz 0x10(r11))
        // (DecFIGS 0x14C7F4 inlines the same pair: +0x208 and +256 == +0xC0 + 64.)
        const VehicleStompingData* GetPotentialStompees(s32* lpiNumStompees) const   // :122
        {
            *lpiNumStompees = miPotentialStompeeCount;
            return mPotentialStompees;
        }

        // DWARF :126 -- empty the stompee buffer before a frame's candidates are added. Only
        // the count is reset (the records stay, exactly as Construct leaves them). Inlined by its
        // one caller, TrafficEntityModule::GeneratePotentialLeapedAndStompedCarsOutput
        // @0x8271F298: `bl GetTrafficToRaceCarInterface_PreScene` then `stw r31(0), 0x208(r3)`
        // at 0x8271F2E4 (DecFIGS 0x91D530: `*((lpOutput + 818784) + 0x208) = 0`).
        void ClearStompees()                                                       // :126
        {
            miPotentialStompeeCount = 0;
        }

        // ADDITIVE GROW. The console has no out-of-line
        // body for this: OutputBuffer_PreScene::Construct @0x82761790 INLINES the whole
        // initialisation of this member as a run of raw stores over the console span
        // [818784, 819328). De-inlining it here (AGENTS "inlining reversal") is what lets that
        // Construct spell its leg by NAME instead of value-initialising an opaque 544-byte
        // blob, and keeps the zero roll-call next to the members it names.
        //
        // MEASURED store roll-call, in console order (offsets relative to the member's base):
        //   7 x `std 0` @ +0..55   -> mSympatheticCrashers (BitArray<400> == 7 u64)
        //   `stw 0`     @ +448     -> mNearMissTrafficCollection.miCount (320 + 16*8)
        //   `stw 0`     @ +516     -> mNearMissRaceCarCollection.miCount (452 + 8*8)
        //   `stw 0`     @ +520     -> miPotentialStompeeCount
        //   `stw 0`     @ +524     -> muNearbyStaticVehicleCount
        //   4 x `stfs flt_82001CC0` @ +528/+532/+536/+540
        //                          -> mfClosestDistanceSq / mfSecondClosestDistanceSq /
        //                             mfClosestAngleDiff / mfClosestPerpendicularDist
        //                             (flt_82001CC0 is the rodata 0.0f the build reuses)
        // NOTE what the console does NOT zero: mPotentialStompees[8] keeps whatever the IO
        // stack's previous tenant left -- only its count is reset. Do not "helpfully" clear it.
        void Construct();

        // The two near-miss collections the race-car module drains once per post-scene tick.
        // The console has no out-of-line body for either: it folds both to the member address
        // at the drain site and then asserts the result is non-NULL, which is the assert the
        // consumer still carries. Const because the drain only reads.
        const NearMissTrafficCollection* GetNearMissTrafficCollection() const
        {
            return &mNearMissTrafficCollection;
        }
        const NearMissRaceCarCollection* GetNearMissRaceCarCollection() const
        {
            return &mNearMissRaceCarCollection;
        }

        // The write halves, folded the same way at the traffic module's publish site
        // (GenerateNearMissOutput), which copies each whole Array in over the top.
        NearMissTrafficCollection* GetNearMissTrafficCollection()
        {
            return &mNearMissTrafficCollection;
        }
        NearMissRaceCarCollection* GetNearMissRaceCarCollection()
        {
            return &mNearMissRaceCarCollection;
        }

        // DWARF :135 -- the parked-traffic proximity publish. No out-of-line body in the image: its
        // one producer, TrafficEntityModule::GenerateNearbyParkedTrafficOutput @0x8271FA18, folds
        // it into five stores on the interface GetTrafficToRaceCarInterface_PreScene (0x82710DD0)
        // returns: 0x8271FBC4 `stw r22, 0x20C` then `stfs 0x210 / 0x214 / 0x218 / 0x21C`.
        void SetNearbyParkedTrafficData(u32 luNearbyStaticVehicleCount, f32 lfClosestDistanceSq,
                                        f32 lfSecondClosestDistanceSq, f32 lfClosestAngleDiff,
                                        f32 lfClosestPerpendicularDist)                     // :135
        {
            muNearbyStaticVehicleCount = luNearbyStaticVehicleCount;
            mfClosestDistanceSq        = lfClosestDistanceSq;
            mfSecondClosestDistanceSq  = lfSecondClosestDistanceSq;
            mfClosestAngleDiff         = lfClosestAngleDiff;
            mfClosestPerpendicularDist = lfClosestPerpendicularDist;
        }

        // DWARF :144 -- the read half, also inlined: RaceCarEntityModule::ProcessPowerParking
        // @0x822CDF10 reads +0x210 / +0x20C / +0x214 / +0x218 / +0x21C at 0x822CDF34..0x822CDF64.
        void GetNearbyParkedTrafficData(u32* lpuNearbyStaticVehicleCount, f32* lpfClosestDistanceSq,
                                        f32* lpfSecondClosestDistanceSq, f32* lpfClosestAngleDiff,
                                        f32* lpfClosestPerpendicularDist) const            // :144
        {
            *lpuNearbyStaticVehicleCount = muNearbyStaticVehicleCount;
            *lpfClosestDistanceSq        = mfClosestDistanceSq;
            *lpfSecondClosestDistanceSq  = mfSecondClosestDistanceSq;
            *lpfClosestAngleDiff         = mfClosestAngleDiff;
            *lpfClosestPerpendicularDist = mfClosestPerpendicularDist;
        }

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
