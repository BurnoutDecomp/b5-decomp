#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

#include <cmath>    // std::fabs
#include <cstddef>  // offsetof (layout asserts)

// BrnPhysics::Vehicle::VehicleManager -- takedown impact classification.
// This TU is reconstructed PARTIALLY: it bodies the classifier entry point
// CheckForAllTypesOfImpacts, the commit routine InstantTakedown, and the eight per-type
// sub-classifiers it dispatches to (the full 64-function VehicleManager is built out by its own
// reconstruction passes). The classifier bodies use NAMED access only -- the RaceCarResponseInfo
// fields, the deep VehicleManager tuning members (BrnVehicleManager.h §7 layout), the per-car
// maRaceCarVehicles[idx] records, and the contact-normal/point via mpContact->mNormal/mPointOnA.
// The SIMD plane-geometry sub-tests (IsPointBetweenTwoParallelPlanes,
// CheckForVerticalTakedownSituation) and the recency throttle (HasRaceCarHadRecentImpact) are NOT
// in this dossier -- they are declared-only callees (FLAGged at their use sites).

namespace BrnPhysics
{
namespace Vehicle
{
    // The minimum combined closing speed below which a contact is too gentle to be any kind of
    // takedown/shunt (X360 reads the rodata float at flt_82FB8290 for the `>=` gate).
    // FLAG: the rodata value is not in the per-function exports -- shipped as a flagged 0.0f
    // placeholder (with which the gate is a pass-through). Resolve the real threshold from the
    // XEX .rdata @0x82FB8290 before relying on the gate magnitude.
    static const f32 KF_MIN_IMPACT_SPEED_SUM = 0.0f;   // FLAG: rodata flt_82FB8290 value unrecovered

