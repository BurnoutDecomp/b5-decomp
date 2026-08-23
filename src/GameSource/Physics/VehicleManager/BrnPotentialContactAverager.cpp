// =================================================================================================
// GameSource/Physics/VehicleManager/BrnPotentialContactAverager.cpp
//
// Wave T3 round 2, owner B -- the per-frame potential-contact accumulator that sits between
// DoCrashPrediction and every Handle*PotentialContact handler.
//
//   PotentialContactAverager::FindSlotForContact       @0x825B4FD0 ( 50)
//   PotentialContactAverager::AddContactPair           @0x825C3218 (108)
//   PotentialContactAverager::GetAveragedContactPoint  @0x825C34D0 (135)
//
// Console home is BrnVehicleManager.cpp (its asserts cite :422/:493/:494/:503); homed in its own
// TU here so the crash-prediction slice can include the type without dragging that file in.
// The DecFIGS DWARF only forward-declares the type, so the header's layout is asm-recovered --
// see the header banner.
//
// FindSlotForContact was an .ida-exports HOLE; dumped headless from a COPY of the ARTIST .i64
// for this round (scratchpad .../wave3r2/B/findslot.txt).
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnPotentialContactAverager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cmath>   // sqrtf (the vrsqrtefp + 2 Newton steps the console inlines)

namespace BrnPhysics
{
namespace Vehicle
{
    namespace
    {
        // flt_82002138 -- the "is this normal unit length" tolerance both this file and
        // DecideOutcomeOfRaceCarTrafficContact / PredictCarCarIntersection share.
        const f32 KF_UNIT_NORMAL_TOLERANCE = 0.00999999978f;

        // stru_8208F620 / flt_82002514 -- RwMathFPU::IsZero's symmetric epsilon.
        const f32 KF_IS_ZERO_EPSILON = 1.1920928955078125e-07f;

        // unk_82181510 -- the {0,1,0,0} world-up fallback the console stores over a normal that
        // still fails the unit test after the divide (`lvx128 v0, r0, unk_82181510 ; stvx128`).
        // Recovered from the image (.rdata, not a dyn-init slot).
        inline Vector3 FallbackContactNormal()
        {
            Vector3 lvNormal;
            lvNormal.x = 0.0f;
            lvNormal.y = 1.0f;
            lvNormal.z = 0.0f;
            lvNormal.w = 0.0f;
            return lvNormal;
        }

        inline bool IsUnitLength(const Vector3& lrVector)
        {
            const f32 lfLengthSq = lrVector.x * lrVector.x
                                 + lrVector.y * lrVector.y
                                 + lrVector.z * lrVector.z;
            const f32 lfLength   = (lfLengthSq == 0.0f) ? 0.0f : lfLengthSq / sqrtf(lfLengthSq);
            const f32 lfError    = (lfLength < 1.0f) ? (1.0f - lfLength) : (lfLength - 1.0f);
            return !(lfError > KF_UNIT_NORMAL_TOLERANCE);
        }

        inline Vector3 AddVectors(const Vector3& lrA, const Vector3& lrB)
        {
            Vector3 lvResult;
            lvResult.x = lrA.x + lrB.x;
            lvResult.y = lrA.y + lrB.y;
            lvResult.z = lrA.z + lrB.z;
            lvResult.w = lrA.w + lrB.w;
            return lvResult;
        }

        inline Vector3 ScaleVector(const Vector3& lrV, f32 lfScale)
        {
            Vector3 lvResult;
            lvResult.x = lrV.x * lfScale;
            lvResult.y = lrV.y * lfScale;
            lvResult.z = lrV.z * lfScale;
            lvResult.w = lrV.w * lfScale;
            return lvResult;
        }

        inline Vector3 NegateVector(const Vector3& lrV)
        {
            // `vspltisw v0,-1 ; vslw v0,v0,v0 ; vxor` == a per-lane sign flip, all four lanes.
            return ScaleVector(lrV, -1.0f);
        }
    }

    // ---------------------------------------------------------------------------------------
    // FindSlotForContact  @0x825B4FD0
    //
    // The console compares only the two VolumeInstanceIds' embedded ENTITY words (the HIGH
    // dword: `ld r10,-8(r11) ; srdi r10,r10,32 ; clrlwi`), never the volume index -- so every
    // volume overlap between the same two entities folds into one slot. Both orders match; the
    // swapped order sets the out flag so AddContactPair can mirror the incoming contact.
    // ---------------------------------------------------------------------------------------
    s32 PotentialContactAverager::FindSlotForContact(
        CgsSceneManager::SceneManagerIO::PotentialContact lContact,
        bool* lpbOutSwapped) const
    {
        const u32 luEntityA = static_cast<u32>(lContact.muVolumeInstanceIdA.muId >> 32);
        const u32 luEntityB = static_cast<u32>(lContact.muVolumeInstanceIdB.muId >> 32);

        for (u32 luSlot = 0; luSlot < muContactPairCount; ++luSlot)
        {
            const CgsSceneManager::SceneManagerIO::PotentialContact& lrStored = maContactPairs[luSlot];
            const u32 luStoredA = static_cast<u32>(lrStored.muVolumeInstanceIdA.muId >> 32);
            const u32 luStoredB = static_cast<u32>(lrStored.muVolumeInstanceIdB.muId >> 32);

            if (luStoredA == luEntityA && luStoredB == luEntityB)
            {
                *lpbOutSwapped = false;
                return static_cast<s32>(luSlot);
            }
            if (luStoredA == luEntityB && luStoredB == luEntityA)
            {
                *lpbOutSwapped = true;
                return static_cast<s32>(luSlot);
            }
        }
        return -1;
    }

