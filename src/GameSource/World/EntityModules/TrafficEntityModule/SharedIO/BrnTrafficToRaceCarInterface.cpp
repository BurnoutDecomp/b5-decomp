#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"  // BrnTraffic::MakeTrafficEntityId
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene member bodies, reconstructed from
// BURNOUT_X360_ARTIST.XEX. Two producers on the traffic module's pre-scene race-car interface:
// AddPotentialStompee (the stomp-candidate buffer) and SetSympatheticCrasher (the sympathetic-
// crash bit set).

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // Local mirror of BrnTraffic::TrafficEntityModule::KU_MAX_STANDARD_TRAFFIC (asm immediate 0x190).
    // Avoids pulling the heavy BrnTrafficEntityModule.h just for the bound.
    static const u32 KU_MAX_STANDARD_TRAFFIC = 0x190; // 400

    // BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene::Construct
    //   -- NO out-of-line console body: OutputBuffer_PreScene::Construct @0x82761790 INLINES the
    // whole initialisation of its mTrafficToRaceCarInterface_PreScene member as a run of raw
    // stores over the console span [818784, 819328). De-inlined here (AGENTS "inlining reversal")
    // so that Construct can spell its leg by name instead of value-initialising an opaque blob.
    //
    // ⭐ ADDED wave Q6 round-1 fix (bridges #1). Store roll-call, offsets relative to the member:
    //   7 x `std 0` @ +0..55  -> mSympatheticCrashers        (BitArray<400> == 7 u64 fields)
    //   `stw 0`     @ +448    -> mNearMissTrafficCollection  (320 + 16*8 == the Array miCount)
    //   `stw 0`     @ +516    -> mNearMissRaceCarCollection  (452 +  8*8 == the Array miCount)
    //   `stw 0`     @ +520    -> miPotentialStompeeCount
    //   `stw 0`     @ +524    -> muNearbyStaticVehicleCount
    //   4 x `stfs flt_82001CC0` @ +528/+532/+536/+540
    //                         -> mfClosestDistanceSq / mfSecondClosestDistanceSq /
    //                            mfClosestAngleDiff / mfClosestPerpendicularDist.
    //                            flt_82001CC0 is the image's 0x00000000 == 0.0f.
    // ⚠️ mPotentialStompees[8] (+64..+319) is DELIBERATELY untouched -- the console zeroes only
    // the count that bounds it. Do not "helpfully" clear the records.
    void TrafficToRaceCarInterface_PreScene::Construct()
    {
        mSympatheticCrashers.UnSetAll();
        mNearMissTrafficCollection.Construct();
        mNearMissRaceCarCollection.Construct();
        miPotentialStompeeCount        = 0;
        muNearbyStaticVehicleCount     = 0;
        mfClosestDistanceSq            = 0.0f;
        mfSecondClosestDistanceSq      = 0.0f;
        mfClosestAngleDiff             = 0.0f;
        mfClosestPerpendicularDist     = 0.0f;
    }

    // BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene::AddPotentialStompee
    //   @ 0x82706028. Records a traffic vehicle the player could stomp (drive over). Two regimes on
    // the live count (KI_MAX_POTENTIAL_STOMPEES == 8):
    //   - count < 8: append a fresh entry at [count] (position + traffic EntityId + distance^2), ++count.
    //   - count == 8 (full): scan for the FIRST existing entry whose stored distance^2 exceeds the new
    //     one and OVERWRITE it in place (no shift, no count change). If every stored entry is nearer, drop it.
    // The full-regime path inlines MakeTrafficEntityId ((idx<<10)|0x02000000) with its idx<1<<14 assert.
    // Producer: BrnTraffic::TrafficEntityModule::GeneratePotentialLeapedAndStompedCarsOutput.
    void TrafficToRaceCarInterface_PreScene::AddPotentialStompee(
            u32 luEntityIndex, Vector3 lPosition, f32 lfDistanceSquared)
    {
        static_assert(offsetof(TrafficToRaceCarInterface_PreScene, miPotentialStompeeCount) == 520,
                      "miPotentialStompeeCount @520");

        if (miPotentialStompeeCount < KI_MAX_POTENTIAL_STOMPEES)
        {
            // Append a fresh entry at the end.
            VehicleStompingData& lrSlot = mPotentialStompees[miPotentialStompeeCount];
            lrSlot.mStompeeEntityId  = MakeTrafficEntityId(luEntityIndex);
            lrSlot.mStompeePosition  = lPosition;
            lrSlot.mfDistanceSquared = lfDistanceSquared;
            ++miPotentialStompeeCount;
            return;
        }

        // Buffer full: replace the first stored entry that is farther than the new candidate.
        for (s32 liSlot = 0; liSlot < miPotentialStompeeCount; ++liSlot)
        {
            if (lfDistanceSquared < mPotentialStompees[liSlot].mfDistanceSquared)
            {
                // Inlined MakeTrafficEntityId (asm packs it directly here; same immediates: <<10 | 0x02000000).
                CGS_ASSERT(luEntityIndex < (1U << 14),
                           "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");

                VehicleStompingData& lrSlot = mPotentialStompees[liSlot];
                lrSlot.mfDistanceSquared        = lfDistanceSquared;
                lrSlot.mStompeeEntityId.muValue = (luEntityIndex << 10) | 0x02000000u;
                lrSlot.mStompeePosition         = lPosition;
                return;
            }
        }
    }

    // BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene::SetSympatheticCrasher
    //   @ 0x82710848. Sets or clears the sympathetic-crash flag for a standard-traffic vehicle in the
    // mSympatheticCrashers bit set (BitArray<400>). Two entry asserts bound-check the index
    // (>= 0, < KU_MAX_STANDARD_TRAFFIC == 400), then the X360 inlines BitArray<400>::SetBit /
    // ::UnSetBit -- each carrying its own bounds tripwire (the streamed CgsBitArray.h:222 for SetBit;
    // the static `luIndex < NUMBITS` :241 for UnSetBit). Producer:
    // BrnTraffic::TrafficEntityModule::GenerateSympatheticCrasherOutput.
    void TrafficToRaceCarInterface_PreScene::SetSympatheticCrasher(s32 liTrafficCarIndex, bool lbIsCrasher)
    {
        CGS_ASSERT(liTrafficCarIndex >= 0, "liTrafficCarIndex >= 0");
        CGS_ASSERT(static_cast<u32>(liTrafficCarIndex) < KU_MAX_STANDARD_TRAFFIC,
                   "uint32_t(liTrafficCarIndex) < BrnTraffic::KU_MAX_STANDARD_TRAFFIC");

        if (lbIsCrasher)
        {
            // Streamed CgsBitArray.h:222 tripwire (dynamic `Index: <i>, Number of bits: 400`); the
            // recoverable rodata head is `Index: `. Non-gating.
            CGS_ASSERT(static_cast<u32>(liTrafficCarIndex) < KU_MAX_STANDARD_TRAFFIC, "Index: ");
            mSympatheticCrashers.SetBit(static_cast<u32>(liTrafficCarIndex));
        }
        else
        {
            CGS_ASSERT(static_cast<u32>(liTrafficCarIndex) < KU_MAX_STANDARD_TRAFFIC,
                       "luIndex < NUMBITS");   // CgsBitArray.h:241 (static rodata)
            mSympatheticCrashers.UnSetBit(static_cast<u32>(liTrafficCarIndex));
        }
    }
}
}
