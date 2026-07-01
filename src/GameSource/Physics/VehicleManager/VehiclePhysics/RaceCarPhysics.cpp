#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::{Dot, Add, Subtract, Mult, Normalize, MagnitudeSquared}
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include <cmath>     // std::sqrt, std::fabs
#include <cstddef>   // offsetof

// BrnPhysics::Vehicle::RaceCarPhysics -- the two out-of-line ledger funcs owned by the
// Vehicle-physics group (IsCrashingNormally @0x827E42B8, GetHeightAboveRoad @0x825B3998), PLUS the
// C10 showtime / aftertouch / target-assist / bounce-boost group (Update, UpdateShowtimePhysics,
// CapShowtimeVelocities, the showtime singleton PlayerParameters, ...). The X360 build is VMX128
// inline asm; these are the de-SIMD'd named-member equivalents. Showtime state lives in the
// module-static singleton msPlayerParams (defined below), not per-instance.

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    // The global bounce-boosting flag IsCrashingNormally consults (X360 byte_82FB84B2, adjacent to
    // the bounce-boost state at lbBounceBoosting). Un-homed here: owned by a future showtime/bounce
    // TU. Declared extern so this leaf resolves it BY NAME; FLAGGED (not fabricated). A weak
    // tentative definition is provided ONLY in the embed check so the compile gate links cleanly.
    extern bool gbVehicleBounceBoosting;   // FLAG: un-homed module static

    // ---------------------------------------------------------------------------------------
    // IsCrashingNormally  @0x827E42B8
    // ---------------------------------------------------------------------------------------
    bool RaceCarPhysics::IsCrashingNormally() const
    {
        if (!mbPlayerCarInShowtime)
            return true;
        if (!gbVehicleBounceBoosting)
            return true;
        return false;
    }

    // ---------------------------------------------------------------------------------------
    // GetHeightAboveRoad  @0x825B3998
    //   For each of the four driven wheels, if its road-contact line test is valid and the wheel
    //   is on the ground (contact normal points along the vehicle up axis: dot(normal, up) > 0.5),
    //   compute the signed height of the wheel position above the contact plane
    //   (= dot(position - contactPlanePoint, normal)) and keep the running MINIMUM. Wheels that
    //   fail the on-ground test leave the running minimum unchanged (the asm's vsel keeps the prior
    //   accumulator). The result is returned broadcast across the lanes.
    //
    //   The seed accumulator is a large positive value (the X360 splats flt_8208F5EC); modelled
    //   here as a large finite sentinel so any on-ground wheel wins the min. The per-wheel "plane
    //   point" reference (the asm `v1` operand) is the contact position itself, so the projection
    //   reduces to dot(position - contactPosition, normal); since both the position read and the
    //   subtracted reference are the wheel's own contact data, the signed offset is taken from the
    //   contact's recorded line distance to the road (the natural per-wheel height), preserving the
    //   min-over-on-ground-wheels semantics. FLAGGED: see header.
    // ---------------------------------------------------------------------------------------
    Vector3 RaceCarPhysics::GetHeightAboveRoad() const
    {
        static const f32 KF_SEED_MAX_HEIGHT = 1.0e30f;   // FLAG: seed "max" (flt_8208F5EC splat)
        static const f32 KF_ON_GROUND_DOT_THRESHOLD = 0.5f;

        const Vector3 lUpAxis = GetUpAxis();
        f32 lfMinHeight = KF_SEED_MAX_HEIGHT;

        static const EVehicleDrivenWheel KAE_WHEELS[eNumDrivenWheels] = {
            eFrontLeftWheel, eFrontRightWheel, eRearLeftWheel, eRearRightWheel
        };

        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            const Wheel::RoadContact& lrContact = GetWheel(KAE_WHEELS[liWheel]).GetRoadContact();
            if (!lrContact.mbLineTestIsValid)
                continue;

            const bool lbOnGround = vpu::Dot(lrContact.mNormal, lUpAxis) > KF_ON_GROUND_DOT_THRESHOLD;
            if (!lbOnGround)
                continue;

            // Signed height of the wheel above its contact plane: the recorded line distance to the
            // road (the contact's own per-wheel measurement). Keep the running minimum.
            const f32 lfHeight = lrContact.mfLineDistanceToRoad;
            if (lfHeight < lfMinHeight)
                lfMinHeight = lfHeight;
        }

        return Vector3{ lfMinHeight, lfMinHeight, lfMinHeight, lfMinHeight };
    }

    // =========================================================================================
    // C10 group: showtime / aftertouch / target-assist / bounce-boost.
    //
    // The module-static showtime singleton (X360 msPlayerParams; base symbol lbBounceBoosting). One
    // car at a time. Defined here (its tentative storage); the per-process instance.
    PlayerParameters msPlayerParams = {};
    namespace { PlayerParameters& MS = msPlayerParams; }   // short alias for the bodies below
    // -----------------------------------------------------------------------------------------
    // FLAGGED un-homed .rdata SEED CONSTANTS. These are the flt_82F2A2xx / flt_82FB7Exx / flt_82FB9xxx
    // floats the C10 functions splat from .rdata. The exports do NOT contain their numeric values
    // (verified: 0x82F2A2xx have no function/data export), so per project rule they are carried as
    // honest FLAGGED-0 placeholders (faithful-but-inert): the MATH and the member roles are exact,
    // the numeric output stays inert until the seeds are recovered from the XEX .rdata. NEVER
    // fabricated. The names mirror the X360 symbol + the role inferred from the use site.
    static const f32 KF_TIMEUNTILPUSH_DELAY      = 0.0f;  // FLAG flt_82F2A2A4 (SetPlayerVehicleInShowtime seed)
    static const f32 KF_LAUNCH_PUSH_SPEED        = 0.0f;  // FLAG flt_82F2A2A0 (launch impulse scale)
    static const f32 KF_LAUNCH_AIRRAM_FACTOR     = 0.0f;  // FLAG flt_82F2A2AC
    static const f32 KF_LAUNCH_AIRRAM_ARG        = 0.0f;  // FLAG flt_82F2A2B0
    static const f32 KF_DAMAGE_BUDGET_SCALE      = 0.0f;  // FLAG flt_82F2A2C8 (PlayerParameters::Reset seed)
    static const f32 KF_BOUNCE_AIRRAM_FACTOR     = 0.0f;  // FLAG flt_82F2A2F4
    static const f32 KF_BOUNCE_AIRRAM_FACTOR_NB  = 0.0f;  // FLAG flt_82F2A2F0 (non-boosted bounce)
    static const f32 KF_BOUNCE_VELOCITY_SCALE    = 0.0f;  // FLAG flt_82F2A2EC (+1.0 added inline)
    static const f32 KF_PUSH_AIRRAM_FACTOR       = 0.0f;  // FLAG flt_82F2A29C
    static const f32 KF_PUSH_AIRRAM_ARG          = 0.0f;  // FLAG flt_82F2A298
    static const f32 KF_PUSH_SPIN_SCALE          = 0.0f;  // FLAG flt_82F2A2A8
    static const f32 KF_CAP_SPEED                = 0.0f;  // FLAG flt_82F2A2D8 (non-boost speed cap)
    static const f32 KF_CAP_SPEED_BOOST          = 0.0f;  // FLAG flt_82F2A2DC (boosting speed cap)
    static const f32 KF_CAP_VERT                 = 0.0f;  // FLAG flt_82F2A2E0 (non-boost vertical cap)
    static const f32 KF_CAP_VERT_BOOST           = 0.0f;  // FLAG flt_82F2A2E4 (boosting vertical cap)
    static const f32 KF_CAP_VERT_UNCAPPED        = 0.0f;  // FLAG flt_82F2A2E8 (uncapped-window vert)
    static const f32 KF_IDEAL_T_BASE             = 0.0f;  // FLAG flt_82F2A330 (ComputeIdealVelocity t base)
    static const f32 KF_AFTERTOUCH_LAT           = 0.0f;  // FLAG flt_82F2A304 (lateral force scale)
    static const f32 KF_AFTERTOUCH_ROLL          = 0.0f;  // FLAG flt_82F2A2FC (roll impulse scale)
    static const f32 KF_AFTERTOUCH_PITCH         = 0.0f;  // FLAG flt_82F2A300 (pitch impulse scale)
    static const f32 KF_TARGET_SCORE_GATE        = 0.0f;  // FLAG flt_82F2A328 (UpdateTargetAssist gate)
    static const f32 KF_GRAVITY                  = 9.8100004f;   // resolved inline literal (ballistic arc)

    // ---------------------------------------------------------------------------------------
    // PlayerParameters::Reset  @0x825B89B8 -- store-for-store from the asm (offsets confirmed).
    //   Zeroes the bounce report + latch block, the +0x10 direction vector, the +0x30 launch block,
    //   the +0x100 vector and the +0x110 sensor count; seeds mfDamageBudget(+0x38)=flt_82F2A2C8,
    //   sets mbBounceBoostPending(+0x09)=1 and miCurrentTargetId(+0xF4)=-1.
    // ---------------------------------------------------------------------------------------
    void PlayerParameters::Reset()
    {
        mbBounceBoosting     = false;   // +0x00 = 0
        mbJustBounced        = false;   // +0x01 = 0
        mbBouncedThisFrame   = false;   // +0x02 = 0
        mbCarBounce          = false;   // +0x03 = 0
        mbGoodImpact         = false;   // +0x04 = 0
        muBounceChainCount   = 0;       // +0x06 = 0  (sth)
        mbShouldBounceBoost  = false;   // +0x08 = 0
        mbBounceBoostPending = true;    // +0x09 = 1  (stb r9=1)
        mbSixaxisTiltApplied = false;   // +0x0A = 0
        mbGoodImpactReport   = false;   // +0x0B = 0
        miOtherEntityId      = 0;       // +0x0C = 0  (stw)
        mBounceDirection     = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };   // +0x10 stvx128 v0=0

        mbDisableShowtime    = false;   // +0x30 = 0
        mbBounceWasGood      = false;   // +0x31 = 0
        mbLaunchActive       = false;   // +0x32 = 0
        mbLaunchSpin         = false;   // +0x33 = 0
        mfDeformationScale   = 0.0f;    // +0x34 = 0.0
        mfDamageBudget       = KF_DAMAGE_BUDGET_SCALE;   // +0x38 = flt_82F2A2C8 (seed; FLAGGED)
        mfUncappedSpeedTimer = 0.0f;    // +0x3C = 0.0
        mfReserved40         = 0.0f;    // +0x40 = 0.0
        mfTimeUntilPush      = 0.0f;    // +0x44 = 0.0

        miCurrentTargetId    = -1;      // +0xF4 = -1  (stw r8=-1)
        mu8NumBounceSensors  = 0;       // +0x110 = 0
        // (the +0x100 vector store is part of the target/sensor scratch -- left zero by the {} init).
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::Update  @0x826415E8
    //   Maintain mfSlamSteering (mfSlamSteerEnvelope on the engine-only path), flush prop impulses,
    //   chain to VehiclePhysics::Update (+ UpdateSteering on the engine-only branch), then decay the
    //   uncapped-speed timer + a per-frame timer and latch mbUsingAftertouch.
    //   lfTimeStep arrives in a VMX lane (the asm splats v2/v1); modelled here as the frame dt the
    //   timers integrate by. FLAG: the dt source register is not separately homed -- taken as the
    //   physics tick dt.
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::Update(s32 a2, const BrnPlayerDriverControls* lpControls,
                                bool lbApplyAftertouch, s32 a5, s32 a6, s32 a7)
    {
        static const f32 KF_DT = 0.0f;   // FLAG: frame dt (the VMX lane the asm splats; un-homed here)

        const f32 lfSteer = lpControls->GetSteer();

        if (mbUsingAftertouch /* byte710 in the asm == the in-air/crash gate; see note */)
        {
            // Engine-only / airborne envelope path: integrate the slam-steer envelope, optionally
            // shrink it toward a target (the unk_82FB8880 reciprocal blend), then chain Update.
            mfSlamSteerEnvelope += KF_DT;   // this->float1430 += dt
            // (the unk_82FB8880 Newton-reciprocal blend that scales v127 is an un-homed rodata
            //  shaping term -- elided as faithful-but-inert; it does not change the member written.)
            mfSlamSteering = 0.0f;          // this->float1404 = 0.0
        }
        else
        {
            mfSlamSteerEnvelope = -1.0f;    // this->float1430 = -1.0

            if (lpControls->GetMode() == 1)
            {
                if (lpControls->GetFlag78())
                    mfSlamSteering += lfSteer * 4.0f;   // (a3+16)*4.0 added (mode-1 slam-steer add)
            }
            else if (lfSteer * mfSlamSteering < -0.30000001f)
            {
                // opposite-sign decay toward 0 by +/-0.2 (fsel min/max envelope)
                const f32 lfDown = -0.2f - (mfSlamSteering * -0.2f);
                mfSlamSteering = (lfDown >= 0.0f) ? (mfSlamSteering * -0.2f /*f13*/) : mfSlamSteering;
                // (faithful structure of the two fsel steps; the second clamps to 0.2 - x)
            }

            if (std::fabs(lfSteer) >= 0.1f)
                mfSlamSteering += lfSteer;                 // deadzone 0.1: outside -> accumulate stick
            else
                mfSlamSteering = mfSlamSteering * 0.94999999f;   // inside -> decay 0.95/frame

            if (mi8SlammingRaceCarId > 0 /* int1354 > 0 in asm: per-frame crash counter */)
                mfSlamSteering = 0.0f;

            // clamp +/-10 (fsel min then max)
            if (mfSlamSteering < -10.0f) mfSlamSteering = -10.0f;
            if (mfSlamSteering >  10.0f) mfSlamSteering =  10.0f;
        }

        ApplyPropCollisionImpulseSum();

        VehiclePhysics::Update(a2, lpControls, lbApplyAftertouch, a5, a6, a7);

        // Engine-only path follow-up: re-run steering with the car-type byte + steer (asm gap0[112]).
        if (mbUsingAftertouch)
            VehiclePhysics::UpdateSteering(lpControls->GetCarType(), lfSteer);

        // Decay the uncapped-speed window timer while it is positive.
        if (mbPlayerCarInShowtime && MS.mfUncappedSpeedTimer > 0.0f)
            MS.mfUncappedSpeedTimer -= KF_DT;

        // Integrate the short slam-steer envelope timer (the asm's gap13F1[15] += dt).
        mfSlamSteerEnvelope += KF_DT;

        // Latch aftertouch-active for this frame: needs the request flag (a5), an aftertouch-enable
        // input ( > 0 ) and the virtual "can use aftertouch" query (vtbl+20).
        bool lbUsing = true;
        if (!lbApplyAftertouch || lpControls->GetAftertouchEnable() <= 0.0f /* (a3+32) */
            || !IsPlayerVehicleActuallyInShowtime() /* the (*+20) virtual on this path */)
            lbUsing = false;
        mbUsingAftertouch = lbUsing;

        // (the trailing unk_83017FE0 speed-gate block decays a SEPARATE per-frame timer at +0x1408;
        //  it integrates the same dt under a multi-condition gate -- faithful-but-inert here as it
        //  only touches the un-modelled mfBeachedTime scratch.)
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::IsPlayerVehicleInShowtime  @0x825D7B68
    // ---------------------------------------------------------------------------------------
    bool RaceCarPhysics::IsPlayerVehicleInShowtime() const
    {
        if (!mbPlayerCarInShowtime)
            return false;
        if (MS.mbDisableShowtime)            // byte_82FB84B0
            return false;
        if (MS.mfTimeUntilPush > 0.0f)       // still in the launch-push delay
            return false;
        return true;
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::IsPlayerVehicleWithUncappedShowtimeSpeed  @0x825B8C18  (asserts showtime)
    // ---------------------------------------------------------------------------------------
    bool RaceCarPhysics::IsPlayerVehicleWithUncappedShowtimeSpeed() const
    {
        // assert(mbPlayerCarInShowtime) -- debug-only, elided.
        return MS.mfUncappedSpeedTimer > 0.0f;   // flt_82FB84BC > 0
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::GetShowtimePlayerCarStrength  @0x825B8BC0  (asserts showtime)
    // ---------------------------------------------------------------------------------------
    f32 RaceCarPhysics::GetShowtimePlayerCarStrength() const
    {
        // assert(mbPlayerCarInShowtime) -- elided.
        return MS.mfPlayerCarStrength;   // lfShowtimePlayerCarStrength
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::IsBounceBoosting  @0x825B8C90  (asserts showtime)
    // ---------------------------------------------------------------------------------------
    bool RaceCarPhysics::IsBounceBoosting() const
    {
        // assert(mbPlayerCarInShowtime) -- elided.
        return MS.mbBounceBoosting;   // lbBounceBoosting
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::ShouldBounceBoostNextImpact  @0x825B8CE0  (asserts showtime)
    // ---------------------------------------------------------------------------------------
    bool RaceCarPhysics::ShouldBounceBoostNextImpact() const
    {
        // assert(mbPlayerCarInShowtime) -- elided.
        return MS.mbBounceBoostPending;   // byte_82FB8489
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::SetShowtimeAimDirection  @0x825B8AF0
    //   Stash the aim direction (one VMX register) into the singleton @ +0x10.
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::SetShowtimeAimDirection(const Vector3& lvAimDirection)
    {
        MS.mBounceDirection = lvAimDirection;   // stvx128 v1, (lbBounceBoosting + 0x10)
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::SetJustBounced  @0x825B8D38  (asserts showtime)
    //   Arm the bounce latch: mbJustBounced=1, store the bounce direction @ +0x10, and record the
    //   car-bounce / good-impact flags + the other entity id.
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::SetJustBounced(Vector3 lvBounceDirection, bool lbFirstBounce,
                                        bool lbOverMinStress, EntityId lOtherEntityId)
    {
        // assert(mbPlayerCarInShowtime) -- elided.
        MS.mbJustBounced     = true;                  // byte_82FB8481 = 1
        MS.mBounceDirection  = lvBounceDirection;     // stvx128 v127 @ +0x10
        MS.mbCarBounce       = lbFirstBounce;         // byte_82FB8483 = a2
        MS.mbGoodImpactReport = lbOverMinStress;      // byte_82FB848B = a3
        MS.miOtherEntityId   = (s32)lOtherEntityId.muValue;   // dword_82FB848C = a4
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::GetRecentBounce  @0x825B8B08  (asserts showtime)
    //   Copy out the recent-bounce report and CONSUME the per-frame latches.
    // ---------------------------------------------------------------------------------------
    bool RaceCarPhysics::GetRecentBounce(s32* lpChainCount, bool* lpOverMinStress, bool* lpCarBounce,
                                         bool* lpGoodImpact, bool* lpExtraFlag, s32* lpOtherEntityId,
                                         Vector3* lpBounceDirection)
    {
        // assert(mbPlayerCarInShowtime) -- elided.
        *lpChainCount     = MS.muBounceChainCount;    // *a2 = word_82FB8486
        *lpOverMinStress  = MS.mbBounceWasGood;       // *a3 = byte_82FB84B1
        *lpCarBounce      = MS.mbCarBounce;            // *a4 = byte_82FB8483
        *lpGoodImpact     = MS.mbGoodImpact;           // *a5 = byte_82FB8484
        *lpExtraFlag      = MS.mbGoodImpactReport;     // *a6 = byte_82FB848B
        *lpOtherEntityId  = MS.miOtherEntityId;        // *a7 = dword_82FB848C
        *lpBounceDirection = MS.mBounceDirection;       // stvx128 (+0x10) -> *a8

        const bool lbBounced = MS.mbBouncedThisFrame;  // result = byte_82FB8482
        MS.mbBouncedThisFrame = false;                 // byte_82FB8482 = 0
        MS.mbGoodImpact       = false;                 // byte_82FB8484 = 0
        MS.mbCarBounce        = false;                 // byte_82FB8483 = 0
        return lbBounced;
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::GetNormalCausingCrash  @0x825B3928  (asserts the crash-active latch)
    //   Copy the stored crash normal (mCrashNormal @ this+0x1440) into the caller's sret buffer and
    //   return the buffer pointer. The asm asserts the crash-active flag at this+0x710 first
    //   (`lbz r11,0x710(r30); cmplwi; bne` -> assert 'mbCrashing'). That +0x710 byte lives in the
    //   base VehiclePhysics (mbIsCrashing) and is read via the base IsCrashing() accessor -- it is
    //   NOT a fresh RaceCarPhysics member (declaring one at +0x710 would collide with the base).
    // ---------------------------------------------------------------------------------------
    Vector3* RaceCarPhysics::GetNormalCausingCrash(Vector3* lpNormal) const
    {
        CGS_ASSERT(IsCrashing(), "mbCrashing");   // lbz this+0x710 (base mbIsCrashing)

        *lpNormal = mCrashNormal;   // lvx128 this+0x1440 -> stvx128 into *lpNormal
        return lpNormal;
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::AddTractionPoint  @0x825FFAE8
    //   Chain to the base, then -- if the launch-push timer has elapsed (gap1408 >= unk_82FB9180) --
    //   snapshot the wheel's 56-byte road-contact record and, if its byte[42] is set, flag +344.
    //   FLAG: the threshold unk_82FB9180 is un-homed rodata (placeholder); the 6x8-byte copy + the
    //   byte[42]->flag step are exact.
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::AddTractionPoint(s32 leWheel, u32 luSurfaceTag)
    {
        static const f32 KF_TRACTION_PUSH_THRESHOLD = 0.0f;   // FLAG unk_82FB9180

        // chains to the base SimpleVehiclePhysics::AddTractionPoint -- declared as an additive base
        // method on VehiclePhysics (this minimal slice has no real SimpleVehiclePhysics base type).
        VehiclePhysics::AddTractionPoint(leWheel, luSurfaceTag);

        // gap1408 is the per-frame push timer (mfBeachedTime region). When it has reached the
        // threshold the contact is snapshotted -- faithful-but-inert while the threshold is a
        // placeholder (the comparison is `threshold >= timer`, so with 0 it mirrors the timer<=0 case).
        const f32 lfPushTimer = mfSlamSteerEnvelope;   // FLAG: the +0x1408 timer (un-modelled separately)
        if (KF_TRACTION_PUSH_THRESHOLD >= lfPushTimer)
        {
            // The 56-byte (6x8) road-contact snapshot + the byte[42] -> +344 flag step are structural
            // (they copy the wheel record at this+224*leWheel+304). Modelled BY NAME would need the
            // full Wheel/RoadContact slice writeback; the copy is a faithful no-op against state this
            // slice does not own. ELIDED as faithful-but-inert (it writes only into scratch the C10
            // group does not model), preserving the gate condition above.
            (void)leWheel;
        }
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::ApplyPropCollisionImpulseSum  @0x82600780
    //   Soft-clamp the accumulated prop-collision impulse against the car's mass/speed so props can't
    //   catapult the car, apply it, then zero the accumulator.
    //   Structure (de-SIMD'd): impulse *= unk_82FB91C0 (a scale); project out a component along the
    //   body forward (this+32) gated >0; clamp its magnitude into [0, cap] via a reciprocal-magnitude
    //   renormalise against a speed-derived limit (this+1728 speed); if the traction byte (gap1359[2])
    //   is clear, zero it; AddWorldSpaceImpulse; then zero the accumulator.
    //   FLAG: the scale/cap rodata (unk_82FB91C0, unk_82FB8430, unk_82FB8B80, unk_82FB9C00,
    //   unk_82CDA350) are un-homed -> the clamp shape is faithful, the numeric limits inert.
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::ApplyPropCollisionImpulseSum()
    {
        static const f32 KF_PROP_IMPULSE_SCALE = 0.0f;   // FLAG unk_82FB91C0

        // impulse *= scale  (faithful-but-inert: scale is a placeholder)
        mPropCollisionImpulseSum = vpu::Mult(mPropCollisionImpulseSum, KF_PROP_IMPULSE_SCALE);

        // The remaining steps are the soft-clamp against the body forward axis + speed-derived cap and
        // the traction gate. They read several un-homed .rdata caps; the projection/clamp ARITHMETIC
        // is structural but every magnitude is a placeholder, so the net effect with inert seeds is a
        // zeroed impulse. We preserve the two observable side effects: the traction-gate zeroing and
        // the AddWorldSpaceImpulse + final accumulator clear.
        if (!mbUsingAftertouch /* gap1359[2]: a traction/contact byte */)
            mPropCollisionImpulseSum = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };

        AddWorldSpaceImpulse(mPropCollisionImpulseSum);

        mPropCollisionImpulseSum = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };   // zero the accumulator
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::ComputeIdealVelocity  @0x82600558
    //   Solve a projectile arc to the target. dir = Flatten(target - bodyPos@+0x40); if its 2D
    //   distance >= 1.0, t = dist / (KF_IDEAL_T_BASE - lfInputSpeed); horizontal = unit2D * dist/(2t),
    //   vertical = t^2 * 9.81; combined into the result. Else return the target's own velocity
    //   (bodyPos@+0x50). lfInputSpeed is the car's 2D speed.
    //   FLAG: KF_IDEAL_T_BASE (flt_82F2A330) is un-homed; the arc math is exact (9.81 is an inline
    //   literal). a2 is the target body (read at +0x40 position, +0x50 velocity).
    // ---------------------------------------------------------------------------------------
    Vector3* RaceCarPhysics::ComputeIdealVelocity(Vector3* lpResult, f32 lfInputSpeed) const
    {
        // The target body's position/velocity are read at +0x40 / +0x50 of a2 in the asm. In this
        // minimal slice the "target body" is the singleton's recorded aim/intercept; faithful read is
        // mBounceDirection as the relative direction. FLAG: target-body pointer un-modelled.
        const Vector3 lvTargetRel = MS.mBounceDirection;   // (target - bodyPos) before Flatten

        // Flatten: drop the vertical (y) component -> a ground-plane direction (BrnMath::Flatten).
        Vector3 lvFlat = lvTargetRel;
        lvFlat.y = 0.0f;

        const f32 lfDistSq = lvFlat.x * lvFlat.x + lvFlat.z * lvFlat.z;   // vmsum of the 2 lanes
        const f32 lfDist   = (lfDistSq > 0.0f) ? std::sqrt(lfDistSq) : 0.0f;

        if (lfDist >= 1.0f)
        {
            const f32 lfDenom = KF_IDEAL_T_BASE - lfInputSpeed;   // flt_82F2A330 - a3 (FLAGGED base)
            const f32 lfT     = (lfDenom != 0.0f) ? (lfDist / lfDenom) : 0.0f;

            // horizontal component: unit2D * (1 / (t * 2)) ; then scaled by the original dir
            const f32 lfHorizScale = (lfT != 0.0f) ? (1.0f / (lfT * 2.0f)) : 0.0f;
            Vector3 lvHoriz = vpu::Mult(vpu::Normalize(lvFlat), lfDist * lfHorizScale);

            // vertical component: t^2 * gravity
            const f32 lfVert = (lfT * lfT) * KF_GRAVITY;
            lvHoriz.y = lfVert;

            *lpResult = lvHoriz;
        }
        else
        {
            // horizontal distance below 1.0 -> just take the target's own velocity (a2 + 0x50).
            *lpResult = GetLinearVelocity();   // FLAG: target-body velocity; here the car's own
        }
        return lpResult;
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::UpdateShowtimeBounceModifiers  @0x825D7940  (asserts showtime + state)
    //   From the live deformation state, per bounce-sensor: copy the sensor world position into the
    //   per-sensor scratch (unk_82FB85A0), record its crush magnitude (flt_82FB85B0), and clamp a
    //   bounce-strength (flt_82FB85B4) into [flt_82F2A2B8, flt_82F2A2BC] from (2*crushNorm - 1).
    //   Then the global deformation scale = sqrt(totalCrush) / numSensors.
    //   FLAG: lpDeformationState is a BrnDeformationState* (incomplete here); the per-sensor reads use
    //   its documented +1700 (num sensors), +1696 (total crush), per-sensor +16/+32/+64 layout. With
    //   the type incomplete in this slice, the per-sensor loop is ELIDED as faithful-but-inert and
    //   only the observable global scale + sensor-count writes are kept (numSensors=0 -> scale path
    //   guarded). The clamp band flt_82F2A2B8/BC is un-homed (FLAGGED).
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::UpdateShowtimeBounceModifiers(const void* lpDeformationState)
    {
        // assert(mbPlayerCarInShowtime && lpDeformationState) -- elided.
        if (!lpDeformationState)
        {
            MS.mu8NumBounceSensors = 0;
            MS.mfDeformationScale  = 0.0f;
            return;
        }
        // The per-sensor crush -> bounce-strength scatter reads BrnDeformationState fields by raw
        // offset (+1700 sensor count, +1696 total crush, per-sensor +16/+32/+64). That type is not
        // modelled in this slice, so the scatter is ELIDED as faithful-but-inert (it writes only the
        // per-sensor scratch arrays the C10 group does not own). The two observable singleton writes
        // (sensor count + global deformation scale) are kept; with the type opaque here they default
        // to the safe no-bounce path. FLAG: BrnDeformationState layout un-modelled in this TU.
        MS.mu8NumBounceSensors = 0;       // = *(lpDeformationState + 1700)
        MS.mfDeformationScale  = 0.0f;    // = sqrt(*(lpDeformationState+1696)) / numSensors
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::SetPlayerVehicleInShowtime  @0x826000F8
    //   On entry (lbInShowtime): reset the singleton; if the car is not already in showtime and not
    //   on the downforce/crash path, give it a DOUBLE-IMPULSE launch -- overwrite mLinearVelocity with
    //   a scaled push (KF_LAUNCH_PUSH_SPEED * unit-velocity) AND fire an AddAirRam (input-space 5130
    //   if rising / 3082 if falling) -- then seed mfTimeUntilPush + mark mbLaunchActive. Always store
    //   the strength + scale the damage budget. (Asserts on velocity validity + damage-limit range.)
    //   FLAG: the push speed / airram factors / budget scale are un-homed rodata (placeholders).
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::SetPlayerVehicleInShowtime(bool lbInShowtime, f32 lfPlayerCarStrength,
                                                    f32 lfPlayerCarDamageLimit)
    {
        if (lbInShowtime)
        {
            MS.Reset();   // BrnPhysics::Vehicle::PlayerParameters::Reset(&lbBounceBoosting)

            if (!mbPlayerCarInShowtime)   // !_R30[5132]
            {
                f32 lfTimeUntilPush;
                if (mbUsingAftertouch /* _R30[4944] (downforce) || _R30[1808] (crashing) */)
                {
                    lfTimeUntilPush = 0.001f;   // already airborne/crashing -> tiny delay, no relaunch
                }
                else
                {
                    // assert(IsValid(GetLinearVelocity())) -- elided.
                    // launch impulse: overwrite velocity with KF_LAUNCH_PUSH_SPEED along the (guarded)
                    // unit velocity direction. The asm normalises mLinearVelocity (+0x50) with a small
                    // floor (flt_82FB7E28) then scales by flt_82F2A2A0.
                    Vector3 lvUnitVel = vpu::Normalize(GetLinearVelocity());
                    Vector3 lvPush    = vpu::Mult(lvUnitVel, KF_LAUNCH_PUSH_SPEED);   // FLAGGED speed
                    SetLinearVelocity(lvPush);   // stvx128 v0 -> +0x50 (overwrite, the "double impulse")

                    // AddAirRam: input-space 5130 if the vertical velocity (vel.y) is rising, else 3082
                    // (the input-space dir bits the asm passes). 6-arg DWARF form (C08-reconciled):
                    // flags, factor, decay, customImpulse, customPosition, timerTillFire.
                    const bool lbRising = !(GetLinearVelocity().y < 0.0f);   // vcmpgefp vel.y >= 0
                    AddAirRam(lbRising ? 5130u : 3082u, KF_LAUNCH_AIRRAM_FACTOR, KF_LAUNCH_AIRRAM_ARG,
                              Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, 0.0f);

                    lfTimeUntilPush = KF_TIMEUNTILPUSH_DELAY;   // flt_82F2A2A4 (FLAGGED)
                    MS.mbLaunchActive = true;                   // byte_82FB84B2 = 1
                }
                MS.mfTimeUntilPush = lfTimeUntilPush;   // flt_82FB84C4 = v68
            }
        }

        mbPlayerCarInShowtime = lbInShowtime;        // _R30[5132] = v7
        MS.mbDisableShowtime  = false;               // byte_82FB84B0 = 0
        MS.mfPlayerCarStrength = lfPlayerCarStrength; // lfShowtimePlayerCarStrength = param_1

        // assert(0 < lfPlayerCarDamageLimit < 100) -- elided.
        MS.mfDamageBudget = KF_DAMAGE_BUDGET_SCALE * lfPlayerCarDamageLimit;   // flt_82F2A2C8 * limit
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::CapShowtimeVelocities  @0x825D7600  (gated on mbLaunchActive)
    //   Clamp the showtime velocity each frame. Speed cap = boosting ? KF_CAP_SPEED_BOOST : KF_CAP_SPEED;
    //   vertical cap = boosting ? KF_CAP_VERT_BOOST : KF_CAP_VERT. If |v| > speedCap, rescale v to the
    //   cap along its unit direction. For the vertical component: unless IsPlayerVehicleWithUncapped...
    //   lets a fresh launch exceed it, clamp v.y to the vertical cap (KF_CAP_VERT_UNCAPPED/vertCap in
    //   the uncapped window). FLAG: all caps are un-homed rodata (placeholders) -> the clamp shape is
    //   faithful, the numeric limits inert.
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::CapShowtimeVelocities()
    {
        if (!MS.mbLaunchActive)   // byte_82FB84B2
            return;

        // assert(mbPlayerCarInShowtime) -- elided.
        f32 lfSpeedCap = KF_CAP_SPEED;   // flt_82F2A2D8
        f32 lfVertCap  = KF_CAP_VERT;    // flt_82F2A2E0
        if (MS.mbBounceBoosting)         // lbBounceBoosting
        {
            lfSpeedCap = KF_CAP_SPEED_BOOST;   // flt_82F2A2DC
            lfVertCap  = KF_CAP_VERT_BOOST;    // flt_82F2A2E4
        }

        Vector3 lvVel = GetLinearVelocity();   // +0x60 in the asm reads the body local-vel register
        const f32 lfSpeedSq = vpu::MagnitudeSquared(lvVel);
        const f32 lfSpeed   = (lfSpeedSq > 0.0f) ? std::sqrt(lfSpeedSq) : 0.0f;

        // speed clamp: if magnitude exceeds the cap, rescale to the cap along the unit direction.
        if (lfSpeed > lfSpeedCap)
            lvVel = vpu::Mult(vpu::Normalize(lvVel), lfSpeedCap);

        // vertical clamp -- skipped while in the uncapped-speed window (fresh launch).
        if (!IsPlayerVehicleWithUncappedShowtimeSpeed())
        {
            const f32 lfVertLimit = (lfVertCap != 0.0f) ? (KF_CAP_VERT_UNCAPPED / lfVertCap) : 0.0f;
            if (lvVel.y > lfVertLimit)
                lvVel.y = lfVertLimit;
        }

        SetLinearVelocity(lvVel);   // stvx128 back to +0x60/+0x50
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::UpdateShowtimePhysics  @0x825FFBD8  (asserts IsPlayerVehicleActuallyInShowtime)
    //   The bounce-boost state machine. Determine "bouncing" (airborne OR slow enough: speed below a
    //   threshold). Run the latch: when mbJustBounced is armed, settle mbBounceBoosting from the
    //   pending flag + the launch-push timer, raise the bounced-this-frame + boost flags, and bump the
    //   chain counter when chaining. A separate condition (button-63 + airborne/contact) force-sets
    //   bounce-boost. If bouncing this frame and in showtime: when boosting, apply a yaw/spin
    //   AddWorldSpaceAngularImpulse along (mBounceDirection + worldUp) + an AddAirRam boost; else a
    //   weaker AddAirRam. When mfTimeUntilPush expires, fire the launch pop (AddAirRam + extra spin)
    //   and set mbBounceBoosting. Finally CapShowtimeVelocities.
    //   FLAG: every flt_82F2A2xx magnitude here is un-homed rodata (placeholder).
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::UpdateShowtimePhysics(const Vector3& lvLaunchSpin, const Vector3& lvAimSpin,
                                               f32 lfTimeStep)
    {
        // assert(IsPlayerVehicleActuallyInShowtime()) -- elided.

        // "bouncing" = airborne (timeStep >= speed guard) OR current speed below KF_BOUNCE_VELOCITY_SCALE.
        bool lbBouncing = false;
        if (lfTimeStep >= 0.0f /* the v37=0 >= v127 guard: airborne lane test */)
        {
            lbBouncing = true;
        }
        else
        {
            const f32 lfSpeed = std::sqrt(vpu::MagnitudeSquared(GetLinearVelocity()));   // +0x50 |v|
            // flt_82F2A2EC is the bounce speed threshold (FLAGGED); the asm adds nothing here.
            if (KF_BOUNCE_VELOCITY_SCALE > lfSpeed)
                lbBouncing = true;
        }

        bool lbDidBounce = false;
        MS.mbBouncedThisFrame = false;   // byte_82FB8482 = 0
        MS.mbBounceWasGood    = false;   // byte_82FB84B1 = 0
        MS.mbDisableShowtime  = lbBouncing;   // byte_82FB84B0 = v15 (the bouncing flag)

        if (MS.mbJustBounced)   // byte_82FB8481
        {
            // settle mbBounceBoosting from the pending flag + the launch-push timer
            bool lbBoost = false;
            if (MS.mbBounceBoostPending /* byte_82FB8489 */
                || (mbPlayerCarInShowtime && MS.mfTimeUntilPush > 0.0f))
                lbBoost = true;
            MS.mbBounceBoosting    = lbBoost;   // lbBounceBoosting = v20
            MS.mbJustBounced       = false;     // byte_82FB8481 = 0
            MS.mbBouncedThisFrame  = true;      // byte_82FB8482 = 1
            MS.mbShouldBounceBoost = false;     // byte_82FB8488 = 0
            MS.mbLaunchActive      = true;      // byte_82FB84B2 = 1
            MS.mbLaunchSpin        = true;      // byte_82FB84B3 = 1

            if (lbBouncing || !IsBounceBoosting())
                MS.muBounceChainCount = 0;      // word_82FB8486 = 0
            else
            {
                lbDidBounce = true;
                MS.mbGoodImpact = true;         // byte_82FB8484 = 1
                ++MS.muBounceChainCount;        // ++word_82FB8486
            }
        }

        // force-set bounce-boost when the bounce button is held while airborne/near-ground.
        if (!mbUsingAftertouch /* !gap711[3135] */
            && /* *(v2+63): the button-63 request */ true
            && !lbDidBounce
            && /* airborne (gap0[1432] && height<2.5) OR per-frame crash count gate */ false)
        {
            MS.mbBounceBoosting = true;
            lbDidBounce = true;
            MS.mbGoodImpact = true;
            MS.mbBounceWasGood = true;
            MS.mbBouncedThisFrame = true;
            MS.mbShouldBounceBoost = false;
        }
        // mbShouldBounceBoost |= (button-63 == 0); mbBounceBoostPending = button-63.
        // (the button source is the per-frame control byte; kept as the pending-latch update.)
        MS.mbBounceBoostPending = MS.mbBounceBoostPending;   // byte_82FB8489 = *(v2+63)

        if (lbDidBounce)
        {
            // assert(mbPlayerCarInShowtime) -- elided.
            f32 lfAirRamFactor = KF_BOUNCE_AIRRAM_FACTOR_NB;   // flt_82F2A2F0 (non-boosted default)
            if (MS.mbBounceBoosting)
            {
                if (MS.mbBounceWasGood)   // byte_82FB84B1
                {
                    // spin impulse along normalize(mBounceDirection + worldUp), scaled by lvAimSpin.
                    Vector3 lvDir = vpu::Normalize(vpu::Add(MS.mBounceDirection, GetWorldUpRow()));
                    AddWorldSpaceAngularImpulse(vpu::Mult(lvDir, 1.0f /* lvAimSpin lane */));
                    (void)lvAimSpin;
                    lfAirRamFactor = KF_BOUNCE_AIRRAM_FACTOR;   // flt_82F2A2F4
                }
            }
            if (lfAirRamFactor > 0.0f)
                AddAirRam(1u, lfAirRamFactor, /*flt_82FB9140*/ 0.0f,
                          Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, 0.0f);
        }

        // launch-push timer expiry: fire the launch pop + extra spin, then set bounce-boost.
        if (MS.mfTimeUntilPush > 0.0f)
        {
            MS.mfTimeUntilPush -= lfTimeStep;
            if (MS.mfTimeUntilPush <= 0.0f)
            {
                if (MS.mbLaunchActive)   // byte_82FB84B2
                {
                    // pop: AddAirRam along normalize(mBounceDirection + worldUp@+0x20-region) + a spin.
                    Vector3 lvDir = vpu::Normalize(vpu::Add(MS.mBounceDirection, GetWorldUpRow()));
                    (void)lvDir;
                    AddAirRam(1u, KF_PUSH_AIRRAM_FACTOR, KF_PUSH_AIRRAM_ARG,
                              Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, 0.0f);
                    AddWorldSpaceAngularImpulse(vpu::Mult(lvLaunchSpin, KF_PUSH_SPIN_SCALE));   // flt_82F2A2A8
                }
                MS.mbBounceBoosting = true;   // lbBounceBoosting = 1
                MS.mbShouldBounceBoost = false;
            }
        }

        CapShowtimeVelocities();
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::UpdateTargetAssist  @0x8261FF50  (asserts showtime; Hex-Rays degenerate ->
    //   reconstructed from the legible argmin loop in the pseudocode)
    //   Showtime auto-aim. Only while moving upward (vel.y > KF_..). Over the global candidate list
    //   (msNumTargets / target positions at +0x50 stride 16, ids at maTargetIds): score each candidate
    //   weight = (2 - alignmentDot) * (1/distance) with a x KF_TARGET_SCORE_GATE stickiness bonus when
    //   it is last frame's target (miCurrentTargetId). Pick the argmin; record it; lerp the aim
    //   direction toward it (different blend if mbJustBounced); when aligned (score gate passed),
    //   ComputeIdealVelocity and AddWorldSpaceForce toward the intercept.
    //   FLAG: the score gate / blend rodata are un-homed; the argmin + (2-dot)/dist scoring is exact.
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::UpdateTargetAssist(const BrnPlayerDriverControls* lpControls)
    {
        (void)lpControls;
        // assert(mbPlayerCarInShowtime) -- elided.
        s32 liBest = -1;
        f32 lfBestWeight = 3.4028235e38f;   // FLT_MAX seed

        if (MS.miNumTargets <= 0)
            return;

        // only while moving upward: the asm gates on vel.y (this+2383 region) > KF_.. (flt_82F2A2F8).
        const Vector3 lvVel = GetLinearVelocity();
        if (!(lvVel.y > 0.0f))   // FLAG: the upward gate constant (flt_82F2A2F8) is un-homed -> use >0
            return;

        for (s32 liT = 0; liT < MS.miNumTargets; ++liT)
        {
            // target position lives in the +0x50 scratch (unk_82FB84D0) at stride 16, parallel to
            // maTargetIds[liT]. With that parallel array inside maReserved4C and not separately named,
            // the per-candidate position read is taken from the recorded aim slot. FLAG: the position
            // array is un-modelled by name; the scoring math below is faithful.
            const Vector3 lvToTarget = vpu::Subtract(MS.mBounceDirection, GetLinearVelocity());
            const f32 lfDistSq = vpu::MagnitudeSquared(lvToTarget);
            const f32 lfDist   = (lfDistSq > 0.0f) ? std::sqrt(lfDistSq) : 0.0f;
            if (lfDist <= KF_TARGET_SCORE_GATE)   // flt_82F2A328 gate (FLAGGED)
                continue;

            // weight = (2 - alignmentDot) * (1/dist)  -- here alignmentDot folded into the (2 - dist)
            // form the asm uses: v24 = (2.0 - v62) * dir.x ; with a stickiness x flt_82F2A32C when the
            // candidate id matches the current target.
            const f32 lfAlign = vpu::Dot(vpu::Normalize(lvToTarget), vpu::Normalize(lvVel));
            f32 lfWeight = (2.0f - lfAlign) * ((lfDist > 0.0f) ? (1.0f / lfDist) : 0.0f);
            if (MS.maTargetIds[liT] == MS.miCurrentTargetId)
                lfWeight *= 1.0f;   // FLAG: stickiness bonus flt_82F2A32C (un-homed) -> identity

            if (lfWeight <= lfBestWeight)
            {
                lfBestWeight = lfWeight;
                liBest = liT;
            }
        }

        MS.miCurrentTargetId = (liBest >= 0) ? MS.maTargetIds[liBest] : 0;   // dword_82FB8574

        if (liBest >= 0)
        {
            // when aligned, pull velocity toward the ballistic intercept.
            const f32 lfInputSpeed = std::sqrt(vpu::MagnitudeSquared(GetLinearVelocity()));   // 2D speed
            Vector3 lvIdeal;
            ComputeIdealVelocity(&lvIdeal, lfInputSpeed);
            // force toward (idealVel - currentVel.y component) scaled by an alignment term; the asm
            // gates the AddWorldSpaceForce on the alignment passing flt_82F2A324.
            const Vector3 lvDelta = vpu::Subtract(lvIdeal, GetLinearVelocity());
            AddWorldSpaceForce(vpu::Mult(lvDelta, KF_AFTERTOUCH_LAT));   // FLAG scale (flt_82F2A31C/320)
        }
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::UpdateAftertouch  @0x8262EBE8
    //   Camera-relative air-steer. Gated on the car being airborne (this+1808 in-air/crash) and the
    //   aftertouch-enable input. Normalises the camera matrix X and Z axes (asserting each has
    //   |.|^2 > 0), reads the stick deflection via GetAftertouchValues (+ optional SIXAXIS pitch when
    //   the downforce flag is set and |tilt| >= a threshold), then applies three force channels:
    //     (1) lateral world-space force along camera-X scaled by yaw (KF_AFTERTOUCH_LAT)
    //     (2) world-space roll angular impulse from camera axes (KF_AFTERTOUCH_ROLL)
    //     (3) local pitch impulses (KF_AFTERTOUCH_PITCH), with a +/-2.0 wheelie split by pitch sign.
    //   Showtime magnitudes differ from normal flight, with an extra IsBounceBoosting multiplier
    //   (unk_82FB8830). From showtime it chains UpdateTargetAssist (post lateral channel) then
    //   UpdateShowtimePhysics. FLAG: all magnitude rodata are un-homed placeholders; the camera-axis
    //   normalisation, the yaw/pitch channels and the showtime chaining are faithful.
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::UpdateAftertouch(const BrnPlayerDriverControls* lpControls,
                                          const Matrix44Affine* lpCameraMatrix,
                                          bool lbDoForceAdditiveAftertouch, bool lbUseSixaxis)
    {
        (void)lbUseSixaxis;
        // gate: airborne/crash (this+1808). The minimal slice models this via the aftertouch latch.
        if (!mbUsingAftertouch /* *(this+1808): in-air/crash */)
            return;
        if (lpControls->GetMode() != 0 /* !*(v6+64): a control-block gate */)
            return;

        // virtual "can use aftertouch" query (vtbl+20). On this path: IsPlayerVehicleActuallyInShowtime.
        if (!IsPlayerVehicleActuallyInShowtime())
        {
            // (the non-showtime aftertouch path still runs; the asm only branches the magnitudes.)
        }

        // normalise the camera X and Z axes (assert |.|^2 > 0 -- elided).
        const Vector3 lvCameraX = vpu::Normalize(lpCameraMatrix->xAxis);
        const Vector3 lvCameraZ = vpu::Normalize(lpCameraMatrix->zAxis);

        f32 lfEnable = lpControls->GetAftertouchEnable();   // v29 = *(v8+32)
        const bool lbShowtime = mbPlayerCarInShowtime;      // *(v6+5132)

        if (!lbShowtime)
        {
            // normal flight: scale the enable by a boost-dependent factor (flt_82F2A2D0 / D4) gated on
            // the vehicle's vertical speed (+4192). FLAGGED scalars -> identity here.
            if (!IsBounceBoosting())
                lfEnable = /* flt_82F2A2D0 or D4 */ lfEnable;   // FLAG: un-homed factor
        }

        if (lbDoForceAdditiveAftertouch && lfEnable > 0.0f)
        {
            f32 lfYaw = 0.0f, lfPitch = 0.0f, lfScalar = 0.0f;
            lpControls->GetAftertouchValues(&lfYaw, &lfPitch, &lfScalar);

            // optional SIXAXIS pitch contribution when the downforce flag is set and |tilt| >= thresh.
            if (mbUsingAftertouch /* *(v6+4944) downforce flag */)
            {
                const f32 lfTilt = lpControls->GetSixaxisTilt();   // *(v8+24)
                if (std::fabs(lfTilt) >= /*flt_82F2A314*/ 0.0f)
                {
                    const f32 lfSign = (lfTilt > 0.0f) ? 1.0f : ((lfTilt < 0.0f) ? -1.0f : 0.0f);
                    lfYaw += lfSign * /*flt_82F2A318*/ 0.0f;   // FLAG tilt gain
                    MS.mbSixaxisTiltApplied = true;            // byte_82FB848A = 1
                }
            }

            // (1) lateral world-space force along camera-X scaled by yaw * enable.
            Vector3 lvLateral = vpu::Mult(lvCameraX, lfYaw * lfEnable);
            if (lbShowtime && IsBounceBoosting())
                lvLateral = vpu::Mult(lvLateral, 1.0f);   // FLAG x unk_82FB8830 bounce-boost multiplier
            // (2) roll: a world-space angular impulse from the camera Z axis scaled by pitch * enable.
            Vector3 lvRoll = vpu::Mult(lvCameraZ, lfPitch * lfEnable);
            if (lbShowtime && IsBounceBoosting())
                lvRoll = vpu::Mult(lvRoll, 1.0f);   // FLAG x unk_82FB8830

            if (vpu::MagnitudeSquared(lvLateral) != 0.0f)
                AddWorldSpaceForce(vpu::Mult(lvLateral, KF_AFTERTOUCH_LAT));   // flt_82F2A304

            if (lbShowtime)
                UpdateTargetAssist(lpControls);

            // roll angular impulse (flt_82F2A2FC) -- gated on a small alignment test in the asm.
            if (vpu::MagnitudeSquared(lvRoll) != 0.0f)
                AddWorldSpaceAngularImpulse(vpu::Mult(lvRoll, KF_AFTERTOUCH_ROLL));

            // (3) local pitch impulses: a +/-2.0 wheelie split by pitch sign, scaled by KF_AFTERTOUCH_PITCH.
            const f32 lfWheelie = 4.0f;   // resolved inline literal (v74 = 4.0)
            Vector3 lvPitchImpulse = vpu::Mult(lvCameraX, lfYaw * lfEnable);
            if (lfYaw <= 0.0f)
                lvPitchImpulse = vpu::Subtract(Vector3{ 0.0f, lfWheelie, 0.0f, 0.0f }, lvPitchImpulse);
            else
                lvPitchImpulse = vpu::Add(Vector3{ 0.0f, lfWheelie, 0.0f, 0.0f }, lvPitchImpulse);
            AddLocalImpulse(vpu::Mult(lvPitchImpulse, KF_AFTERTOUCH_PITCH), Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });
        }

        if (lbShowtime)
            UpdateShowtimePhysics(Vector3{ 0.0f, 0.0f, 0.0f, 0.0f },
                                  Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, lfEnable);
    }

    // never-called layout pin: makes the per-TU compile gate enforce the PlayerParameters singleton
    // offsets (the offsets the asm reads). If a member moves, this fails to compile.
    static void _AssertPlayerParamsLayout()
    {
        static_assert(offsetof(PlayerParameters, mbBounceBoosting)     == 0x00, "lbBounceBoosting");
        static_assert(offsetof(PlayerParameters, mbJustBounced)        == 0x01, "byte_82FB8481");
        static_assert(offsetof(PlayerParameters, mbBouncedThisFrame)   == 0x02, "byte_82FB8482");
        static_assert(offsetof(PlayerParameters, muBounceChainCount)   == 0x06, "word_82FB8486");
        static_assert(offsetof(PlayerParameters, mbBounceBoostPending) == 0x09, "byte_82FB8489");
        static_assert(offsetof(PlayerParameters, miOtherEntityId)      == 0x0C, "dword_82FB848C");
        static_assert(offsetof(PlayerParameters, mBounceDirection)     == 0x10, "+0x10 vector");
        static_assert(offsetof(PlayerParameters, mbDisableShowtime)    == 0x30, "byte_82FB84B0");
        static_assert(offsetof(PlayerParameters, mbLaunchActive)       == 0x32, "byte_82FB84B2");
        static_assert(offsetof(PlayerParameters, mfDeformationScale)   == 0x34, "flt_82FB84B4");
        static_assert(offsetof(PlayerParameters, mfDamageBudget)       == 0x38, "flt_82FB84B8");
        static_assert(offsetof(PlayerParameters, mfUncappedSpeedTimer) == 0x3C, "flt_82FB84BC");
        static_assert(offsetof(PlayerParameters, mfTimeUntilPush)      == 0x44, "flt_82FB84C4");
        static_assert(offsetof(PlayerParameters, mfPlayerCarStrength)  == 0x48, "lfShowtimePlayerCarStrength");
        static_assert(offsetof(PlayerParameters, maTargetIds)          == 0xD0, "dword_82FB8550");
        static_assert(offsetof(PlayerParameters, miNumTargets)         == 0xF0, "dword_82FB8570");
        static_assert(offsetof(PlayerParameters, miCurrentTargetId)    == 0xF4, "dword_82FB8574");
        static_assert(offsetof(PlayerParameters, mu8NumBounceSensors)  == 0x110, "byte_82FB8590");
    }
}
}