    // -------------------------------------------------------------------------------------------
    // CheckForAllTypesOfImpacts  @0x82642E58
    //
    // The takedown CLASSIFIER. Given a populated RaceCarResponseInfo, run the per-type
    // sub-classifiers in strict priority order and stop at the first that fires (first-match-wins;
    // each sub-classifier commits its own side effects -- crashing the victim etc. -- when it
    // matches, so this entry point returns void). Priority order (from the X360 asm):
    //   1. player slamming an AI into another AI   2. hitting an already-crashing car
    //   -- the remaining geometric tests only run if NEITHER car is already crashing --
    //   3. vertical   4. T-bone   5. head-to-head   6. shunt/nudge   7. slam/trading-paint
    //   8. stationary-target.
    // A leading energy gate skips classification entirely for gentle contacts
    // (|speedB| + |speedA| must clear KF_MIN_IMPACT_SPEED_SUM).
    //
    // FLAG (signature): the X360 Hex-Rays rendered this `int(int result, int a2)`; the DWARF
    // (BrnVehicleManager.h:1182) gives the true shape `void CheckForAllTypesOfImpacts(
    // RaceCarResponseInfo*)`. The "result" the pseudocode returns is the first-match value, which
    // the declared void signature discards.
    // -------------------------------------------------------------------------------------------
    void VehicleManager::CheckForAllTypesOfImpacts(RaceCarResponseInfo* lpInfo)
    {
        // Energy gate: ignore contacts whose combined closing speed is below the threshold.
        if (std::fabs(lpInfo->mfRaceCarBSpeed) + std::fabs(lpInfo->mfRaceCarASpeed) < KF_MIN_IMPACT_SPEED_SUM)
            return;

        // Highest priority: a player shunting an AI into a third AI, and re-hits on a car that is
        // already crashing -- these run even if a car is mid-crash.
        if (CheckForPlayerSlammingAIIntoAI(lpInfo))    return;
        if (CheckForHittingAlreadyCrashingCar(lpInfo)) return;

        // The geometric classifiers only apply to a fresh impact: a car already crashing cannot be
        // freshly taken down.
        if (lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
            return;

        if (CheckForVerticalTakedown(lpInfo))   return;
        if (CheckForTBoneTakedown(lpInfo))      return;
        if (CheckForHeadToHead(lpInfo))         return;
        if (CheckForShuntAndNudge(lpInfo))      return;
        if (CheckForSlamAndTradingPaint(lpInfo)) return;
        CheckForStationaryTargetTakedown(lpInfo);
    }

    // -------------------------------------------------------------------------------------------
    // The sentinel the per-car crash-state array holds while a car is in the fatal/active crash
    // state. InstantTakedown skips re-crashing the victim when its crash-state already equals this.
    // FLAG: no recovered enum home for the crash-state values -- the X360 compares the array slot
    // against the bare literal 2, so it is reproduced here as a named integer literal (NOT an
    // invented enum). Resolve the real crash-state enum and replace this when its home is recovered.
    static const s32 KI_RACECAR_CRASH_STATE_FATAL = 2;   // FLAG: literal sentinel; enum home unrecovered

    // -------------------------------------------------------------------------------------------
    // InstantTakedown  @0x82636108
    //
    // The takedown COMMIT routine the impact classifiers call once a takedown is decided. It:
    //   1. decodes the victim and aggressor EntityIds to active-car indices (the recurring
    //      `(muValue >> 10) & 0x3FFF` packing used TU-wide);
    //   2. does nothing unless takedowns are enabled (the master gate at mbTakedownsEnabled);
    //   3. crashes the victim via SetRaceCarCrashing -- UNLESS the victim is already in the fatal
    //      crash state -- forwarding the collision normal + contact point, the four output/
    //      deformation interfaces, and the takedown type (lfNormalStressSq is NOT forwarded);
    //   4. if the victim is the local player, zeroes the aggressor's per-car recovery timer;
    //   5. records the aggressor (via miAttackerToRecord) as the victim's last attacker, and marks
    //      the aggressor's "taken down this frame" status byte.
    //
    // INDEXING NOTE (asm-authoritative, surprising): the crash-state check and the last-attacker
    // write are indexed by the VICTIM slot, while the recovery-timer zero and the taken-down status
    // byte are indexed by the AGGRESSOR slot. This matches the X360 exactly (5216*v39 and 224*v39
    // use the aggressor index v39; 4*(v38+...) use the victim index v38) -- reproduced verbatim.
    //
    // FLAG (signature): the X360 Hex-Rays rendered this with a 37-arg `int(...)` prototype -- an
    // artefact of SIMD-spilled Vector3s and pass-through registers. The DWARF (BrnVehicleManager.h
    // :1257) gives the true 10-parameter shape used here; the returned `HIDWORD(a1)` the pseudocode
    // produces is the SetRaceCarCrashing result threaded back through r4, which the void return drops.
    // -------------------------------------------------------------------------------------------
    void VehicleManager::InstantTakedown(EntityId lVictimEntityId,
                                         EntityId lAggressorEntityId,
                                         Vector3 lCollisionNormal,
                                         Vector3 lContactPoint,
                                         f32 lfNormalStressSq,
                                         BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
                                         VehicleManagerOutputInterface* lpManagerOutputInterface,
                                         BrnGameState::GameStateModuleIO::VehicleOutputInterface* lpVehicleOutputInterface,
                                         BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                                         BrnGameState::ETakedownType leTakedownType)
    {
        // lfNormalStressSq is decided by the classifier but not used by the commit (the X360 never
        // forwards a3); reference it so the unused parameter is explicit rather than a warning.
        (void)lfNormalStressSq;

        // Decode both entities to their active-car slots (TU-wide packing: bits 10..23 of muValue).
        const s32 liVictimActiveRaceCarIndex    = static_cast<s32>((lVictimEntityId.muValue    >> 10) & 0x3FFF);
        const s32 liAggressorActiveRaceCarIndex = static_cast<s32>((lAggressorEntityId.muValue >> 10) & 0x3FFF);

        // Master gate: do nothing at all unless takedowns are currently enabled.
        if (!mbTakedownsEnabled)
            return;

        // Crash the victim, unless it is already in the fatal/active-crash state.
        if (maRaceCarCrashState[liVictimActiveRaceCarIndex] != KI_RACECAR_CRASH_STATE_FATAL)
        {
            SetRaceCarCrashing(lVictimEntityId,
                               lAggressorEntityId,
                               lCollisionNormal,
                               lContactPoint,
                               lpRequestOutputInterface,
                               lpManagerOutputInterface,
                               lpVehicleOutputInterface,
                               lpDeformationInterface,
                               leTakedownType);
        }

        // If the player was the one taken down, reset the aggressor's recovery timer.
        if (mePlayerActiveRaceCarIndex == liVictimActiveRaceCarIndex)
            maRaceCarVehicles[liAggressorActiveRaceCarIndex].mfRecoveryTimer = 0.0f;

        // Record who took the victim down, and flag the aggressor as having scored a takedown this frame.
        maRaceCarLastAttacker[liVictimActiveRaceCarIndex]   = miAttackerToRecord;
        maRaceCarStatus[liAggressorActiveRaceCarIndex].mbTakenDown = 1;
    }

    // ===========================================================================================
    // The eight per-type sub-classifiers (run in priority order by CheckForAllTypesOfImpacts).
    //
    // Conventions used below:
    //  - "named access only": the asm's raw offsets are resolved to the RaceCarResponseInfo fields
    //    (a2+92 == mfRaceCarASpeed, a2+96 == mfRaceCarBSpeed, a2+240 == mfAngleBetweenCars, etc.),
    //    the §7 deep VehicleManager tuning members, and maRaceCarVehicles[idx]. The collision
    //    normal / contact point the InstantTakedown calls pass-by-VMX-register are the contact's
    //    mpContact->mNormal (+48) and mpContact->mPointOnA (+64).
    //  - asm-visible immediates (pi/2, pi/180, 5.0, 0.04, 1.9, 180.0) are used directly; rodata
    //    floats whose VALUES are not in the per-function exports are file-static KF_/KVF_
    //    placeholders, FLAGged.
    //  - the SIMD plane-geometry helpers and the recency throttle are declared-only callees.
    // ===========================================================================================

    // ---- shared rodata-value placeholders (FLAG: values not in the per-function exports) -------
    // The global speed-unit scale that multiplies every tuning speed threshold (flt_82F31928).
    static const f32 KF_SPEED_UNIT_SCALE       = 1.0f;   // FLAG: rodata flt_82F31928 value unrecovered (1.0 = identity)
    // Stationary-target speed thresholds (flt_82FB8298 asymmetry gate, 829C slow cap, 7F18 fast floor).
    static const f32 KF_STATIONARY_MIN_SPEED_DIFF = 0.0f; // FLAG: rodata flt_82FB8298 value unrecovered
    static const f32 KF_STATIONARY_SLOW_CAP        = 0.0f; // FLAG: rodata flt_82FB829C value unrecovered
    static const f32 KF_STATIONARY_FAST_FLOOR      = 0.0f; // FLAG: rodata flt_82FB7F18 value unrecovered
    // Trading-paint alignment-dot gate (unk_82FB8310). A dot >= this means the cars are aligned
    // enough to be side-by-side rather than a true crossing impact.
    static const f32 KF_PAINT_ALIGNMENT_GATE   = 0.0f;   // FLAG: rodata unk_82FB8310 value unrecovered

    // EntityId packing helper: re-encode an active-race-car index into the EntityId word the
    // commit routines decode. The X360 spells this `(luEntityIndex << 10) | 0x1000000` (the
    // 0x1000000 type bits == E_ENTITYTYPE_RACECAR; the index occupies bits 10..23). Matches the
    // inline encode in the heavier classifiers.
    static inline EntityId MakeRaceCarEntityId(u32 luEntityIndex)
    {
        return EntityId{ (luEntityIndex << 10) | 0x1000000u };
    }

    // -------------------------------------------------------------------------------------------
    // CheckForTBoneTakedown  @0x8263D480  ->  T_BONE
    //
    // Perpendicular hit. Gates: neither car already crashing; the angle between the cars is within
    // an asm band of pi/2: |mfAngleBetweenCars - pi/2| < mfTBoneAngleBandDegrees * (pi/180). Then a
    // side-plane containment test (IsPointBetweenTwoParallelPlanes, half-width
    // mfTBoneSidePlaneHalfWidth * scale) decides which car is the victim. Commits T_BONE via
    // InstantTakedown with the contact normal/point.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForTBoneTakedown(RaceCarResponseInfo* lpInfo)
    {
        // Already-crashing cars can't be freshly T-boned (asm: a2+80 / a2+81).
        if (lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
            return false;

        // asm v4 = the point lies inside car A's side-plane pair; asm v6 = inside car B's. The
        // takedown then fires with a fixed entity-id order (below). FLAG: which physical car is the
        // VICTIM vs the AGGRESSOR depends on IsPointBetweenTwoParallelPlanes' plane orientation
        // (unrecovered); we reproduce the asm's literal id-load order rather than re-deriving it.
        bool lbInPlanesA = false;  // asm v4 (planes built from car A's record, 5216*indexA)
        bool lbInPlanesB = false;  // asm v6 (planes built from car B's record, 5216*indexB)

        // Perpendicularity band: |angle - pi/2| < band(deg) * (pi/180).
        const f32 lfPiOver2 = 1.5707964f;
        const f32 lfDegToRad = 0.017453292f;
        if (std::fabs(std::fabs(lpInfo->mfAngleBetweenCars) - lfPiOver2)
            < mfTBoneAngleBandDegrees * lfDegToRad)
        {
            const f32 lfHalfWidth = mfTBoneSidePlaneHalfWidth * KF_SPEED_UNIT_SCALE; // FLAG: scale rodata flt_82F31928

            // The X360 builds a parallel-plane pair from a car's transform basis + the half-width and
            // tests the contact point against it (twice: once per car). The plane-construction VMX
            // math is delegated to IsPointBetweenTwoParallelPlanes (declared-only -- not in this
            // dossier). FLAG: the two plane vectors are approximated by the car's forward (At) axis
            // and a half-width-scaled lane; the exact VMX plane basis is unrecovered. The asm runs
            // the A-record test first; only if it fails does it run the B-record test.
            const Vector3 lvPoint  = lpInfo->mpContact->mPointOnA;
            const Vector3 lvWidth  = Vector3{ lfHalfWidth, lfHalfWidth, lfHalfWidth, 0.0f };
            if (IsPointBetweenTwoParallelPlanes(lvPoint, lpInfo->mRaceCarATransform.At(), lvWidth))
            {
                lbInPlanesA = true;
            }
            else if (IsPointBetweenTwoParallelPlanes(lvPoint, lpInfo->mRaceCarBTransform.At(), lvWidth))
            {
                lbInPlanesB = true;
            }
        }

        if (!lbInPlanesA && !lbInPlanesB)
            return false;

        // Entity-id order, verbatim from the asm: the v4 (A-planes) path loads victim=idB,
        // aggressor=idA; the v6 (B-planes) path loads victim=idA, aggressor=idB.
        const EntityId lVictim    = lbInPlanesA ? lpInfo->mRaceCarBEntityID : lpInfo->mRaceCarAEntityID;
        const EntityId lAggressor = lbInPlanesA ? lpInfo->mRaceCarAEntityID : lpInfo->mRaceCarBEntityID;

        InstantTakedown(lVictim, lAggressor,
                        lpInfo->mpContact->mNormal,
                        lpInfo->mpContact->mPointOnA,
                        lpInfo->mfNormalStressSq,
                        lpInfo->mpRequestOutputInterface,
                        lpInfo->mpManagerOutputInterface,
                        lpInfo->mpVehicleOutputInterface,
                        lpInfo->mpDeformationInterface,
                        BrnGameState::E_TAKEDOWN_T_BONE);
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // CheckForHeadToHead  @0x8263D1A0  ->  HEAD_ON (the only classifier that BOTH shoves AND crashes)
    //
    // Gates: neither car crashing; |angle| >= (180 - tolerance) * (pi/180); at least one car's
    // speed >= minClosingSpeed * scale. Decides the loser by scaled speed, multiplies the closing
    // magnitude by 5.0, sets impact type 4 (E_IMPACT_SHUNT), applies a shove (ApplyShunt), and
    // commits HEAD_ON.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForHeadToHead(RaceCarResponseInfo* lpInfo)
    {
        if (lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
            return false;

        const f32 lfDegToRad = 0.017453292f;
        // Near-180 band: bail if the cars are not nearly head-on.
        if (std::fabs(lpInfo->mfAngleBetweenCars) < (180.0f - mfHeadToHeadAngleToleranceDeg) * lfDegToRad)
            return false;

        // At least one car must clear the min closing speed (* scale).
        const f32 lfMinSpeed = mfHeadToHeadMinClosingSpeed * KF_SPEED_UNIT_SCALE; // FLAG: scale rodata flt_82F31928
        const f32 lfSpeedA = lpInfo->mfRaceCarASpeed;   // asm a2+23 (==+0x5C)
        const f32 lfSpeedB = lpInfo->mfRaceCarBSpeed;   // asm a2+24 (==+0x60)
        if (lfSpeedA <= lfMinSpeed && lfSpeedB <= lfMinSpeed)
            return false;

        // The X360 scales each car's speed by a per-car transform-derived lane (vmulfp of the speed
        // against the a2[9]/a2[10] transform columns) and compares the two scaled values; the
        // branch that wins applies the shove. Here we compare the raw speeds, which is the asm's
        // decision input once the scale lanes cancel.
        // FLAG: the per-car scale lane (a2[9]/a2[10] transform column * speed) is approximated by
        // the raw speeds; the exact VMX scaling is not reconstructed.
        //
        // The two candidate entity ids the asm later passes to InstantTakedown are the CONTACT's own
        // id fields (mpContact->mEntityIdA == **a2, mEntityIdB == (*a2)[1]); the (v18,v19) pair is
        // assigned in a branch-specific order and the final victim/aggressor split is decided below
        // by the situation byte + a numeric-id ordering test. We read those ids by name.
        const EntityId lContactIdA = lpInfo->mpContact->mEntityIdA; // asm **a2
        const EntityId lContactIdB = lpInfo->mpContact->mEntityIdB; // asm (*a2)[1]

        bool        lbFired = false;
        bool        lbPlayerWon = false; // asm a2[258] (mbPlayerWonImpact lane)
        EntityId    lId0{};   // asm v18
        EntityId    lId1{};   // asm v19
        if (lfSpeedB > lfSpeedA)
        {
            // asm: !a2+85 (B not network) guards this branch.
            if (!lpInfo->mbRaceCarBIsNetworkCar)
            {
                lbPlayerWon = lpInfo->mbRaceCarAIsPlayer; // asm v15 = *(a2+82)
                lId0 = lContactIdB;  // asm v18 = (*a2)[1]
                lId1 = lContactIdA;  // asm v19 = **a2
                lpInfo->mfClosingSpeed *= 5.0f;         // asm *(a2+22) == word 22 == byte 88 == mfClosingSpeed
                lpInfo->mbPlayerWonImpact = lbPlayerWon; // asm *(a2+258) = v15
                lpInfo->meAggressorActiveRaceCarIndex = lpInfo->meActiveRaceCarIndexA; // asm a2[62]=a2[7]
                lpInfo->meVictimActiveRaceCarIndex    = lpInfo->meActiveRaceCarIndexB; // asm a2[63]=a2[8]
                lpInfo->meImpactType = E_IMPACT_SHUNT;  // asm a2[61] = 4
                lbFired = true;
                ApplyShunt(lpInfo);
            }
        }
        else if (lfSpeedB < lfSpeedA)
        {
            // asm: !a2+84 (A not network) guards this branch.
            if (!lpInfo->mbRaceCarAIsNetworkCar)
            {
                lbPlayerWon = lpInfo->mbRaceCarBIsPlayer; // asm v20 = *(a2+83)
                lId0 = lContactIdA;  // asm v18 = **a2
                lId1 = lContactIdB;  // asm v19 = (*a2)[1]
                lpInfo->mfClosingSpeed *= 5.0f;
                lpInfo->mbPlayerWonImpact = lbPlayerWon;
                lpInfo->meAggressorActiveRaceCarIndex = lpInfo->meActiveRaceCarIndexB; // asm a2[62]=a2[8]
                lpInfo->meVictimActiveRaceCarIndex    = lpInfo->meActiveRaceCarIndexA; // asm a2[63]=a2[7]
                lpInfo->meImpactType = E_IMPACT_SHUNT;
                lbFired = true;
                ApplyShunt(lpInfo);
            }
        }
        else
        {
            // Exact speed tie: the asm marks both cars' "already handled" bytes and returns 1
            // without crashing anyone.
            if (!lpInfo->mbRaceCarBIsNetworkCar) lpInfo->mbCrashRaceCarB = true; // asm *(a2+257)=1 (B path)
            if (!lpInfo->mbRaceCarAIsNetworkCar) lpInfo->mbCrashRaceCarA = true; // asm *(a2+256)=1 (A path)
            return true;
        }

        // The asm only commits a crash when the shove fired AND at least one car is a network car
        // (a2+85 || a2+84), AND the player-won byte is set; it then orders victim/aggressor by the
        // numeric id comparison v19 < v18 (or the fallback v18 < v19).
        if (!lbFired || !(lpInfo->mbRaceCarBIsNetworkCar || lpInfo->mbRaceCarAIsNetworkCar))
            return lbFired;   // shove applied but no crash committed (still "handled" -> 1)
        if (!lbPlayerWon)
            return true;

        // asm: victim = the larger id, aggressor = the smaller (the v19 < v18 / v18 < v19 split).
        EntityId lVictim;
        EntityId lAggressor;
        if (lId1.muValue < lId0.muValue)      { lVictim = lId0; lAggressor = lId1; } // asm LABEL_20 (v18 victim)
        else if (lId0.muValue < lId1.muValue) { lVictim = lId1; lAggressor = lId0; }
        else                                  return true; // equal ids -> no commit

        InstantTakedown(lVictim, lAggressor,
                        lpInfo->mpContact->mNormal,
                        lpInfo->mpContact->mPointOnA,
                        lpInfo->mfNormalStressSq,
                        lpInfo->mpRequestOutputInterface,
                        lpInfo->mpManagerOutputInterface,
                        lpInfo->mpVehicleOutputInterface,
                        lpInfo->mpDeformationInterface,
                        BrnGameState::E_TAKEDOWN_HEAD_ON);
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // CheckForStationaryTargetTakedown  @0x8263D948  ->  STANDARD (taking down a near-stopped car)
    //
    // Gates: neither car crashing; the master gate mbStationaryTakedownsEnabled; the two cars are
    // close (squared distance between their positions < 0.04); a speed asymmetry
    // (|speedA - speedB| >= flt_82FB8298, the slower car <= flt_82FB829C, the faster >= flt_82FB7F18).
    // No shove (the victim is stationary) -- commits straight to InstantTakedown.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForStationaryTargetTakedown(RaceCarResponseInfo* lpInfo)
    {
        if (lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
            return false;
        if (!mbStationaryTakedownsEnabled)
            return false;

        // Proximity: squared distance between the two car positions must be < 0.04 (asm immediate
        // 0.040000003). The asm subtracts the +160 / +224 transform lanes -- the position (Pos)
        // axes of mRaceCarA/BTransform.
        const Vector3 lvPosA = lpInfo->mRaceCarATransform.Pos();
        const Vector3 lvPosB = lpInfo->mRaceCarBTransform.Pos();
        const f32 ldx = lvPosA.x - lvPosB.x;
        const f32 ldy = lvPosA.y - lvPosB.y;
        const f32 ldz = lvPosA.z - lvPosB.z;
        const f32 lfDistSq = ldx * ldx + ldy * ldy + ldz * ldz;
        // asm: vcmpgtfp (0.04 > distSq) must hold; otherwise bail. Equivalent to distSq < 0.04.
        if (lfDistSq >= 0.040000003f)
            return false;

        // Speed asymmetry. The asm reads a2[23]/a2[24] (mfRaceCarASpeed/BSpeed).
        const f32 lfSpeedA = lpInfo->mfRaceCarASpeed;
        const f32 lfSpeedB = lpInfo->mfRaceCarBSpeed;
        if (std::fabs(lfSpeedA - lfSpeedB) < KF_STATIONARY_MIN_SPEED_DIFF) // FLAG: rodata flt_82FB8298
            return false;

        EActiveRaceCarIndex leVictim;
        EActiveRaceCarIndex leAggressor;
        EntityId lVictimId;
        EntityId lAggressorId;
        bool     lbPlayerWon;
        if (lfSpeedA <= lfSpeedB)
        {
            // A is the slower (stationary) VICTIM; B is the faster aggressor. (asm: speedA<=speedB)
            // asm: stamps victim(+63)=v16=indexA, aggressor(+62)=v15=indexB; InstantTakedown victim=idA.
            if (lfSpeedA > KF_STATIONARY_SLOW_CAP || lfSpeedB < KF_STATIONARY_FAST_FLOOR) // FLAG: rodata 829C / 7F18
                return false;
            leVictim    = lpInfo->meActiveRaceCarIndexA; // asm v16 = a2+7  -> victim slot a2[63]
            leAggressor = lpInfo->meActiveRaceCarIndexB; // asm v15 = a2+8  -> aggressor slot a2[62]
            lVictimId    = lpInfo->mRaceCarAEntityID;    // asm v18 = a2+5
            lAggressorId = lpInfo->mRaceCarBEntityID;    // asm v17 = a2+6
            lbPlayerWon  = lpInfo->mbRaceCarBIsPlayer;   // asm v14 = *(a2+83)
        }
        else
        {
            // B is the slower (stationary) VICTIM; A is the faster aggressor.
            if (lfSpeedB > KF_STATIONARY_SLOW_CAP || lfSpeedA < KF_STATIONARY_FAST_FLOOR) // FLAG: rodata 829C / 7F18
                return false;
            leVictim    = lpInfo->meActiveRaceCarIndexB;
            leAggressor = lpInfo->meActiveRaceCarIndexA;
            lVictimId    = lpInfo->mRaceCarBEntityID;
            lAggressorId = lpInfo->mRaceCarAEntityID;
            lbPlayerWon  = lpInfo->mbRaceCarAIsPlayer;   // asm v14 = *(a2+82)
        }

        // Stamp the impact bookkeeping the asm writes before committing.
        lpInfo->meVictimActiveRaceCarIndex    = leVictim;    // asm *(_R11+63)=v16
        lpInfo->meAggressorActiveRaceCarIndex = leAggressor; // asm *(_R11+62)=v15
        lpInfo->mbPlayerWonImpact             = lbPlayerWon;  // asm *(_R11+258)=v14

        InstantTakedown(lVictimId, lAggressorId,
                        lpInfo->mpContact->mNormal,
                        lpInfo->mpContact->mPointOnA,
                        lpInfo->mfNormalStressSq,
                        lpInfo->mpRequestOutputInterface,
                        lpInfo->mpManagerOutputInterface,
                        lpInfo->mpVehicleOutputInterface,
                        lpInfo->mpDeformationInterface,
                        BrnGameState::E_TAKEDOWN_STANDARD);
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // CheckForShuntAndNudge  @0x8261A3A0  ->  FORCE ONLY (no crash). Recency-gated.
    //
    // Bails if either car has had a recent impact. Alignment must be below 1.9 (too head-on belongs
    // to head-to-head). Picks the aggressor/victim by a closing-velocity sign test, then -- if the
    // victim is not already being slammed/shunted -- classifies the contact as nudge (closing <=
    // mfNudgeMaxClosingSpeed*scale, type 2) or shunt (<= mfShuntMaxClosingSpeed*scale, type 4),
    // promoting shunt->boost-shunt (6) when the victim is boost-eligible. Returns 1; never crashes.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForShuntAndNudge(RaceCarResponseInfo* lpInfo)
    {
        // Recency throttle on BOTH cars (asm a2[8] then a2[7]). FLAG: HasRaceCarHadRecentImpact body
        // not in this dossier -- declared-only callee.
        if (HasRaceCarHadRecentImpact(static_cast<s32>(lpInfo->meActiveRaceCarIndexB)))
            return false;
        if (HasRaceCarHadRecentImpact(static_cast<s32>(lpInfo->meActiveRaceCarIndexA)))
            return false;

        // Alignment gate: the sum of the two cars' forward-axis dots with the contact normal must
        // be below 1.9 (asm immediate). A higher value means the contact is too head-on.
        // FLAG: the asm forms two vmsum3fp dots of the contact normal (mpContact+48) against the
        // two car transform forward columns (a2+208 / a2+144). Modelled with named Vector3 dots.
        const Vector3 lvNormal = lpInfo->mpContact->mNormal;
        const Vector3 lvFwdA   = lpInfo->mRaceCarATransform.At();   // asm a2+144 region
        const Vector3 lvFwdB   = lpInfo->mRaceCarBTransform.At();   // asm a2+208 region
        const f32 lfDotA = std::fabs(lvNormal.x * lvFwdA.x + lvNormal.y * lvFwdA.y + lvNormal.z * lvFwdA.z);
        const f32 lfDotB = std::fabs(lvNormal.x * lvFwdB.x + lvNormal.y * lvFwdB.y + lvNormal.z * lvFwdB.z);
        if (1.9f < (lfDotA + lfDotB))
            return false;

        // Aggressor/victim by the closing-velocity sign (asm subtracts the +160/+224 position lanes
        // and dots against the forward column; >= 0 picks A as aggressor, else B).
        const f32 ldx = lpInfo->mRaceCarATransform.Pos().x - lpInfo->mRaceCarBTransform.Pos().x;
        const f32 ldy = lpInfo->mRaceCarATransform.Pos().y - lpInfo->mRaceCarBTransform.Pos().y;
        const f32 ldz = lpInfo->mRaceCarATransform.Pos().z - lpInfo->mRaceCarBTransform.Pos().z;
        const f32 lfApproach = ldx * lvFwdB.x + ldy * lvFwdB.y + ldz * lvFwdB.z;

        const EActiveRaceCarIndex leA = lpInfo->meActiveRaceCarIndexA; // asm v20 = _R31[7]
        const EActiveRaceCarIndex leB = lpInfo->meActiveRaceCarIndexB; // asm v21 = _R31[8]
        if (lfApproach >= 0.0f)
        {
            lpInfo->meAggressorActiveRaceCarIndex = leA;        // asm _R31[62]=v20
            lpInfo->meVictimActiveRaceCarIndex    = leB;        // asm _R31[63]=v21
            lpInfo->mbPlayerWonImpact = lpInfo->mbRaceCarAIsPlayer; // asm v22 = *(_R31+82)
        }
        else
        {
            lpInfo->meAggressorActiveRaceCarIndex = leB;        // asm _R31[62]=v21 ; [63]=v20 (swapped)
            lpInfo->meVictimActiveRaceCarIndex    = leA;
            lpInfo->mbPlayerWonImpact = lpInfo->mbRaceCarBIsPlayer; // asm v22 = *(_R31+83)
        }

        // If the victim is already being slammed/shunted by a race car, this contact is redundant.
        // FLAG: IsBeingSlamedOrShuntedByRaceCar takes the victim's RaceCarPhysics record + the
        // aggressor active-index; modelled via maRaceCarVehicles[victim] cast to RaceCarPhysics*.
        // (Declared on VehiclePhysics, not in this TU -- left as a structural no-op gate here so the
        // classification still resolves; the real call belongs to the VehiclePhysics home.)
        // -> we cannot call into VehiclePhysics from this minimal slice, so the slam/shunt-already
        //    guard is documented but not invoked. FLAG: redundancy guard omitted (cross-TU callee).

        const f32 lfClosing = lpInfo->mfClosingSpeed; // asm *(_R31+22) == word 22 == byte 88 == mfClosingSpeed
        if (lfClosing <= (mfShuntMaxClosingSpeed * KF_SPEED_UNIT_SCALE)) // FLAG: scale rodata flt_82F31928
        {
            EImpactType leType = E_IMPACT_SHUNT;                       // asm v25 = 4
            if (lfClosing <= (mfNudgeMaxClosingSpeed * KF_SPEED_UNIT_SCALE)) // FLAG: scale rodata
                leType = E_IMPACT_NUDGE;                               // asm v25 = 2
            lpInfo->meImpactType = leType;                             // asm _R31[61] = v25
            // Promote a plain shunt to a boost-shunt when the aggressor is boost-eligible (+123).
            const s32 liAggressor = static_cast<s32>(lpInfo->meAggressorActiveRaceCarIndex);
            if (maRaceCarStatus[liAggressor].mbBoostImpactEligible && lpInfo->meImpactType == E_IMPACT_SHUNT)
                lpInfo->meImpactType = E_IMPACT_BOOST_SHUNT;          // asm _R31[61] = 6
            return true;
        }

        // Too fast for shunt/nudge -- mark both cars handled and report consumed (no crash).
        if (!lpInfo->mbCrashRaceCarA) lpInfo->mbCrashRaceCarA = true;  // asm *(_R31+256)=1
        if (!lpInfo->mbCrashRaceCarB) lpInfo->mbCrashRaceCarB = true;  // asm *(_R31+257)=1
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // CheckForSlamAndTradingPaint  @0x82619F30  ->  FORCE ONLY (no crash). The lightest tier.
    //
    // Recency-gated. The contact-normal alignment must clear the paint-alignment gate
    // (unk_82FB8310). The combined energy must fall in the band [mfTradingPaintMinSpeed,
    // mfTradingPaintMaxSpeed] (* scale). Sets impact severity 1/3/5 (TRADING_PAINT / SLAM /
    // BOOST_SLAM), stores a slam vector, returns 1. No crash.
    //
    // FLAG: this classifier's full body reads several RaceCarPhysics in-record fields the asm
    // reaches via 5216*idx + 5124 (a per-car slam accumulator) plus the aggressive-driving timer
    // tables (+171936 / +171944) and a rodata speed-curve table (flt_82F2A218). Those reads are
    // NOT placeable with named access in this minimal slice (the RaceCarPhysics layout +5124 field
    // and the aggressive-driving tables are unmodelled). The load-bearing CLASSIFICATION decision
    // (recency gate, alignment gate, energy band, severity) is reconstructed with named access; the
    // in-record slam-accumulator comparison and the aggressive-driving timer veto are documented
    // and FLAGged as omitted cross-record reads.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForSlamAndTradingPaint(RaceCarResponseInfo* lpInfo)
    {
        if (lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
            return false;

        // Recency throttle on both cars (asm reads a2+28 / a2+32 == the active-index fields).
        if (HasRaceCarHadRecentImpact(static_cast<s32>(lpInfo->meActiveRaceCarIndexA)))
            return false;
        if (HasRaceCarHadRecentImpact(static_cast<s32>(lpInfo->meActiveRaceCarIndexB)))
            return false;

        // Alignment gate: the contact-normal dot must clear the paint-alignment threshold.
        // FLAG: the asm dots two transform-derived lanes (v123 . v122); modelled as the contact
        // normal vs the A-car forward axis.
        const Vector3 lvNormal = lpInfo->mpContact->mNormal;
        const Vector3 lvFwdA   = lpInfo->mRaceCarATransform.At();
        const f32 lfAlignDot = lvNormal.x * lvFwdA.x + lvNormal.y * lvFwdA.y + lvNormal.z * lvFwdA.z;
        if (KF_PAINT_ALIGNMENT_GATE > lfAlignDot) // FLAG: rodata unk_82FB8310
            return false;

        // Energy band [min, max] (* scale). The asm reads *(_R31+88) == mfClosingSpeed.
        const f32 lfEnergy = lpInfo->mfClosingSpeed;
        if (lfEnergy <= (mfTradingPaintMinSpeed * KF_SPEED_UNIT_SCALE)) // FLAG: scale rodata flt_82F31928
            return false;
        if (lfEnergy > (mfTradingPaintMaxSpeed * KF_SPEED_UNIT_SCALE))  // FLAG: scale rodata flt_82F31928
        {
            // Above the band -> mark both cars handled, report consumed (no crash).
            if (!lpInfo->mbCrashRaceCarA) lpInfo->mbCrashRaceCarA = true; // asm *(_R31+256)=1
            if (!lpInfo->mbCrashRaceCarB) lpInfo->mbCrashRaceCarB = true; // asm *(_R31+257)=1
            return true;
        }

        // In-band: the asm picks the slammer by an in-record slam-accumulator comparison (+5124)
        // gated by the aggressive-driving timers, then sets severity 1 (TRADING_PAINT) or 3 (SLAM),
        // promoting SLAM->BOOST_SLAM (5) when the slammer is boost-eligible (+123).
        // FLAG: the slam-accumulator (+5124) + aggressive-driving timer (+171936/+171944) reads are
        // omitted cross-record/table reads; the severity here defaults to TRADING_PAINT and is
        // promoted only by the boost-eligible byte we DO model.
        const s32 liA = static_cast<s32>(lpInfo->meActiveRaceCarIndexA);
        lpInfo->meAggressorActiveRaceCarIndex = lpInfo->meActiveRaceCarIndexA; // asm *(_R31+248)=v27
        lpInfo->meVictimActiveRaceCarIndex    = lpInfo->meActiveRaceCarIndexB; // asm *(_R31+252)=v29
        EImpactType leSeverity = E_IMPACT_TRADING_PAINT;                       // asm *(_R31+244)=1
        if (maRaceCarStatus[liA].mbBoostImpactEligible)
            leSeverity = E_IMPACT_BOOST_SLAM;                                  // asm *(_R31+244)=5 (3->5 promote)
        lpInfo->meImpactType = leSeverity;
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // CheckForVerticalTakedown  @0x8263D728  ->  VERTICAL
    //
    // Early-outs if BOTH cars had a recent impact. Then for each candidate victim it calls the
    // sibling CheckForVerticalTakedownSituation (the up-axis geometry test) and an up-axis height
    // comparison (the asm compares a transform +4192 lane against rodata unk_82FB82A0, then equality
    // against 0). Commits VERTICAL.
    //
    // FLAG: CheckForVerticalTakedownSituation is a declared-only callee (not in this dossier). The
    // up-axis height lanes the asm reads at in-record +4192 are part of the unmodelled RaceCarPhysics
    // layout; the height comparison is delegated to the situation helper rather than reconstructed
    // inline, and the rodata unk_82FB82A0 vector is unrecovered.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForVerticalTakedown(RaceCarResponseInfo* lpInfo)
    {
        // Early-out only when BOTH cars are recency-blocked (asm: && of the two recency checks).
        if (HasRaceCarHadRecentImpact(static_cast<s32>(lpInfo->meActiveRaceCarIndexB))
            && HasRaceCarHadRecentImpact(static_cast<s32>(lpInfo->meActiveRaceCarIndexA)))
            return false;

        RaceCarPhysics* const lpVehB = reinterpret_cast<RaceCarPhysics*>(&maRaceCarVehicles[static_cast<s32>(lpInfo->meActiveRaceCarIndexB)]);
        RaceCarPhysics* const lpVehA = reinterpret_cast<RaceCarPhysics*>(&maRaceCarVehicles[static_cast<s32>(lpInfo->meActiveRaceCarIndexA)]);

        bool lbFired = false;
        EntityId lVictimId{};
        EntityId lAggressorId{};

        // Candidate 1: B above A. The situation helper + height test decide.
        if (CheckForVerticalTakedownSituation(lpVehB, lpVehA))
        {
            // FLAG: the up-axis height comparison (transform +4192 vs unk_82FB82A0, then ==0) is
            // delegated to the situation helper result -- the in-record height lane is unmodelled.
            lbFired = true;
            lVictimId    = lpInfo->mRaceCarAEntityID; // asm v10 = **a2 ; v8 = *(*a2+4)
            lAggressorId = lpInfo->mRaceCarBEntityID;
        }

        // Candidate 2: A above B. (asm runs BOTH situation tests unconditionally; we short-circuit
        // on the first match since a second match would only overwrite the same victim/aggressor
        // pair. FLAG: the asm's exact victim-id selection (v8/v26 across both blocks) is inferred.)
        if (!lbFired && CheckForVerticalTakedownSituation(lpVehA, lpVehB))
        {
            lbFired = true;
            lVictimId    = lpInfo->mRaceCarBEntityID;
            lAggressorId = lpInfo->mRaceCarAEntityID;
        }

        if (!lbFired)
            return false;

        InstantTakedown(lVictimId, lAggressorId,
                        lpInfo->mpContact->mNormal,
                        lpInfo->mpContact->mPointOnA,
                        lpInfo->mfNormalStressSq,
                        lpInfo->mpRequestOutputInterface,
                        lpInfo->mpManagerOutputInterface,
                        lpInfo->mpVehicleOutputInterface,
                        lpInfo->mpDeformationInterface,
                        BrnGameState::E_TAKEDOWN_VERTICAL);
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // CheckForPlayerSlammingAIIntoAI  @0x8263E000  ->  STANDARD (domino). Highest priority.
    //
    // The player rams one AI into a second AI. Requires both cars active AI (maRaceCarCrashState==1),
    // neither crashing, and (via per-record attacker bookkeeping vs mePlayerActiveRaceCarIndex) that
    // the player is the slammer. Calls ShouldRaceCarCrashOnCarImpact per victim and commits each
    // that passes.
    //
    // FLAG: the asm reads several RaceCarPhysics in-record attacker fields (5216*idx + 6944 / +6288
    // / +6032, and the v13+4432 / +4176 lanes) to confirm the player is the current attacker of the
    // car being shunted. Those are unmodelled RaceCarPhysics fields; the player-is-slammer
    // confirmation is delegated to ShouldRaceCarCrashOnCarImpact (declared-only) -- the standalone
    // attacker-field reads are documented and omitted here.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForPlayerSlammingAIIntoAI(RaceCarResponseInfo* lpInfo)
    {
        const s32 liA = static_cast<s32>(lpInfo->meActiveRaceCarIndexA); // asm v8 = a2[7]
        const s32 liB = static_cast<s32>(lpInfo->meActiveRaceCarIndexB); // asm v9 = a2[8]

        // Both cars must be active AI (crash-state == 1) and neither crashing.
        if (maRaceCarCrashState[liA] != 1)
            return false;
        if (maRaceCarCrashState[liB] != 1 || lpInfo->mbRaceCarAIsCrashing || lpInfo->mbRaceCarBIsCrashing)
            return false;

        RaceCarPhysics* const lpVehA = reinterpret_cast<RaceCarPhysics*>(&maRaceCarVehicles[liA]);
        RaceCarPhysics* const lpVehB = reinterpret_cast<RaceCarPhysics*>(&maRaceCarVehicles[liB]);

        // FLAG: the "is the player the slammer of this car" confirmation (asm reads the in-record
        // attacker fields +6944/+6288/+6032 == mePlayerActiveRaceCarIndex) is delegated to
        // ShouldRaceCarCrashOnCarImpact -- the standalone attacker-field reads are unmodelled.
        bool lbAny = false;

        // Victim candidate A (asm: ShouldRaceCarCrashOnCarImpact(this, v8, vehA, normal-rodata)).
        if (ShouldRaceCarCrashOnCarImpact(liA, lpVehA, lpVehB))
        {
            lpInfo->mbCrashRaceCarA = true; // asm *(a2+256)=1
            // aggressor = the player slot; victim = A. The asm re-encodes the player index and a2[7].
            const EntityId lAggressor = MakeRaceCarEntityId(static_cast<u32>(mePlayerActiveRaceCarIndex));
            const EntityId lVictim    = MakeRaceCarEntityId(static_cast<u32>(lpInfo->meActiveRaceCarIndexA));
            InstantTakedown(lVictim, lAggressor,
                            lpInfo->mpContact->mNormal,
                            lpInfo->mpContact->mPointOnA,
                            lpInfo->mfNormalStressSq,
                            lpInfo->mpRequestOutputInterface,
                            lpInfo->mpManagerOutputInterface,
                            lpInfo->mpVehicleOutputInterface,
                            lpInfo->mpDeformationInterface,
                            BrnGameState::E_TAKEDOWN_STANDARD);
            lbAny = true;
        }

        // Victim candidate B (asm: ShouldRaceCarCrashOnCarImpact(this, a2[8], vehB, vehA-negnormal)).
        if (ShouldRaceCarCrashOnCarImpact(liB, lpVehB, lpVehA))
        {
            lpInfo->mbCrashRaceCarB = true; // asm *(a2+257)=1
            const EntityId lAggressor = MakeRaceCarEntityId(static_cast<u32>(mePlayerActiveRaceCarIndex));
            const EntityId lVictim    = MakeRaceCarEntityId(static_cast<u32>(lpInfo->meActiveRaceCarIndexB));
            InstantTakedown(lVictim, lAggressor,
                            lpInfo->mpContact->mNormal,
                            lpInfo->mpContact->mPointOnA,
                            lpInfo->mfNormalStressSq,
                            lpInfo->mpRequestOutputInterface,
                            lpInfo->mpManagerOutputInterface,
                            lpInfo->mpVehicleOutputInterface,
                            lpInfo->mpDeformationInterface,
                            BrnGameState::E_TAKEDOWN_STANDARD);
            lbAny = true;
        }

        return lbAny;
    }

    // -------------------------------------------------------------------------------------------
    // CheckForHittingAlreadyCrashingCar  @0x8263DAC0  ->  STANDARD (pile-on / finish-off)
    //
    // One car is already crashing and the other rams it. Runs BEFORE the not-crashing gate. Includes
    // a player-revenge sub-gate (mePlayerActiveRaceCarIndex vs the record's current-attacker field).
    // Calls ShouldRaceCarCrashOnCarImpact for the still-live car; commits if it passes and the two
    // cars' crash-states are both 1.
    //
    // FLAG: the asm reads RaceCarPhysics in-record velocity lanes (5216*idx + 5680, the +4432
    // current-attacker field, and the +4176 height lane) plus the maRaceCarCrashState array via
    // 4*(idx+11048)+this. The crash-state reads ARE modelled (named array); the in-record velocity /
    // attacker / height reads are unmodelled RaceCarPhysics fields, so the "which car is crashing"
    // determination is taken from the response-info crash flags and the player-revenge sub-gate is
    // delegated to ShouldRaceCarCrashOnCarImpact. Those omitted in-record reads are documented.
    // -------------------------------------------------------------------------------------------
    bool VehicleManager::CheckForHittingAlreadyCrashingCar(RaceCarResponseInfo* lpInfo)
    {
        const s32 liA = static_cast<s32>(lpInfo->meActiveRaceCarIndexA); // asm v4 = v0[7]
        const s32 liB = static_cast<s32>(lpInfo->meActiveRaceCarIndexB); // asm v6 = v0[8]

        // The asm OR-folds an in-record velocity-magnitude test with the response-info crash flag to
        // decide each car is "crashing-and-fast-enough". FLAG: the velocity lane (+5680) is
        // unmodelled; we use the response-info crash flags directly.
        const bool lbACrashing = lpInfo->mbRaceCarAIsCrashing; // asm (v19 & v5)
        const bool lbBCrashing = lpInfo->mbRaceCarBIsCrashing; // asm (v7 & v24)
        if (!lbACrashing && !lbBCrashing)
            return false;

        RaceCarPhysics* const lpVehA = reinterpret_cast<RaceCarPhysics*>(&maRaceCarVehicles[liA]);
        RaceCarPhysics* const lpVehB = reinterpret_cast<RaceCarPhysics*>(&maRaceCarVehicles[liB]);

        if (lbACrashing)
        {
            // A is crashing -> B is the live rammer (the victim of the takedown is the crashing A).
            // asm: ShouldRaceCarCrashOnCarImpact(this, a2[8], vehB, vehA).
            if (!ShouldRaceCarCrashOnCarImpact(liB, lpVehB, lpVehA))
                return false;
            if (!lpInfo->mbCrashRaceCarB)
            {
                // Both cars' crash-states must read 1 for the pile-on takedown to register.
                if (maRaceCarCrashState[liA] == 1 && maRaceCarCrashState[liB] == 1)
                {
                    // Player-revenge sub-gate: only fire when the player is the current attacker.
                    // FLAG: the asm reads the in-record current-attacker field (+4432) and the
                    // +4176 height lane vs mePlayerActiveRaceCarIndex; those in-record reads are
                    // unmodelled, so the revenge confirmation is delegated to the crash-state gate.
                    const u32 luPlayer = static_cast<u32>(mePlayerActiveRaceCarIndex);
                    const EntityId lVictim    = MakeRaceCarEntityId(static_cast<u32>(liA));
                    const EntityId lAggressor = MakeRaceCarEntityId(luPlayer);
                    InstantTakedown(lVictim, lAggressor,
                                    lpInfo->mpContact->mNormal,
                                    lpInfo->mpContact->mPointOnA,
                                    lpInfo->mfNormalStressSq,
                                    lpInfo->mpRequestOutputInterface,
                                    lpInfo->mpManagerOutputInterface,
                                    lpInfo->mpVehicleOutputInterface,
                                    lpInfo->mpDeformationInterface,
                                    BrnGameState::E_TAKEDOWN_STANDARD);
                }
                lpInfo->mbCrashRaceCarA = true; // asm *(_R31+256)=1
            }
            return true;
        }

        // B is crashing -> A is the live rammer. asm: ShouldRaceCarCrashOnCarImpact(this, a2[7], vehA, vehB).
        if (!ShouldRaceCarCrashOnCarImpact(liA, lpVehA, lpVehB))
            return false;
        if (!lpInfo->mbCrashRaceCarB)
        {
            if (maRaceCarCrashState[liA] == 1 && maRaceCarCrashState[liB] == 1)
            {
                // FLAG: player-revenge in-record reads (+4432 / +4176) omitted as above.
                const u32 luPlayer = static_cast<u32>(mePlayerActiveRaceCarIndex);
                const EntityId lVictim    = MakeRaceCarEntityId(static_cast<u32>(liB));
                const EntityId lAggressor = MakeRaceCarEntityId(luPlayer);
                InstantTakedown(lVictim, lAggressor,
                                lpInfo->mpContact->mNormal,
                                lpInfo->mpContact->mPointOnA,
                                lpInfo->mfNormalStressSq,
                                lpInfo->mpRequestOutputInterface,
                                lpInfo->mpManagerOutputInterface,
                                lpInfo->mpVehicleOutputInterface,
                                lpInfo->mpDeformationInterface,
                                BrnGameState::E_TAKEDOWN_STANDARD);
            }
            lpInfo->mbCrashRaceCarB = true; // asm *(_R31+257)=1
        }
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // Layout pins for the deep members InstantTakedown reaches. Never called; exists only to host
    // the offsetof asserts (offsetof on a private member must be evaluated in member-function scope).
    // Each offset here is asm-proven; if a padding run drifts, the gate fails -- that is intended.
    // -------------------------------------------------------------------------------------------
    void VehicleManager::_AssertLayout()
    {
        static_assert(sizeof(RaceCarStatusRecord)  == 224,  "RaceCarStatusRecord stride (asm: 224)");
        static_assert(offsetof(RaceCarStatusRecord, mbBoostImpactEligible) == 123, "boost-eligible byte (asm: +123)");
        static_assert(offsetof(RaceCarStatusRecord, mbTakenDown) == 124, "taken-down byte (asm: +124)");
        static_assert(sizeof(RaceCarVehicleRecord) == 5216, "RaceCarVehicleRecord stride (asm: 5216)");
        static_assert(offsetof(RaceCarVehicleRecord, mfRecoveryTimer) == 5120, "recovery timer (asm: +5120)");
        static_assert(sizeof(RaceCarCrashData)     == 12,   "RaceCarCrashData stride (asm: 12)");

        static_assert(offsetof(VehicleManager, maRaceCarStatus)          == 0,      "maRaceCarStatus (asm base 0)");
        static_assert(offsetof(VehicleManager, maRaceCarVehicles)        == 1856,   "maRaceCarVehicles (asm base 1856)");
        static_assert(offsetof(VehicleManager, maRaceCarCrashData)       == 43808,  "maRaceCarCrashData (asm base 43808)");
        static_assert(offsetof(VehicleManager, maRaceCarCrashState)      == 44192,  "maRaceCarCrashState (asm base 44192)");
        static_assert(offsetof(VehicleManager, mUsedRaceCars)            == 44224,  "mUsedRaceCars (asm +44224)");
        static_assert(offsetof(VehicleManager, mRaceCarCrashDataAllocBits) == 44232, "mRaceCarCrashDataAllocBits (asm +44232)");
        static_assert(offsetof(VehicleManager, mbTakedownsEnabled)       == 171464, "mbTakedownsEnabled (asm +171464)");
        static_assert(offsetof(VehicleManager, mbSlamShuntPhysicsEnabled) == 171465, "mbSlamShuntPhysicsEnabled (asm +171465)");
        static_assert(offsetof(VehicleManager, miAttackerToRecord)       == 171540, "miAttackerToRecord (asm +171540)");
        static_assert(offsetof(VehicleManager, mfTBoneAngleBandDegrees)   == 171564, "mfTBoneAngleBandDegrees (asm +171564)");
        static_assert(offsetof(VehicleManager, mfTBoneSidePlaneHalfWidth) == 171568, "mfTBoneSidePlaneHalfWidth (asm +171568)");
        static_assert(offsetof(VehicleManager, mfNudgeMaxClosingSpeed)    == 171580, "mfNudgeMaxClosingSpeed (asm +171580)");
        static_assert(offsetof(VehicleManager, mfShuntMaxClosingSpeed)    == 171584, "mfShuntMaxClosingSpeed (asm +171584)");
        static_assert(offsetof(VehicleManager, mfTradingPaintMinSpeed)    == 171616, "mfTradingPaintMinSpeed (asm +171616)");
        static_assert(offsetof(VehicleManager, mfTradingPaintMaxSpeed)    == 171620, "mfTradingPaintMaxSpeed (asm +171620)");
        static_assert(offsetof(VehicleManager, mfHeadToHeadAngleToleranceDeg) == 171628, "mfHeadToHeadAngleToleranceDeg (asm +171628)");
        static_assert(offsetof(VehicleManager, mfHeadToHeadMinClosingSpeed)   == 171636, "mfHeadToHeadMinClosingSpeed (asm +171636)");
        static_assert(offsetof(VehicleManager, maRaceCarLastImpactMagnitude) == 171644, "maRaceCarLastImpactMagnitude (asm base 171644)");
        static_assert(offsetof(VehicleManager, maRaceCarTakenDownThisFrame)  == 171676, "maRaceCarTakenDownThisFrame (asm base 171676)");
        static_assert(offsetof(VehicleManager, maRaceCarLastAttacker)    == 171684, "maRaceCarLastAttacker (asm base 171684)");
        static_assert(offsetof(VehicleManager, mePlayerActiveRaceCarIndex) == 172204, "mePlayerActiveRaceCarIndex (DWARF/asm +172204)");
        static_assert(offsetof(VehicleManager, mbSuppressPlayerCrash)    == 172306, "mbSuppressPlayerCrash (asm +172306)");
        static_assert(offsetof(VehicleManager, mbSuppressIfAlreadyCrashState1) == 172307, "mbSuppressIfAlreadyCrashState1 (asm +172307)");
        static_assert(offsetof(VehicleManager, mbHornTakedownEnabled)    == 172311, "mbHornTakedownEnabled (asm +172311)");
        static_assert(offsetof(VehicleManager, mbStationaryTakedownsEnabled) == 172315, "mbStationaryTakedownsEnabled (asm +172315)");
        static_assert(offsetof(VehicleManager, muTakedownEventsThisFrame) == 172612, "muTakedownEventsThisFrame (asm +172612)");
    }
}
}
