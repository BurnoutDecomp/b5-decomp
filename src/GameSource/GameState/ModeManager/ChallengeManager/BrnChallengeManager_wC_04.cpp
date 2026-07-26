// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wC_04.cpp
// ============================================================================
// BrnGameState::ChallengeManager -- wave-C partfile (group G5): the three world-space
// gates of the freeburn-challenge scorer. All three were previously parked as
// "VMX128 blocked"; every VMX sequence in the X360 asm decodes to plain scalar math over
// the NAMED Vector3 lanes of the committed types, so they are reconstructed store-for-store
// here (no raw offset access, no header edits).
//
//   IsPointInTriggerRegion  (X360 0x82333368)
//   CheckCurrentLocation    (X360 0x82333878)
//   UpdateLeaptCars         (X360 0x82333BB8)
//
// SOURCE-OF-TRUTH: the X360 ARTIST asm is authoritative for every store, branch, early-out,
// assert and call. Two places where the wave-C fix spec's prose disagrees with the asm are
// resolved in FAVOUR OF THE ASM and flagged inline:
//   (1) CheckCurrentLocation's world-map sample goes through the *Vector2* overload
//       (xref target 0x82907FF8 == CgsWorld::WorldMap2D::GetValue(Vector2)); the `vperm`
//       against unk_82CDA450 immediately before the call is a BARE inline Vector3->Vector2
//       XZ pack that REUSES BrnMath::Flatten's permute mask -- it is NOT a call to Flatten
//       (@0x822CB8E8): the site emits no `bl` and 0x82333878 is absent from Flatten's
//       xrefs_to, so Flatten's own `RwMath::IsValid( lVector )` assert does not fire here.
//   (2) UpdateLeaptCars' two PHASE-1 SetCurrentSkillScore calls pass lbBankImmediately ==
//       TRUE (`li r6, 1` @0x82333D00 and @0x82333D98); only the tail call passes false
//       (`li r6, 0` @0x82334574).
//
// VMX decode notes (all confirmed against the operand order IDA prints for `vmaddfp`, which
// is the raw field order vD, vA, vB, vC -- i.e. the result is vA*vC + vB; the only reading
// under which both fused-multiply-adds in this TU are meaningful):
//   * `vspltw vX, vY, N` == reading lane N of a Vector3: 0 -> .x, 1 -> .y, 2 -> .z.
//   * `vsubfp` / `vmulfp128` by a splat scalar == the committed rw::math::vpu operator- /
//     operator*(Vector3, float).
//   * `vmsum3fp128 v0, v0, v0` == rw::math::vpu::MagnitudeSquared (xyz lanes).
//   * the `fsel` pairs are the two-step max() chain over the box dimensions.
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"

#include "GameShared/GameClasses/Containers/CgsObjectPool.h"        // ObjectPool<CarLeapingData,7,s32> (local candidate pool)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"          // BitArray<7> (local keep-mask)
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT + Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"        // CgsDev::StrStream (runtime-value assert messages)
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"             // CgsWorld::WorldMap2D::GetValue
#include "SharedClasses/DataLists/ChallengeListEntry.h"             // GetNumLocations / GetLocationType / GetDistrict / GetTriggerID / GetRoadID / GetCoopType
#include "SharedClasses/Trigger/BrnTriggerData.h"                   // BrnTrigger::TriggerData::GetRegion
#include "SharedClasses/Trigger/BrnTriggerBase.h"                   // BrnTrigger::TriggerRegion (GetId / GetType / GetBoxRegion)
#include "SharedClasses/Trigger/BrnGenericRegion.h"                 // BrnTrigger::GenericRegion::GetType (the sub-type byte)
#include "SharedClasses/Trigger/BrnRegion.h"                        // BrnTrigger::BoxRegion (GetPosition / GetDimension* / ComputeTransform)
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // GetTriggerData / GetActiveTriggerCount / GetActiveTrigger
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"     // RoadRulesManager::GetCurrentRoadID
#include "GameSource/Math/BrnMathUtils.h"                           // BrnMath::Flatten / BrnMath::IsPointInsideBox
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // the interface + RaceCarState
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h" // BrnPhysics::Vehicle::RaceCarState members
#include "rw/math/vpu/vector3_operation.h"                          // operator- / operator* / MagnitudeSquared

