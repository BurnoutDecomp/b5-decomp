// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_UpdateCrashes.cpp
//
// THE PER-FRAME CRASH-RECORD TICKER -- called every frame by the landed UpdateVehiclePhysics
// (BrnVehicleManager_UpdateVehiclePhysics.cpp:844, between the gs_iUpdateCrashesPM monitors).
//
//   VehicleManager::UpdateCrashes  @0x825EA640  (203 insns; DWARF BrnVehicleManager.cpp:7105)
//
// Slice TU. The LinkStubs boot gate that stood in BrnVehicleManagerLinkStubs.cpp is DELETED in
// the same change (LNK2005 is the tripwire if it ever reappears).
//
// -------------------------------------------------------------------------------------------------
// WHAT THE 203 INSTRUCTIONS ARE. All but eleven of them are the INLINED CgsBitArray<32u>
// iterator pair (GetFirstNonZeroBit / GetNextNonZeroBit) plus the two CGS_ASSERT message
// builders it carries ("luIndex < NUMBITS" CgsBitArray.h:241 for UnSetBit, and the StrStream-
// formatted "invalid index : %u < 32" CgsBitArray.h:203 for IsBitSet). The body proper is:
//
//   0x825EA650  r22 = this + 0x10000 - 0x5338 == this + 44232   -> mUsedRaceCarCrashesList
//               (BitArray<32u>: ONE u64 word -- the word-scan loop bound is `cmplwi r11, 1`)
//   0x825EA66C..0x825EA6B4  lowest set bit: isolate (x & (x-1)) ^ x, cntlzd, 63 - clz
//   0x825EA6B8  exit unless 0 <= liIndex < 32
//   per set bit liIndex (0x825EA724..):
//     0x825EA724  r11 = this + liIndex*12 + 0xAB28   == &maRaceCarCrashes[liIndex] + 8
//                 (43808 + 8: the pool base is +43808, +8 is RaceCarCrashData::mfTimeSinceImpact)
//     0x825EA73C  f0 = mfTimeSinceImpact + lfSimTimerTimeStep ; stfs  (the ONLY non-bit store)
//     0x825EA748  fcmpu f0, flt_82004014 (== 0.1f) ; ble skip      -> if (time > 0.1f)
//     0x825EA770      word &= ~(1 << (liIndex & 63))                -> UnSetBit(liIndex)
//     0x825EA788..0x825EA958  GetNextNonZeroBit(liIndex): scan liIndex+1 .. min(word end, 32),
//                 then the next non-zero word (there is none: 1 word), then exit on -1 / >= 32.
//
// The DWARF hint block for :7105 lists exactly: liIndex, BitArray<32u>::GetFirstNonZeroBit,
// BitArray<32u>::UnSetBit, BitArray<32u>::GetNextNonZeroBit. Nothing else is called.
//
// WHAT IT MEANS. A set bit in mUsedRaceCarCrashesList is an ALLOCATED maRaceCarCrashes slot
// (SetRaceCarCrashing allocates one per committed crash, seeding mfTimeSinceImpact = 0 -- and,
// pool full, evicts the slot with the LARGEST mfTimeSinceImpact, i.e. the oldest). This ticker
// ages every allocated slot by the sim timestep and frees it once it is older than 0.1 s. The
// record therefore lives for ~3 sim frames at 30 Hz -- long enough for the consumers that match
// crash records by entity id (the crash-event / scoring readback) to see it, no longer.
//
// It does NOT end the car's crash state (mbCrashing lives on RaceCarPhysics and is cleared by the
// crash-lifecycle code on the physics side); a taken-down rival is not stranded by anything here.
//
// POLARITY, checked twice against the asm:
//   * `fcmpu cr6, f0, f13 ; ble cr6, skip` -- the bit is CLEARED when time > 0.1f, KEPT otherwise.
//   * `andc r10, r9, r10 ; stdx` -- the store is word & ~mask: an UNSET, not a set.
//   * The first/next-bit exits are `>= 32` and `<= -1`; the recon BitArray<32> returns -1 in
//     both cases, so `liIndex >= 0` is the whole loop condition.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<32> (mUsedRaceCarCrashesList)

namespace BrnPhysics
{
namespace Vehicle
{
    namespace
    {
        // flt_82004014 == 0.1f (read from the image). How long an allocated RaceCarCrashData slot
        // stays live before UpdateCrashes frees it. FLAG: constant NAME proposed (no DWARF symbol
        // for the literal); the value and its use are asm-literal.
        const f32 KF_RACE_CAR_CRASH_DATA_LIFETIME = 0.1f;
    }

    void VehicleManager::UpdateCrashes(f32 lfSimTimerTimeStep)
    {
        for (s32 liIndex = mUsedRaceCarCrashesList.GetFirstNonZeroBit();          // 0x825EA66C..0x825EA6C4
             liIndex >= 0;
             liIndex = mUsedRaceCarCrashesList.GetNextNonZeroBit(liIndex))         // 0x825EA788..0x825EA958
        {
            RaceCarCrashData& lrCrash = maRaceCarCrashes[liIndex];                 // 0x825EA724 (stride 12, +43808)

            lrCrash.mfTimeSinceImpact += lfSimTimerTimeStep;                       // 0x825EA740 fadds ; stfs
            if (lrCrash.mfTimeSinceImpact > KF_RACE_CAR_CRASH_DATA_LIFETIME)       // 0x825EA748 fcmpu ; ble
            {
                mUsedRaceCarCrashesList.UnSetBit(static_cast<u32>(liIndex));       // 0x825EA770..0x825EA784 andc ; stdx
            }
        }
    }

} // namespace Vehicle
} // namespace BrnPhysics