    // ---------------------------------------------------------------------------------------
    // AddContactPair  @0x825C3218
    //
    // New pair -> claim slot muContactPairCount (80-byte copy, weight 1.0) unless all 20 are
    // taken (the ONLY false return). Existing pair -> accumulate points and normal, MIRRORED
    // when the stored slot holds the pair the other way round (0x825C32BC..0x825C32D8: the
    // incoming mPointOnA lands on the slot's mPointOnB and the normal is sign-flipped), then
    // weight += 1.0.
    // ---------------------------------------------------------------------------------------
    bool PotentialContactAverager::AddContactPair(
        CgsSceneManager::SceneManagerIO::PotentialContact lContact)
    {
        bool lbSwapped = false;
        const s32 liSlot = FindSlotForContact(lContact, &lbSwapped);

        if (liSlot == -1)
        {
            if (muContactPairCount >= KU_MAX_CONTACT_PAIRS)
                return false;

            maContactPairs[muContactPairCount]        = lContact;
            mafContactPairsCounts[muContactPairCount] = 1.0f;
            ++muContactPairCount;
            return true;
        }

        CgsSceneManager::SceneManagerIO::PotentialContact& lrSlot = maContactPairs[liSlot];

        const Vector3 lvNormal   = lbSwapped ? NegateVector(lContact.mNormal) : lContact.mNormal;
        const Vector3 lvOntoSlotA = lbSwapped ? lContact.mPointOnB : lContact.mPointOnA;
        const Vector3 lvOntoSlotB = lbSwapped ? lContact.mPointOnA : lContact.mPointOnB;

        lrSlot.mNormal   = AddVectors(lrSlot.mNormal, lvNormal);
        lrSlot.mPointOnA = AddVectors(lrSlot.mPointOnA, lvOntoSlotA);
        lrSlot.mPointOnB = AddVectors(lrSlot.mPointOnB, lvOntoSlotB);

        // BrnVehicleManager.cpp:422 -- a per-lane vcmpeqfp(n,n) finite test on the ACCUMULATED
        // normal. Console streams the offending vector; lowered to the static prefix.
        CGS_ASSERT(lrSlot.mNormal.x == lrSlot.mNormal.x
                       && lrSlot.mNormal.y == lrSlot.mNormal.y
                       && lrSlot.mNormal.z == lrSlot.mNormal.z,
                   "Invalid contact added to averager: ");

        mafContactPairsCounts[liSlot] += 1.0f;
        return true;
    }

    // ---------------------------------------------------------------------------------------
    // GetAveragedContactPoint  @0x825C34D0
    //
    // f30 = 1 / weight (`fdivs f30, 1.0, mafContactPairsCounts[i]` @0x825C35A0), the slot is
    // copied out whole (the ten `ld`/`std` pairs == 80 bytes), then: normal *= f30 and is
    // renormalised, the unit test fires BrnVehicleManager.cpp:503 and -- if it STILL fails --
    // the normal is replaced by unk_82181510 == {0,1,0,0}; finally both contact points are
    // scaled by f30.
    // ---------------------------------------------------------------------------------------
    void PotentialContactAverager::GetAveragedContactPoint(
        u32 luIndex,
        CgsSceneManager::SceneManagerIO::PotentialContact& lrOutContact) const
    {
        CGS_ASSERT(luIndex < muContactPairCount, "luIndex < muContactPairCount");

        const f32 lfWeight = mafContactPairsCounts[luIndex];
        CGS_ASSERT(!(lfWeight <= KF_IS_ZERO_EPSILON && lfWeight >= -KF_IS_ZERO_EPSILON),
                   "!RwMathFPU::IsZero(mafContactPairsCounts[luIndex])");

        const f32 lfRecipWeight = 1.0f / lfWeight;

        lrOutContact = maContactPairs[luIndex];

        Vector3 lvNormal = ScaleVector(lrOutContact.mNormal, lfRecipWeight);
        lrOutContact.mNormal = lvNormal;

        const f32 lfLengthSq = lvNormal.x * lvNormal.x
                             + lvNormal.y * lvNormal.y
                             + lvNormal.z * lvNormal.z;
        if (lfLengthSq != 0.0f)
            lvNormal = ScaleVector(lvNormal, 1.0f / sqrtf(lfLengthSq));
        lrOutContact.mNormal = lvNormal;

        CGS_ASSERT(IsUnitLength(lrOutContact.mNormal), "Bad normal in GetAveragedContactPoint: ");
        if (!IsUnitLength(lrOutContact.mNormal))
            lrOutContact.mNormal = FallbackContactNormal();

        lrOutContact.mPointOnA = ScaleVector(lrOutContact.mPointOnA, lfRecipWeight);
        lrOutContact.mPointOnB = ScaleVector(lrOutContact.mPointOnB, lfRecipWeight);
    }
}
}