namespace BrnGameState
{

// X360 `cmpwi r29, 7` / `cmpwi r21, 7` / `li r5, 7` -- the leaping-data pool capacity, which is
// also the number of bits the local keep-mask BitArray carries (the inlined range asserts print
// "Number of bits: 7"). Named rather than repeated as a literal; the value is attested.
static const s32 KI_LEAPT_CAR_POOL_CAPACITY = 7;

// ----------------------------------------------------------------------------
// IsPointInTriggerRegion  (X360 0x82333368)
// ----------------------------------------------------------------------------
// Answer "is lpPoint inside the trigger region whose id is lTriggerID", for the subset of
// regions the TriggerQueryManager currently has armed. The region must be a generic region of
// sub-type 13; a cheap bounding-sphere pre-cull runs before the exact oriented-box test.
//
// Register map (asm prologue): r3 this, r4 lpPoint (r19), r5 lTriggerID (r20, 64-bit CgsID).
bool ChallengeManager::IsPointInTriggerRegion(const Vector3* lpPoint, CgsID lTriggerID)
{
    CGS_ASSERT(lpPoint, "lpPosition");                                   // X360 line 2594
    CGS_ASSERT(mpTriggerQueryManager, "mpTriggerQueryManager");          // X360 line 2595

    // Linear scan of the armed trigger set. The X360 re-reads the live count through the
    // accessor on every iteration (the CgsArray "Array used before Construct/Clear was called"
    // guard lives inside it) and once more after the loop -- reproduced by calling the committed
    // accessor at both points rather than caching the count.
    const BrnTrigger::TriggerRegion* lpTriggerRegion = NULL;
    s32 liIndex = 0;
    for (; liIndex < static_cast<s32>(mpTriggerQueryManager->GetActiveTriggerCount()); ++liIndex)
    {
        const u16 luRegionIndex = mpTriggerQueryManager->GetActiveTrigger(static_cast<u32>(liIndex));

        // X360 `TriggerData::GetMemor(this+1568)` == the committed ResourcePtr-backed accessor;
        // GetRegion carries its own "liRegionIndex < miRegionCount" guard (BrnTriggerData.h:624).
        lpTriggerRegion = mpTriggerQueryManager->GetTriggerData()->GetRegion(luRegionIndex);
        CGS_ASSERT(lpTriggerRegion != NULL, "lpTriggerRegion != NULL");   // X360 line 2603

        if (lpTriggerRegion->GetId() == lTriggerID)                      // lwz +0x24, extsw, cmpld
        {
            break;
        }
    }

    if (liIndex >= static_cast<s32>(mpTriggerQueryManager->GetActiveTriggerCount()))
    {
        return false;                                                    // scan exhausted: no such armed trigger
    }

    if (lpTriggerRegion->GetType() != BrnTrigger::TriggerRegion::E_TYPE_GENERIC_REGION)  // lbz +0x2A; cmplwi 2
    {
        // Runtime-valued diagnostic: the X360 streams the trigger id into the assert message
        // buffer through the global StrStream (the `sub_82203EE8` call is
        // StrStreamBase::operator<<(u64)). Reproduced with the committed local-StrStream idiom.
        // X360 file/line: BrnChallengeManager.cpp:2642.
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Trigger " << lTriggerID << " is not a generic region";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
        return false;
    }

    const BrnTrigger::GenericRegion* lpGenericRegion =
        static_cast<const BrnTrigger::GenericRegion*>(lpTriggerRegion);

    // X360 `lbz r11, 0x36(r31); cmplwi r11, 0xD` -- only generic sub-type 13 (== the committed
    // BrnTrigger::GenericRegion::E_TYPE_OVERDRIVE_STRENGTH, which the header pins at that very
    // offset) qualifies.
    if (lpGenericRegion->GetType() != BrnTrigger::GenericRegion::E_TYPE_OVERDRIVE_STRENGTH)
    {
        return false;
    }

    const BrnTrigger::BoxRegion* lpBoxRegion = lpTriggerRegion->GetBoxRegion();

    // Bounding-sphere pre-cull: the sphere radius is half the LARGEST box dimension. The two
    // `fsel`s are the max chain (Y-vs-Z first, then X against that).
    const f32 lfMaxDimensionYZ = (lpBoxRegion->GetDimensionY() >= lpBoxRegion->GetDimensionZ())
                                     ? lpBoxRegion->GetDimensionY()
                                     : lpBoxRegion->GetDimensionZ();
    const f32 lfMaxDimension   = (lpBoxRegion->GetDimensionX() >= lfMaxDimensionYZ)
                                     ? lpBoxRegion->GetDimensionX()
                                     : lfMaxDimensionYZ;
    const f32 lfRadius         = lfMaxDimension * 0.5f;                  // flt_82001DA0 == 0.5f

    // vsubfp (boxPosition - point) + vmsum3fp128 == MagnitudeSquared of the xyz lanes.
    const f32 lfDistanceSquared = MagnitudeSquared(lpBoxRegion->GetPosition() - *lpPoint);

    if (lfDistanceSquared < lfRadius * lfRadius)                         // vcmpgtfp. radius^2 > distance^2
    {
        const Matrix44Affine lTransform = lpBoxRegion->ComputeTransform();
        if (BrnMath::IsPointInsideBox(lTransform, *lpPoint, lpBoxRegion->GetDimensions() * 0.5f))
        {
            return true;
        }
    }

    return false;
}

// ----------------------------------------------------------------------------
// CheckCurrentLocation  (X360 0x82333878)
// ----------------------------------------------------------------------------
// Is the player currently somewhere this challenge action allows? An action with no location
// restrictions is allowed anywhere; otherwise ANY one of its (up to four) locations matching
// the player's current district / trigger region / road passes the gate.
//
// Register map (asm prologue): r3 this (r28), r4 lpAction (r26), r5 lpActiveRaceCarOutputInterface (r31).
bool ChallengeManager::CheckCurrentLocation(
    const BrnResource::ChallengeListEntryAction*                                 lpAction,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface)
{
    CGS_ASSERT(lpAction, "lpAction");                                             // X360 line 2723
    CGS_ASSERT(mpRoadRulesManager, "mpRoadRulesManager");                         // X360 line 2724
    CGS_ASSERT(lpActiveRaceCarOutputInterface, "lpActiveRaceCarOutputInterface"); // X360 line 2725

    // sub_82310240 == the committed GetPlayerRaceCarState() (it carries its own player-index asserts).
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState* lpRaceCarState =
        lpActiveRaceCarOutputInterface->GetPlayerRaceCarState();
    CGS_ASSERT(lpRaceCarState, "lpRaceCarState");                                 // X360 line 2729

    const CgsWorld::WorldMap2D* lpWorldMap = lpActiveRaceCarOutputInterface->GetWorldMap2D();
    CGS_ASSERT(lpWorldMap, "lpWorldMap");                                         // X360 line 2733

    // `lvx v0, r0, (state+544)` + `stvx v0` into the stack local == the by-value copy of the
    // transform's translation row. The local is address-taken by the TRIGGER case below.
    const Vector3 lCarPosition = lpRaceCarState->mTransform.Pos();

    // The `lvx128 v7,&unk_82CDA450; vperm v1,v0,v0,v7` here is a BARE permute -- the inline
    // Vector3 -> Vector2 XZ pack that shares BrnMath::Flatten's lane mask. It is NOT a call
    // to Flatten: Flatten @0x822CB8E8 is out-of-line, this site emits no `bl`, and 0x82333878
    // is absent from Flatten's xrefs -- so the IsValid assert Flatten's body fires does NOT
    // execute here. The call target is the Vector2 overload of GetValue (0x82907FF8).
    // FLAG (Flatten lane mask): unk_82CDA450 is still an un-valued .rdata blob, so XZ-vs-XY is
    // the same INFERENCE the committed BrnMathUtils.cpp:23-25 carries. If that mask is ever
    // resolved to XY, this site and BrnMath::Flatten must change together.
    const Vector2 l2DCarPosition = Vector2{ lCarPosition.x, lCarPosition.z };
    const s32 liDistrict = static_cast<s32>(lpWorldMap->GetValue(l2DCarPosition));

    const CgsID lCurrentRoadID = mpRoadRulesManager->GetCurrentRoadID();

    if (lpAction->GetNumLocations() == 0)                                         // lbz +4
    {
        return true;                                                              // unrestricted action
    }

    for (u8 lu8LocationIndex = 0; lu8LocationIndex < lpAction->GetNumLocations(); ++lu8LocationIndex)
    {
        // GetLocationType inlines the "luLocationIndex < KU_MAX_LOCATIONS_PER_ACTION" guard
        // (ChallengeListEntry.h:753) -- call the accessor, never duplicate the assert.
        switch (lpAction->GetLocationType(lu8LocationIndex))
        {
        case BrnResource::ChallengeListEntryAction::E_LOCATION_TYPE_ANYWHERE:
            return true;

        case BrnResource::ChallengeListEntryAction::E_LOCATION_TYPE_DISTRICT:
            if (lpAction->GetDistrict(lu8LocationIndex) == liDistrict)
            {
                return true;
            }
            break;

        case BrnResource::ChallengeListEntryAction::E_LOCATION_TYPE_TRIGGER:
            if (IsPointInTriggerRegion(&lCarPosition, lpAction->GetTriggerID(lu8LocationIndex)))
            {
                return true;
            }
            break;

        // The two road flavours share one branch in the X360 jump table.
        case BrnResource::ChallengeListEntryAction::E_LOCATION_TYPE_ROAD:
        case BrnResource::ChallengeListEntryAction::E_LOCATION_TYPE_ROAD_NO_MARKER:
            if (lCurrentRoadID != 0 && lpAction->GetRoadID(lu8LocationIndex) == lCurrentRoadID)
            {
                return true;
            }
            break;

        default:
        {
            // Runtime-valued diagnostic (the X360 re-reads the type byte through the same inlined
            // accessor to stream it). Committed local-StrStream idiom; the default case does NOT
            // return -- it falls through to the next location. X360 file/line:
            // BrnChallengeManager.cpp:2791.
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Unknown location type "
                       << static_cast<s32>(lpAction->GetLocationType(lu8LocationIndex));
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            break;
        }
        }
    }

    return false;
}

// ----------------------------------------------------------------------------
// UpdateLeaptCars  (X360 0x82333BB8)
// ----------------------------------------------------------------------------
// Per-frame tracker for the LEAP_CARS freeburn skill. Two phases:
//
//   PHASE 1 -- the player is off the gas: wind down the "leap still counts" grace timer, banking
//     the running leap count into the skill each frame (or, for the two accumulating co-op types,
//     only once the timer expires), then reset the tracker.
//
//   PHASE 2 -- rebuild the candidate set: every networked car the player is currently flying OVER
//     (a clear height gap, within a 10m horizontal radius) becomes a candidate. Each candidate is
//     matched against the persistent mPotentiallyLeaptCars pool by active-race-car index; the sign
//     of the 2D (XZ) dot of the stored entry vector against the current player->car vector says
//     whether the player has crossed over that car -- a sign flip is a leap. Slots that were not
//     re-confirmed this frame are freed.
//
// Register map (asm prologue): r3 this (r16), r4 lpActiveRaceCarOutputInterface (r19),
// f1 lfTimeStep (f31).
void ChallengeManager::UpdateLeaptCars(
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
    f32 lfTimeStep)
{
    CGS_ASSERT(lpActiveRaceCarOutputInterface, "lpActiveRaceCarOutputInterface");   // X360 line 3900

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState* lpPlayerRaceCarState =
        lpActiveRaceCarOutputInterface->GetPlayerRaceCarState();
    CGS_ASSERT(lpPlayerRaceCarState, "lpPlayerRaceCarState");                       // X360 line 3903

    // ---------------- phase 1: off-gas wind-down ----------------
    if (lpPlayerRaceCarState->mfGas <= 0.0f)                                        // lfs +0x404
    {
        if (mbUpdateLeaptCars && miNumCarsLeapt > 0)
        {
            if (mfLeapCarsValidTimer == -1.0f)                                      // flt_820037C8
            {
                mfLeapCarsValidTimer = 0.25f;                                       // flt_82021138
            }

            CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");                   // X360 line 3921

            // Everything EXCEPT the two accumulating co-op types banks the running count every
            // frame while the grace timer runs down. (lbBankImmediately == true: `li r6, 1`.)
            if (mpCurrentChallenge->GetAction(miCurrentChallengeAction)->GetCoopType() !=
                    BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION &&
                mpCurrentChallenge->GetAction(miCurrentChallengeAction)->GetCoopType() !=
                    BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_CUMULATIVE)
            {
                SetCurrentSkillScore(E_FREEBURN_SKILL_LEAP_CARS,
                                     static_cast<f32>(miNumCarsLeapt), true);
            }

            mfLeapCarsValidTimer -= lfTimeStep;
            if (mfLeapCarsValidTimer < 0.0f)
            {
                mfLeapCarsValidTimer = -1.0f;
                mbUpdateLeaptCars = false;

                CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");               // X360 line 3942

                // ...and the two accumulating co-op types bank exactly once, on expiry.
                if (mpCurrentChallenge->GetAction(miCurrentChallengeAction)->GetCoopType() ==
                        BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION ||
                    mpCurrentChallenge->GetAction(miCurrentChallengeAction)->GetCoopType() ==
                        BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_CUMULATIVE)
                {
                    SetCurrentSkillScore(E_FREEBURN_SKILL_LEAP_CARS,
                                         static_cast<f32>(miNumCarsLeapt), true);
                }

                mPotentiallyLeaptCars.Clear();   // occupancy=0, free queue refilled {6..0}, numFree=7
                miNumCarsLeapt = 0;
            }
        }
    }
    else
    {
        mbUpdateLeaptCars = true;
    }

    // ---------------- phase 2: rebuild + match the candidate set ----------------
    if (mbUpdateLeaptCars)
    {
        // Stack-local candidate pool (X360 sp+0xC0, same 0x108-byte ObjectPool shape as the member).
        CgsContainers::ObjectPool<CarLeapingData, 7, s32> lLeapCandidates;
        lLeapCandidates.Clear();

        const Vector3 lPlayerPosition = lpPlayerRaceCarState->mTransform.Pos();

        for (::EActiveRaceCarIndex leActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
             static_cast<s32>(leActiveRaceCarIndex) < E_ACTIVE_RACE_CAR_INDEX_COUNT;
             leActiveRaceCarIndex++)
        {
            // The interface's own range asserts (BrnRaceCarEntityModuleOutputInterface.h:898/899)
            // are inlined into this accessor -- call it, do not duplicate them.
            if (!lpActiveRaceCarOutputInterface->IsRaceCarNetwork(leActiveRaceCarIndex))
            {
                continue;
            }

            CGS_ASSERT(lpActiveRaceCarOutputInterface,
                       "lpActiveRaceCarOutputInterface");                           // X360 line 3970 (re-asserted)

            const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState* lpRaceCarState =
                lpActiveRaceCarOutputInterface->GetRaceCarState(leActiveRaceCarIndex);
            CGS_ASSERT(lpRaceCarState, "lpRaceCarState");                           // X360 line 3972

            if (lpRaceCarState->mbResetCarTransform)
            {
                continue;
            }

            const Vector3 lCarPosition = lpRaceCarState->mTransform.Pos();

            // Height gate: the player's underside must clear the car's roof by 0.25m.
            const f32 lfHeightAboveCar = (lPlayerPosition.y - lCarPosition.y)
                                       - lpPlayerRaceCarState->mHalfExtent.y
                                       - lpRaceCarState->mHalfExtent.y;
            if (lfHeightAboveCar < 0.25f)                                           // flt_82021138
            {
                continue;
            }

            // Horizontal gate: within 10m on the XZ plane (100.0 == flt_820049E0, squared radius).
            const Vector3 lPlayerToCar = lCarPosition - lPlayerPosition;
            if (lPlayerToCar.x * lPlayerToCar.x + lPlayerToCar.z * lPlayerToCar.z > 100.0f)
            {
                continue;
            }

            const s32 liAllocatedIndex = lLeapCandidates.AllocateObject();
            CGS_ASSERT(liAllocatedIndex >= 0, "liAllocatedIndex >= 0");             // X360 line 4001
            if (liAllocatedIndex >= 0)
            {
                lLeapCandidates[liAllocatedIndex].mActiveRaceCarIndex = leActiveRaceCarIndex;
                lLeapCandidates[liAllocatedIndex].mPlayerToCar        = lPlayerToCar;
            }
        }

        // Which persistent slots survive this frame. The X360 keeps this in one stack 64-bit word
        // (zeroed by the single `std r15, sp+0x90`); the "Index: N, Number of bits: 7" StrStream
        // blobs around the sets are the container's own inlined range asserts.
        CgsContainers::BitArray<KI_LEAPT_CAR_POOL_CAPACITY> lCarsStillPotentiallyLeapt;
        lCarsStillPotentiallyLeapt.UnSetAll();

        for (s32 liCandidate = 0; liCandidate < KI_LEAPT_CAR_POOL_CAPACITY; ++liCandidate)
        {
            if (!lLeapCandidates.IsObjectAllocated(liCandidate))
            {
                continue;
            }

            const CarLeapingData& lrCandidate = lLeapCandidates[liCandidate];

            s32 liStored = 0;
            for (; liStored < KI_LEAPT_CAR_POOL_CAPACITY; ++liStored)
            {
                if (mPotentiallyLeaptCars.IsObjectAllocated(liStored) &&
                    mPotentiallyLeaptCars[liStored].mActiveRaceCarIndex == lrCandidate.mActiveRaceCarIndex)
                {
                    break;
                }
            }

            if (liStored < KI_LEAPT_CAR_POOL_CAPACITY)
            {
                // Already tracked: compare the entry vector against the current one on the XZ plane.
                const StoredLeapingData& lrStored = mPotentiallyLeaptCars[liStored];
                const f32 lfEntryDot = lrStored.mLeapRadiusEntryVector.x * lrCandidate.mPlayerToCar.x
                                     + lrStored.mLeapRadiusEntryVector.z * lrCandidate.mPlayerToCar.z;
                if (lfEntryDot >= 0.0f)
                {
                    // Same side of the car as on entry: still only a POTENTIAL leap; keep the slot
                    // (and its ORIGINAL entry vector -- the X360 never rewrites it here).
                    lCarsStillPotentiallyLeapt.SetBit(static_cast<u32>(liStored));
                }
                else
                {
                    // Sign flip: the player crossed over the car. Count the leap and let the slot
                    // fall out of the keep-mask so the sweep below frees it.
                    ++miNumCarsLeapt;
                }
            }
            else
            {
                // Newly inside the leap radius: remember where the player entered from.
                const s32 liAllocatedIndex = mPotentiallyLeaptCars.AllocateObject();
                CGS_ASSERT(liAllocatedIndex >= 0, "liAllocatedIndex >= 0");         // X360 line 4064
                if (liAllocatedIndex >= 0)
                {
                    mPotentiallyLeaptCars[liAllocatedIndex].mActiveRaceCarIndex =
                        lrCandidate.mActiveRaceCarIndex;
                    mPotentiallyLeaptCars[liAllocatedIndex].mLeapRadiusEntryVector =
                        lrCandidate.mPlayerToCar;
                    lCarsStillPotentiallyLeapt.SetBit(static_cast<u32>(liAllocatedIndex));
                }
            }
        }

        // Sweep: anything not re-confirmed this frame is out of range (or has been leapt) -- free it.
        for (s32 liStored = 0; liStored < KI_LEAPT_CAR_POOL_CAPACITY; ++liStored)
        {
            if (mPotentiallyLeaptCars.IsObjectAllocated(liStored) &&
                !lCarsStillPotentiallyLeapt.IsBitSet(static_cast<u32>(liStored)))
            {
                mPotentiallyLeaptCars.FreeObject(liStored);
            }
        }

        SetCurrentSkillScore(E_FREEBURN_SKILL_LEAP_CARS,
                             static_cast<f32>(miNumCarsLeapt), false);              // `li r6, 0`
    }
}

}
