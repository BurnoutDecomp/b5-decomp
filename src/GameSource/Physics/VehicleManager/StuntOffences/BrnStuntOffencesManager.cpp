#include "GameSource/Physics/VehicleManager/StuntOffences/BrnStuntOffencesManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"   // RaceCarPhysics (GetTransform/GetAngularVelocity/GetLinearVelocity/GetNumberOfWheelsOnTheGround/IsCrashing)
#include "GameSource/Math/BrnMathUtils.h"                                      // BrnMath::Flatten
#include "GameShared/GameClasses/Core/CgsAssert.h"                             // CgsDev::Assert::{Begin,Fire,End}Assert
#include "rw/math/vpu/vector3_operation.h"                                     // rw::math::vpu::{Dot, Add, Subtract, Mult, Normalize, Magnitude, MagnitudeSquared, Max, Abs}
#include "rw/math/vpu/matrix44affine_operation.h"                             // rw::math::vpu::InverseOfMatrixWithOrthonormal3x3, operator*
#include <cmath>      // std::atan2, std::fabs, std::sqrt, std::cos, std::floor
#include <cstddef>    // offsetof
#include <cstring>    // std::memcpy

namespace vpu = rw::math::vpu;

// ============================================================================================
// MODULE-STATIC rdata thresholds. DWARF declares these as file-scope `extern const float32_t`
// (BrnStuntOffencesManager.h:260-271). FLAG: the X360 inlined the literals into the function asm,
// so the NUMERIC seeds below are the resolved per-comparison float constants from the pseudocode
// (deg/rad/sec). Every value is from the asm and noted at its use site; the symbolic names mirror
// the DWARF. None are fabricated -- where a constant was NOT in the exports it is flagged in-line.
// ============================================================================================
const f32 KF_MIN_TIME_IN_THE_AIR                      = 0.38f;   // SetCurrentCarInAirStatus: airtime > 0.38s -> count as a jump
const f32 KF_MIN_ANGLE_FOR_AIR_SPIN                   = 200.0f;  // CheckForRollsAndSpins (completed) deg/s gate
const f32 KF_MIN_ANGLE_FOR_BARREL_ROLL_COMPLETED      = 30.0f;   // CheckForRollsAndSpins (completed) deg/s gate
const f32 KF_MIN_ANGLE_FOR_BARREL_ROLL_IN_PROGRESS    = 35.0f;   // CheckForRollsAndSpins (in-progress) deg/s gate
const f32 KF_MAX_HANDBREAK_HOLD_TIME                  = 1.0f;    // CheckForHandBreakTurns: stabilise window (s)
const f32 KF_MIN_FOR_HANDBREAK_TURN                   = 90.0f;   // CheckForHandBreakTurns: deg accumulated -> handbrake turn
const f32 KF_HANDBRAKE_STABLE_END_TIME                = 1.0f;    // CheckForHandBreakTurns: end-of-turn stable time (s)
const f32 KF_MAX_TIME_FOR_CLEAN_LANDING_CHECK         = 0.2f;    // CheckForCleanLanding: window (s)
const f32 KF_MAX_TIME_FOR_SUCCESSFUL_LANDING_CHECK    = 1.0f;    // FLAG: successful-landing countdown seed (s); see CheckForSuccessfulLanding
const f32 KF_MAX_ANGLE_FOR_CLEAN_LANDING              = 0.17453292f; // CheckForCleanLanding: 10deg in rad (cos-cone test)
const f32 KF_MIN_AMOUNT_AIR_TIME_FOR_CLEAN_LANDING    = 0.75f;   // CheckForCleanLanding: min airtime (s)
const f32 KF_MIN_AMOUNT_AIR_TIME_FOR_SUCCESSFUL_LANDING = 0.38f; // CheckForSuccessfulLanding: min airtime (s)

namespace
{
    // NOTE: on X360 sizeof(RaceCarPhysics)==5216 and the Update spine indexes the array as
    // 5216*idx + base. Host pointer width differs, so the bodies index by typed pointer (&array[idx]).

    // FLAG: the per-driver record is passed as void* (its real type is GameState/AI-side, not homed by
    // this physics TU). The X360 reads its "state" word at +0xD0 (224-byte stride) and keeps a car when
    // that word is 2 (active) or 0. Modelled as a raw POD read -- the driver record is a non-polymorphic
    // array so the byte offset is host-stable; still flagged as a layout assumption.
    constexpr s32 KI_DRIVER_STRIDE       = 224;   // 0xE0
    constexpr s32 KI_DRIVER_STATE_OFFSET = 208;   // +0xD0
    inline bool DriverIsTailgatable(const void* lpaDrivers, s32 liIndex)
    {
        const s32 liState = *reinterpret_cast<const s32*>(
            reinterpret_cast<const u8*>(lpaDrivers) + KI_DRIVER_STRIDE * liIndex + KI_DRIVER_STATE_OFFSET);
        return liState == 2 || liState == 0;
    }

    // X360 source file string used by every CgsDev::Assert::FireAssert in this TU.
    const char* const KPC_SRC =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Physics/VehicleManager/StuntOffences/BrnStuntOffencesManager.cpp";

    inline void FireAssert(const char* lpcExpr, s32 liLine)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lpcExpr, KPC_SRC, liLine);
        CgsDev::Assert::EndAssert();
    }

    constexpr f32 KF_RAD_TO_DEG = 57.29578f;   // flt_8208F5F8
}

namespace BrnPhysics
{

    // ============================================================================================
    // @0x82642408  Update -- per-frame spine for the player's active car.
    // ============================================================================================
    void StuntOffencesManager::Update(Vehicle::RaceCarPhysics* lpaRaceCarPhysics,
                                      void* lpaRaceCarDrivers,
                                      BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue,
                                      EActiveRaceCarIndex lePlayerActiveRaceCarIndex,
                                      const CgsContainers::BitArray<8>* lpUsedRaceCars,
                                      f32 lfTimeStep)
    {
        if (!lpaRaceCarPhysics) FireAssert("lpaRaceCarPhysics != NULL", 76);
        if (!lpGameEventQueue)  FireAssert("lpGameEventQueue != NULL", 77);
        if (!lpUsedRaceCars)    FireAssert("lpUsedRaceCars != NULL", 78);
        if (lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_0)
            FireAssert("lePlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", 79);

        if (lePlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_COUNT)
        {
            FireAssert("lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", 80);
        }
        else if (lePlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0)
        {
            // 5216 * idx + base  ==  &lpaRaceCarPhysics[idx]  (host indexes by typed pointer)
            Vehicle::RaceCarPhysics* lpCar = &lpaRaceCarPhysics[lePlayerActiveRaceCarIndex];

            SetCurrentCarInAirStatus(lpCar, lfTimeStep);
            CheckForTakenOffLanding(lpCar);
            UpdateInAirRotations(lpCar, lpGameEventQueue, lfTimeStep);
            CheckForRollsAndSpins(lpCar, lpGameEventQueue, lfTimeStep);
            CheckForHandBreakTurns(lpCar, lpGameEventQueue, lfTimeStep);
            CheckForCleanLanding(lpCar, lpGameEventQueue, lfTimeStep);
            CheckForSuccessfulLanding(lpCar, lfTimeStep);
            CheckForDrift(lpCar, lpGameEventQueue, lfTimeStep);
            CheckForConvoy(lpaRaceCarPhysics, lpaRaceCarDrivers, lePlayerActiveRaceCarIndex,
                           lpUsedRaceCars, lfTimeStep);
        }

        OutputStuntsCompleted(lpGameEventQueue);

        // shift IN_THE_AIR_NOW(4) -> IN_THE_AIR_LAST_FRAME(8), clearing all other state bits.
        muCurrentRaceCarState = (muCurrentRaceCarState << 1) & E_CURRENT_CAR_STATE_IN_THE_AIR_LAST_FRAME;
    }

    // ============================================================================================
    // @0x826135A8  SetCurrentCarInAirStatus -- maintain the IN_THE_AIR_NOW state + air timer.
    // ============================================================================================
    void StuntOffencesManager::SetCurrentCarInAirStatus(Vehicle::RaceCarPhysics* lpRaceCarPhysics,
                                                        f32 lfTimeStep)
    {
        if (!lpRaceCarPhysics) FireAssert("lpRaceCarPhysics != NULL", 268);

        // count wheels on the ground (per-wheel on-ground bytes @ +0x158/+0x238/+0x318/+0x3F8).
        s32 liWheelsOnGround = lpRaceCarPhysics->GetNumberOfWheelsOnTheGround();
        const bool lbIsCrashing = lpRaceCarPhysics->IsCrashing();   // +0x710

        // If the car was RESET (bit5, 0x20) or is crashing, clear the air-distance/landing state +
        // re-snapshot takeoff position. Mask 0xFFFFFFEB clears IN_THE_AIR_NOW(4) | LANDING(16).
        if ((muCurrentRaceCarState & E_CURRENT_CAR_STATE_CAR_HAS_BEEN_RESET) != 0 || lbIsCrashing)
        {
            // asm zeroes _R31[8]/[40]/[41]/[104]/[105] = +0x20/+0xA0/+0xA4/+0x1A0/+0x1A4.
            mfTimeInTheAirSoFar    = 0.0f;          // +0x20  _R31[8]
            mfDistanceInAirSoFar   = 0.0f;          // +0xA0  _R31[40]
            mfDistanceOfLastJump   = 0.0f;          // +0xA4  _R31[41]
            mfCompletedAir         = 0.0f;          // +0x1A0 _R31[104]
            mfCompletedAirDistance = 0.0f;          // +0x1A4 _R31[105]
            muCurrentRaceCarState &= ~(E_CURRENT_CAR_STATE_IN_THE_AIR_NOW
                                       | E_CURRENT_CAR_STATE_LANDING);   // 0xFFFFFFEB
            mvPositionAtTakeoff = BrnMath::Flatten(lpRaceCarPhysics->GetPosition());
        }

        // Decide whether the car is airborne now.
        bool lbInAirNow = false;
        if (liWheelsOnGround != 0)
        {
            // 1..3 wheels + been airborne > 1.0s + not crashing -> LANDING (touching down).
            // (asm flt_82001C98 == 1.0; distinct from the 0.38 jump-counts gate below.)
            if (liWheelsOnGround > 0 && liWheelsOnGround < 4
                && mfTimeInTheAirSoFar > 1.0f && !lbIsCrashing)
            {
                muCurrentRaceCarState |= E_CURRENT_CAR_STATE_LANDING;
                lbInAirNow = true;   // skip the clear below
            }
        }
        else
        {
            // 0 wheels + the physics "should be airborne" gate (+0x1350) + not crashing -> IN_AIR_NOW.
            if (lpRaceCarPhysics->IsConsideredAirborne() && !lbIsCrashing)
            {
                muCurrentRaceCarState |= E_CURRENT_CAR_STATE_IN_THE_AIR_NOW;
                lbInAirNow = true;
            }
        }
        if (!lbInAirNow)
        {
            muCurrentRaceCarState &= ~(E_CURRENT_CAR_STATE_IN_THE_AIR_NOW
                                       | E_CURRENT_CAR_STATE_LANDING
                                       | E_CURRENT_CAR_STATE_JUST_LANDED);
            mvPositionAtTakeoff = BrnMath::Flatten(lpRaceCarPhysics->GetPosition());
        }

        if ((muCurrentRaceCarState & E_CURRENT_CAR_STATE_IN_THE_AIR_NOW) != 0)
        {
            // airborne: accumulate air time; > 0.38s flags JUMP_DISTANCE in progress; accumulate
            // horizontal distance from the takeoff position.
            mfTimeInTheAirSoFar += lfTimeStep;
            mfLastAirTime = mfTimeInTheAirSoFar;   // asm stores the post-increment air time to BOTH +0x20 and +0x24
            muStuntActionInProgress |= E_STUNT_ACTION_IN_PROGRESS_IN_AIR;
            if (mfTimeInTheAirSoFar >= KF_MIN_TIME_IN_THE_AIR)
                muStuntActionInProgress |= E_STUNT_ACTION_IN_PROGRESS_JUMP_DISTANCE;

            Vector2 lvCurrentPosition = BrnMath::Flatten(lpRaceCarPhysics->GetPosition());
            mfDistanceInAirSoFar = vpu::Magnitude(vpu::Subtract(Vector3{ lvCurrentPosition.x, lvCurrentPosition.y, 0.0f, 0.0f },
                                                                Vector3{ mvPositionAtTakeoff.x, mvPositionAtTakeoff.y, 0.0f, 0.0f }));
        }
        else
        {
            // grounded: if we WERE airborne, commit the jump (flag AIR complete, store air distance).
            if (mfTimeInTheAirSoFar > 0.0f)
            {
                muStuntActionComplete |= E_STUNT_ACTION_COMPLETE_AIR;
                mfCompletedAir = mfTimeInTheAirSoFar;
                if (mfDistanceInAirSoFar > mfDistanceOfLastJump)   // keep the larger of the two lanes
                    mfDistanceOfLastJump = mfDistanceInAirSoFar;
            }
            mfTimeInTheAirSoFar  = 0.0f;
            mfDistanceInAirSoFar = 0.0f;
        }
    }

    // ============================================================================================
    // @0x825BB078  CheckForTakenOffLanding -- edge-detect takeoff / land transitions.
    // ============================================================================================
    void StuntOffencesManager::CheckForTakenOffLanding(Vehicle::RaceCarPhysics* lpRaceCarPhysics)
    {
        if (!lpRaceCarPhysics) FireAssert("lpRaceCarPhysics != NULL", 367);

        // was-airborne(bit8) && !airborne-now(bit4)  -> JUST_LANDED(bit2)
        if ((muCurrentRaceCarState & E_CURRENT_CAR_STATE_IN_THE_AIR_LAST_FRAME) != 0
            && (muCurrentRaceCarState & E_CURRENT_CAR_STATE_IN_THE_AIR_NOW) == 0)
        {
            muCurrentRaceCarState |= E_CURRENT_CAR_STATE_JUST_LANDED;
        }

        // !was-airborne(bit8) && airborne-now(bit4)  -> JUST_TAKEN_OFF(bit1); compute reverse-takeoff.
        if ((muCurrentRaceCarState & E_CURRENT_CAR_STATE_IN_THE_AIR_LAST_FRAME) == 0
            && (muCurrentRaceCarState & E_CURRENT_CAR_STATE_IN_THE_AIR_NOW) != 0)
        {
            muCurrentRaceCarState |= E_CURRENT_CAR_STATE_JUST_TAKEN_OFF;
            if (!mbKeepCheckingForCleanLanding)   // +0x80 == 0 (asm reads this byte here)
            {
                // dot(forward axis = transform.zAxis @+0x30, linear velocity @+0x50) < 0 -> took off reversed.
                Matrix44Affine lTransform = lpRaceCarPhysics->GetTransform();
                const f32 lfDot = vpu::Dot(lTransform.zAxis, lpRaceCarPhysics->GetLinearVelocity());
                mbTookOffInReverse = (lfDot < 0.0f);
            }
        }
    }

    // ============================================================================================
    // @0x825BB168  UpdateInAirRotations -- integrate body angular velocity (car-space) while airborne.
    // ============================================================================================
    void StuntOffencesManager::UpdateInAirRotations(Vehicle::RaceCarPhysics* lpRaceCarPhysics,
                                                    BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue,
                                                    f32 lfTimeStep)
    {
        if (!lpRaceCarPhysics) FireAssert("lpRaceCarPhysics != NULL", 414);
        if (!lpGameEventQueue) FireAssert("lpGameEventQueue != NULL", 415);

        if ((muCurrentRaceCarState & E_CURRENT_CAR_STATE_IN_THE_AIR_NOW) != 0)
        {
            // transform the WORLD angular velocity (+0x60) into car space (inverse of the orthonormal
            // 3x3 of the car transform) and accumulate: mvCurrentInAirRotations += R^-1 * w * dt.
            Matrix44Affine lInverseCarTransform =
                vpu::InverseOfMatrixWithOrthonormal3x3(lpRaceCarPhysics->GetTransform());
            Vector3 lAngularVelocityInCarSpace =
                vpu::TransformVector(lInverseCarTransform, lpRaceCarPhysics->GetAngularVelocity());
            mvCurrentInAirRotations = vpu::Add(mvCurrentInAirRotations,
                                               vpu::Mult(lAngularVelocityInCarSpace, lfTimeStep));
        }
        else
        {
            mvCurrentInAirRotations = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        }
    }

    // ============================================================================================
    // @0x8263B508  CheckForRollsAndSpins -- score AIR SPINS (.y axis) + BARREL ROLLS (.z axis).
    //   mvStuntRollInProgress (+0x10): .y(+0x14, _R31[5]) = air-spin axis, .z(+0x18, _R31[6]) =
    //   barrel-roll axis (rad/s in car space).  NOTE: the axis<->stunt mapping is the OPPOSITE of the
    //   intuitive naming -- this matches the X360 asm verbatim (verified vs 0x8263B508):
    //     .y * deg > threshold  ->  AIR_SPIN flag + air-spin angle member
    //     .z * deg > threshold  ->  BARREL_ROLL flag + barrel-roll angle member + whole-roll count
    // ============================================================================================
    void StuntOffencesManager::CheckForRollsAndSpins(Vehicle::RaceCarPhysics* lpRaceCarPhysics,
                                                     BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue,
                                                     f32 lfTimeStep)
    {
        if (!lpRaceCarPhysics) FireAssert("lpRaceCarPhysics != NULL", 1182);
        if (!lpGameEventQueue) FireAssert("lpGameEventQueue != NULL", 1183);

        const bool lbIsCrashing = lpRaceCarPhysics->IsCrashing();
        const bool lbInAirNow   = (muCurrentRaceCarState & E_CURRENT_CAR_STATE_IN_THE_AIR_NOW) != 0;
        const bool lbReset      = (muCurrentRaceCarState & E_CURRENT_CAR_STATE_CAR_HAS_BEEN_RESET) != 0;

        if (lbInAirNow && !lbIsCrashing && !lbReset)
        {
            // ACTIVELY AIRBORNE: accumulate the abs current rotation into mvStuntRollInProgress
            // (per-lane max with the running accumulator), then test the in-progress thresholds.
            mvStuntRollInProgress = vpu::Max(vpu::Abs(mvCurrentInAirRotations), mvStuntRollInProgress);

            const f32 lfSpinDeg = mvStuntRollInProgress.y * KF_RAD_TO_DEG;   // _R31[5], air-spin axis
            const f32 lfRollDeg = mvStuntRollInProgress.z * KF_RAD_TO_DEG;   // _R31[6], barrel-roll axis

            // .y * deg > 30 -> AIR_SPIN in progress; store air-spin angle; clear the handbrake-turn
            // accumulator (asm stfs flt_82001CC0(=0.0) @+0x54 and stb 0 @+0x5C).
            if (lfSpinDeg > KF_MIN_ANGLE_FOR_BARREL_ROLL_COMPLETED)   // 30.0
            {
                muStuntActionInProgress |= E_STUNT_ACTION_IN_PROGRESS_AIR_SPIN;   // |= 2
                mfHandBreakAngleSoFar = 0.0f;                                     // +0x54  _R31[21]
                mbHandbreakTurnAttempting = false;                               // +0x5C
                mfInProgressAirSpinAngle = mvStuntRollInProgress.y;              // +0x1B4 _R31[109]
            }
            // .z * deg > 35 -> BARREL_ROLL in progress; store barrel-roll angle.
            if (lfRollDeg > KF_MIN_ANGLE_FOR_BARREL_ROLL_IN_PROGRESS)   // 35.0
            {
                muStuntActionInProgress |= E_STUNT_ACTION_IN_PROGRESS_BARREL_ROLL;   // |= 1
                mfInProgressBarrelRollAngle = mvStuntRollInProgress.z;               // +0x1B0 _R31[108]
            }
        }
        else
        {
            // NOT actively airborne (grounded / crashing / reset). On the JUST_LANDED frame (asm gates
            // this whole block additionally on bit1 of muCurrentRaceCarState), finalise the stunt.
            if (!lbIsCrashing && !lbReset
                && (muCurrentRaceCarState & E_CURRENT_CAR_STATE_JUST_LANDED) != 0)
            {
                const f32 lfSpinDeg = mvStuntRollInProgress.y * KF_RAD_TO_DEG;   // _R31[5], air-spin axis
                const f32 lfRollDeg = mvStuntRollInProgress.z * KF_RAD_TO_DEG;   // _R31[6], barrel-roll axis

                // .y * deg > 30 -> AIR_SPIN complete; store the completed air-spin angle.
                if (lfSpinDeg > KF_MIN_ANGLE_FOR_BARREL_ROLL_COMPLETED)   // 30.0
                {
                    muStuntActionComplete |= E_STUNT_ACTION_COMPLETE_AIR_SPIN;   // |= 2
                    mfCompletedAirSpinAngle = mvStuntRollInProgress.y;           // +0x18C _R31[99]
                }
                // .z * deg > 200 -> BARREL_ROLL complete; store the angle + the whole-roll count
                // (asm fsel truncate of (rollDeg * (1/360)) + 0.5 -> miCompletedBarrelRolls +0x19C).
                if (lfRollDeg > KF_MIN_ANGLE_FOR_AIR_SPIN)   // 200.0
                {
                    muStuntActionComplete |= E_STUNT_ACTION_COMPLETE_BARREL_ROLL;   // |= 1
                    mfCompletedBarrelRollAngle = mvStuntRollInProgress.z;           // +0x188 _R31[98]
                    const f32 lfTurns = (lfRollDeg * 0.0027777778f) + 0.5f;         // flt_82004920 == 1/360
                    miCompletedBarrelRolls = static_cast<s32>(lfTurns);            // +0x19C _R31[103] (truncate)
                }
                // a counted barrel roll PLUS a >=35deg air-spin latches the "spin training" complete bit.
                if (miCompletedBarrelRolls >= 1
                    && (mfCompletedAirSpinAngle * KF_RAD_TO_DEG) >= KF_MIN_ANGLE_FOR_BARREL_ROLL_IN_PROGRESS)   // 35.0
                {
                    muStuntActionComplete |= 0x800u;   // FLAG: bit 0x800 (air-spin training) not in the shared enum
                    miCompletedAirSpinTurns = 1;        // +0x1AC _R31[107]
                }
            }

            // when crashing or reset and the barrel-roll axis is still spinning (>35deg), fire a
            // training event.
            if ((lbIsCrashing || lbReset)
                && (mvStuntRollInProgress.z * KF_RAD_TO_DEG) > KF_MIN_ANGLE_FOR_BARREL_ROLL_IN_PROGRESS)   // 35.0
            {
                s32 liTrainingId = 49;   // RequestGameTrainingEvent payload (asm v20 = 49)
                reinterpret_cast<CgsModule::VariableEventQueue<1536, 16>*>(lpGameEventQueue)
                    ->AddEvent(reinterpret_cast<const CgsModule::Event*>(&liTrainingId), 113 /*0x71*/, 4);
            }

            // the finalize branch ALWAYS clears the running roll/spin accumulator at its tail
            // (asm vspltisw v0,0; stvx128 v0,r31,16 -- unconditional).
            mvStuntRollInProgress = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };   // +0x10 cleared
        }
    }

    // ============================================================================================
    // @0x825E3A38  CheckForHandBreakTurns -- accumulate heading change while the handbrake is active.
    // ============================================================================================
    void StuntOffencesManager::CheckForHandBreakTurns(Vehicle::RaceCarPhysics* lpRaceCarPhysics,
                                                      BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue,
                                                      f32 lfTimeStep)
    {
        if (!lpRaceCarPhysics) FireAssert("lpRaceCarPhysics != NULL", 1088);
        if (!lpGameEventQueue) FireAssert("lpGameEventQueue != NULL", 1089);

        // begin tracking when the physics drift-active timer (+0x109C) goes positive and we're not already.
        if (!mbHandbreakTurnAttempting && lpRaceCarPhysics->GetDriftActiveTime() > 0.0f)
        {
            mfBearingLastFrame      = 0.0f;
            mfHandBreakAngleSoFar   = 0.0f;
            mfHandBrakeStabiliseTime = 0.0f;
            mbHandbreakTurnAttempting = true;
            mvRaceCarPositionLastFrame = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        }

        if (mbHandbreakTurnAttempting)
        {
            // bearing from the car's FORWARD axis (transform zAxis, +0x30 column): atan2(z, x).
            // The +0x40 (wAxis/position) column is stored separately into mvRaceCarPositionLastFrame.
            // (asm: lvx128 v127, r28,+0x40 = position; lvx128 v0, r28,+0x30 = forward -> v21 -> atan2.)
            Matrix44Affine lTransform = lpRaceCarPhysics->GetTransform();
            const Vector3 lvForward = lTransform.zAxis;   // +0x30
            mvRaceCarPositionLastFrame = lTransform.wAxis;   // +0x40 (asm stvx128 v127,r31,+0x40)
            f32 lfBearing = static_cast<f32>(std::atan2(lvForward.z, lvForward.x));

            // delta vs last bearing, wrapped to (-pi, pi].
            f32 lfAngleChange;
            if (mfBearingLastFrame == 0.0f)
            {
                lfAngleChange = 0.0f;
            }
            else
            {
                lfAngleChange = lfBearing - mfBearingLastFrame;
                if (lfAngleChange > -3.1415927f)
                {
                    if (lfAngleChange >= 3.1415927f)
                        lfAngleChange = (lfBearing - mfBearingLastFrame) - 6.2831855f;
                }
                else
                {
                    lfAngleChange = (lfBearing - mfBearingLastFrame) + 6.2831855f;
                }
            }
            const f32 lfNewAngle = (lfAngleChange * KF_RAD_TO_DEG) + mfHandBreakAngleSoFar;
            mfBearingLastFrame    = lfBearing;
            mfHandBreakAngleSoFar = lfNewAngle;

            // > 90 deg accumulated -> handbrake turn in progress.
            if (std::fabs(lfNewAngle) > KF_MIN_FOR_HANDBREAK_TURN)
            {
                mfInProgressHandbreakTurnAngle = std::fabs(lfNewAngle);   // +0x1B8
                muStuntActionInProgress |= E_STUNT_ACTION_IN_PROGRESS_HANDBREAK_TURN;
            }

            // when the handbrake is RELEASED (+0x135B byte clears), wait KF_HANDBRAKE_STABLE_END_TIME
            // of stable driving, then commit the turn if one was registered.
            if (lpRaceCarPhysics->IsHandbrakeHeld())
            {
                const f32 lfStable = mfHandBrakeStabiliseTime + lfTimeStep;
                mfHandBrakeStabiliseTime = lfStable;
                if (lfStable >= KF_HANDBRAKE_STABLE_END_TIME)
                {
                    const bool lbWasTurn = (muStuntActionInProgress & E_STUNT_ACTION_IN_PROGRESS_HANDBREAK_TURN) != 0;
                    mbHandbreakTurnAttempting = false;
                    if (lbWasTurn)
                    {
                        mfCompletedHandbreakTurnAngle = std::fabs(lfNewAngle);   // +0x190
                        muStuntActionComplete |= E_STUNT_ACTION_COMPLETE_HANDBREAK_TURN;
                    }
                }
            }
            else
            {
                mfHandBrakeStabiliseTime = 0.0f;
            }
        }
    }

    // ============================================================================================
    // @0x82614580  CheckForCleanLanding -- a "clean" landing keeps the takeoff heading within ~10deg.
    // ============================================================================================
    void StuntOffencesManager::CheckForCleanLanding(Vehicle::RaceCarPhysics* lpRaceCarPhysics,
                                                    BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue,
                                                    f32 lfTimeStep)
    {
        (void)lpGameEventQueue;
        // CRASHING -> pure no-op early return (asm: bne loc_82614910 = function exit; no writes).
        if (lpRaceCarPhysics->IsCrashing())
            return;

        // JUST_TAKEN_OFF (bit0) -> clear the landing vector, snapshot the takeoff forward axis
        // (transform.zAxis @+0x30), reset the clean-landing timer, and EARLY-RETURN this frame.
        // (asm: if (muCurrentRaceCarState & 1) { mvLandingVector(+0x70)=0; mbKeepChecking(+0x80)=0;
        //       mvTakeOffVector(+0x60)=transform.zAxis; mfCleanLandingCheckTimeSoFar(+0x84)=0; return; })
        if ((muCurrentRaceCarState & E_CURRENT_CAR_STATE_JUST_TAKEN_OFF) != 0)
        {
            mvLandingVector = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
            mbKeepCheckingForCleanLanding = false;
            mvTakeOffVector = lpRaceCarPhysics->GetTransform().zAxis;
            mfCleanLandingCheckTimeSoFar = 0.0f;
            return;
        }

        // not just-taken-off: proceed only if LANDING (bit4) or already armed.
        if ((muCurrentRaceCarState & E_CURRENT_CAR_STATE_LANDING) == 0
            && !mbKeepCheckingForCleanLanding)
            return;

        // arm once we have been airborne long enough (asm reads mfLastAirTime +0x24 vs 0.75).
        if (mfLastAirTime > KF_MIN_AMOUNT_AIR_TIME_FOR_CLEAN_LANDING)
            mbKeepCheckingForCleanLanding = true;

        if (mbKeepCheckingForCleanLanding)
        {
            // FIX (asm 0x82614684: extrwi bit2 == IN_THE_AIR_NOW, inverted -- NOT JUST_LANDED/bit1).
            // Re-evaluate the takeoff-vs-landing heading every grounded frame the check stays armed,
            // not only on the single JUST_LANDED edge frame.
            if ((muCurrentRaceCarState & E_CURRENT_CAR_STATE_IN_THE_AIR_NOW) == 0)
            {
                Vector2 lv2TakeOffAtVector = BrnMath::Flatten(mvTakeOffVector);
                Vector2 lv2LandingAtVector = BrnMath::Flatten(lpRaceCarPhysics->GetTransform().zAxis);
                Vector3 lvTakeOff = vpu::Normalize(Vector3{ lv2TakeOffAtVector.x, lv2TakeOffAtVector.y, 0.0f, 0.0f });
                Vector3 lvLanding = vpu::Normalize(Vector3{ lv2LandingAtVector.x, lv2LandingAtVector.y, 0.0f, 0.0f });
                const f32 lfFinalLandingAngle = vpu::Dot(lvTakeOff, lvLanding);
                const f32 lfLandingDifference = std::cos(KF_MAX_ANGLE_FOR_CLEAN_LANDING);   // cos(10deg)
                if (lfFinalLandingAngle > lfLandingDifference)
                {
                    mbKeepCheckingForCleanLanding = false;
                    muStuntActionComplete |= E_STUNT_ACTION_COMPLETE_CLEANLANDING;
                    mvTakeOffVector = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
                    mvLandingVector = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
                }
            }
            // disarm if the check window expires.
            if (mfCleanLandingCheckTimeSoFar > KF_MAX_TIME_FOR_CLEAN_LANDING_CHECK)
                mbKeepCheckingForCleanLanding = false;
            mfCleanLandingCheckTimeSoFar += lfTimeStep;
        }
    }

    // ============================================================================================
    // @0x825BB320  CheckForSuccessfulLanding -- a "successful" (big-air) landing scores AIR + lands clean.
    // ============================================================================================
    void StuntOffencesManager::CheckForSuccessfulLanding(Vehicle::RaceCarPhysics* lpRaceCarPhysics,
                                                         f32 lfTimeStep)
    {
        const bool lbIsCrashing = lpRaceCarPhysics->IsCrashing();

        // arm when not crashing AND airborne > 0.38s AND all four wheels off the ground.
        if (!lbIsCrashing && mfTimeInTheAirSoFar > KF_MIN_AMOUNT_AIR_TIME_FOR_SUCCESSFUL_LANDING)
        {
            if (lpRaceCarPhysics->GetNumberOfWheelsOnTheGround() == 0)
                mbKeepCheckingForSuccessfulLanding = true;
        }

        if (mbKeepCheckingForSuccessfulLanding)
        {
            // crashing or reset -> abort the check (asm loc_825BB450). Sets mbSuccessfulLanding=0,
            // ORs SUCCESSFUL_LANDING(0x10) into muStuntActionComplete, resets the jump distance to 0.0
            // (flt_82001CC0) and disarms the countdown (-1.0, flt_820037C8). It does NOT touch
            // mfDistanceInAirSoFar. Then falls into the shared LABEL_21 reset.
            if (lbIsCrashing || (muCurrentRaceCarState & E_CURRENT_CAR_STATE_CAR_HAS_BEEN_RESET) != 0)
            {
                mbSuccessfulLanding = false;                                     // +0xAE
                muStuntActionComplete |= E_STUNT_ACTION_COMPLETE_SUCCESSFUL_LANDING;   // |= 0x10
                mfDistanceOfLastJump = 0.0f;                                     // +0xA4  flt_82001CC0 == 0.0
                mfSuccesssfulLandingCheckTimeSoFar = -1.0f;                      // +0xA8  flt_820037C8 == -1.0
                mbKeepCheckingForSuccessfulLanding = false;                      // +0xAC
                return;
            }

            // on touchdown (wheels regained) seed the countdown.
            if (lpRaceCarPhysics->GetNumberOfWheelsOnTheGround() > 0
                && mfSuccesssfulLandingCheckTimeSoFar < 0.0f)
            {
                mfSuccesssfulLandingCheckTimeSoFar = 1.0f;   // KF_MAX_TIME_FOR_SUCCESSFUL_LANDING_CHECK (flt_82001C98)
            }

            // count the countdown down; OR JUMP_DISTANCE(0x40) into muStuntActionInProgress each frame
            // while counting (asm *(v3+44)=v8|0x40). When the timer crosses 0 the landing is "successful".
            if (mfSuccesssfulLandingCheckTimeSoFar >= 0.0f)
            {
                const f32 lfPrev = mfSuccesssfulLandingCheckTimeSoFar - lfTimeStep;
                mfSuccesssfulLandingCheckTimeSoFar -= lfTimeStep;
                muStuntActionInProgress |= E_STUNT_ACTION_IN_PROGRESS_JUMP_DISTANCE;   // |= 0x40 into +0x2C
                if (lfPrev < 0.0f)
                {
                    mfCompletedAirDistance = mfDistanceOfLastJump;   // +0x1A4 = +0xA4
                    mbSuccessfulLanding = true;                      // +0xAE
                    // fire SUCCESSFUL_LANDING|JUMP_DISTANCE (asm v10 = v9 | 0x210).
                    muStuntActionComplete |= (E_STUNT_ACTION_COMPLETE_SUCCESSFUL_LANDING
                                              | E_STUNT_ACTION_COMPLETE_JUMP_DISTANCE);   // 0x10 | 0x200 = 0x210
                    mfDistanceOfLastJump = 0.0f;                     // +0xA4  flt_82001CC0 == 0.0 (LABEL_21 v6)
                    mfSuccesssfulLandingCheckTimeSoFar = -1.0f;      // +0xA8  flt_820037C8 == -1.0
                    mbKeepCheckingForSuccessfulLanding = false;      // +0xAC
                }
            }
        }
    }

    // ============================================================================================
    // @0x82613820  CheckForDrift -- score drift time + distance from the physics drift Z-speed.
    // ============================================================================================
    void StuntOffencesManager::CheckForDrift(Vehicle::RaceCarPhysics* lpRaceCarPhysics,
                                             BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue,
                                             f32 lfTimeStep)
    {
        (void)lpGameEventQueue;
        if (lpRaceCarPhysics->IsCrashing())
        {
            // crashing aborts a live drift (mark FAILED_DRIFT).
            if (mbWasDriftingLastFrame)
            {
                mbWasDriftingLastFrame = false;
                muStuntActionComplete |= E_STUNT_ACTION_COMPLETE_FAILED_DRIFT;   // 0x80
            }
            return;
        }

        // physics drift lateral Z-speed @+0x1010 (lane 2), clamped to >= 0 (asm vmaxfp vs 0).
        const f32 lfDriftZSpeedRaw = lpRaceCarPhysics->GetDriftLateralSpeed();
        const f32 lfDriftZSpeed = (lfDriftZSpeedRaw > 0.0f) ? lfDriftZSpeedRaw : 0.0f;
        mfTimeDriftingLastFrame = 0.0f;

        if (lfDriftZSpeed > 0.0f)
        {
            // DRIFTING. The "time" members (+0xB4/+0x194/+0x1BC) all receive the clamped drift LATERAL
            // SPEED v24[0] (a Burnout quirk -- not elapsed time, not dt). The distance increment
            // (velocity reg @+0x6C0 * unit_83017FE0 * drift-speed).x accumulates into mfCompletedDriftDistance
            // (+0x198) and mirrors into mfInProgressDriftDistance (+0x1C0).
            mbWasDriftingLastFrame = true;
            muStuntActionInProgress |= (E_STUNT_ACTION_IN_PROGRESS_DRIFT | E_STUNT_ACTION_IN_PROGRESS_DRIFT_DISTANCE);   // 0x18
            mfTimeDriftingLastFrame = lfDriftZSpeed;   // +0xB4  (asm stfs v24[0] @0xB4)
            mfInProgressDriftTime   = lfDriftZSpeed;   // +0x1BC (asm stfs v24[0] @0x1BC)
            mfCompletedDriftTime    = lfDriftZSpeed;   // +0x194 (asm stfs v24[0] @0x194)
            Vector3 lvVel = lpRaceCarPhysics->GetStuntReferenceVelocity();   // +0x6C0
            const f32 lfDistInc = lvVel.x;   // FLAG: asm splats the .x lane of (vel * unit_83017FE0 * drift-speed); unit not in exports -> modelled as the x-lane increment
            mfCompletedDriftDistance += lfDistInc;                 // +0x198 accumulator
            mfInProgressDriftDistance = mfCompletedDriftDistance;  // +0x1C0 mirrors +0x198
        }
        else if (mbWasDriftingLastFrame)
        {
            // drift ENDED cleanly this frame -> flag DRIFT + DRIFT_DISTANCE complete.
            mbWasDriftingLastFrame = false;
            muStuntActionComplete |= (E_STUNT_ACTION_COMPLETE_DRIFT | E_STUNT_ACTION_COMPLETE_DRIFT_DISTANCE);   // 0x60
        }
    }

    // ============================================================================================
    // @0x825BB290  IsWithinTailgatingCone -- gap<=40 along + alignment within the cone.
    // ============================================================================================
    bool StuntOffencesManager::IsWithinTailgatingCone(const Vector3& lvForward,
                                                      const Vector3& lvFrom, const Vector3& lvTo)
    {
        Vector3 lvDelta = vpu::Subtract(lvTo, lvFrom);
        // gap along the candidate's forward axis (v1 . delta) must be within 40.0.
        if (vpu::Dot(lvForward, lvDelta) > 40.0f)
            return false;
        // alignment of the normalized delta with forward must exceed the cone cos.
        const f32 lfAlign = vpu::Dot(vpu::Normalize(lvDelta), lvForward);
        return lfAlign >= 0.0f /*FLAG: flt_82FB9E78 (cone cos threshold) not in exports*/;
    }

    // ============================================================================================
    // @0x82613960 / @0x82613F68  GetTailgateeIndex / GetTailgaterIndex -- the nearest live car
    // directly ahead-of / behind the given car within the tailgating cone.
    //   These two are structurally identical (argmin over the live-car bitset, skipping self + cars
    //   whose driver-state @+0xD0 is not {0,2}, gated on the candidate's forward axis being ~aligned
    //   and within the cone). FLAG: the X360 inlines a CgsBitArray<8> first/next-set-bit scan; modelled
    //   here as a plain index loop over the bitset's IsBitSet. The cone "ahead vs behind" sense differs
    //   by which car supplies the forward axis (the candidate for tailgatee, the player for tailgater).
    // ============================================================================================
    s32 StuntOffencesManager::GetTailgateeIndex(EActiveRaceCarIndex leActiveRaceCarIndex,
                                                Vehicle::RaceCarPhysics* lpaRaceCarPhysics,
                                                void* lpaRaceCarDrivers,
                                                const CgsContainers::BitArray<8>* lpRaceCarsToCheck)
    {
        if (!lpRaceCarsToCheck) FireAssert("lpRaceCarsToCheck", 639);
        if (!lpaRaceCarPhysics) FireAssert("lpaRaceCarPhysics", 640);
        if (!lpaRaceCarDrivers) FireAssert("lpaRaceCarDrivers", 641);
        if (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_0)
            FireAssert("leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", 642);
        if (leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_COUNT)
            FireAssert("leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", 643);

        Vehicle::RaceCarPhysics* lpCar = &lpaRaceCarPhysics[leActiveRaceCarIndex];
        if (!lpCar) FireAssert("lpRaceCarPhysics", 646);

        // require the car to be moving forward fast enough (forward speed @+0x6C0 >= 30).
        if (!(lpCar->GetStuntReferenceVelocity().x >= 30.0f)) return -1;

        s32 liBest = -1;
        f32 lfBestDistSq = 3.4028235e38f;
        const Vector3 lvForward   = lpCar->GetStuntForwardAxis();   // FLAG: cone forward axis (normalized stunt velocity reg @+0x1340)
        const Vector3 lvSelfPos   = lpCar->GetStuntWorldPosition();

        for (s32 li = 0; li < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++li)
        {
            if (!lpRaceCarsToCheck->IsBitSet(static_cast<u32>(li))) continue;
            if (li == static_cast<s32>(leActiveRaceCarIndex)) continue;
            if (!DriverIsTailgatable(lpaRaceCarDrivers, li)) continue;   // driver-state @+0xD0 in {0,2}

            Vehicle::RaceCarPhysics* lpOther = &lpaRaceCarPhysics[li];
            if (!lpOther) FireAssert("lpRaceCarToCheckPhysics", 687);
            if (!(lpOther->GetStuntReferenceVelocity().x >= 30.0f)) continue;   // candidate also moving (>30)

            const Vector3 lvOtherPos = lpOther->GetStuntWorldPosition();
            if (IsWithinTailgatingCone(lvForward, lvSelfPos, lvOtherPos))
            {
                const f32 lfDistSq = vpu::MagnitudeSquared(vpu::Subtract(lvSelfPos, lvOtherPos));
                if (liBest == -1 || lfDistSq < lfBestDistSq)
                {
                    liBest = li;
                    lfBestDistSq = lfDistSq;
                }
            }
        }
        return liBest;
    }

    s32 StuntOffencesManager::GetTailgaterIndex(EActiveRaceCarIndex leActiveRaceCarIndex,
                                                Vehicle::RaceCarPhysics* lpaRaceCarPhysics,
                                                void* lpaRaceCarDrivers,
                                                const CgsContainers::BitArray<8>* lpRaceCarsToCheck)
    {
        if (!lpRaceCarsToCheck) FireAssert("lpRaceCarsToCheck", 757);
        if (!lpaRaceCarPhysics) FireAssert("lpaRaceCarPhysics", 758);
        if (!lpaRaceCarDrivers) FireAssert("lpaRaceCarDrivers", 759);
        if (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_0)
            FireAssert("leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", 760);
        if (leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_COUNT)
            FireAssert("leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", 761);

        Vehicle::RaceCarPhysics* lpCar = &lpaRaceCarPhysics[leActiveRaceCarIndex];
        if (!lpCar) FireAssert("lpRaceCarPhysics", 764);

        if (!(lpCar->GetStuntReferenceVelocity().x >= 30.0f)) return -1;

        s32 liBest = -1;
        f32 lfBestDistSq = 3.4028235e38f;
        const Vector3 lvSelfPos = lpCar->GetStuntWorldPosition();

        for (s32 li = 0; li < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++li)
        {
            if (!lpRaceCarsToCheck->IsBitSet(static_cast<u32>(li))) continue;
            if (li == static_cast<s32>(leActiveRaceCarIndex)) continue;
            if (!DriverIsTailgatable(lpaRaceCarDrivers, li)) continue;   // driver-state @+0xD0 in {0,2}

            Vehicle::RaceCarPhysics* lpOther = &lpaRaceCarPhysics[li];
            if (!lpOther) FireAssert("lpRaceCarToCheckPhysics", 831);
            if (!(lpOther->GetStuntReferenceVelocity().x >= 30.0f)) continue;

            // tailgater: the OTHER car's forward axis points at us (it is behind, aiming forward at us).
            const Vector3 lvOtherFwd = lpOther->GetStuntForwardAxis();   // FLAG: normalized stunt velocity reg
            const Vector3 lvOtherPos = lpOther->GetStuntWorldPosition();
            if (IsWithinTailgatingCone(lvOtherFwd, lvOtherPos, lvSelfPos))
            {
                const f32 lfDistSq = vpu::MagnitudeSquared(vpu::Subtract(lvOtherPos, lvSelfPos));
                if (liBest == -1 || lfDistSq < lfBestDistSq)
                {
                    liBest = li;
                    lfBestDistSq = lfDistSq;
                }
            }
        }
        return liBest;
    }

    // ============================================================================================
    // @0x82628EC8  CheckForConvoy -- build the convoy chain centred on the player car.
    //   Walk forward (tailgatees the player is behind) and backward (tailgaters behind the player),
    //   collecting active-car indices into a local list, then per-link accumulate convoy timer +
    //   distance, flag the in-progress / complete convoy bits, and -- when the chain shrinks -- emit the
    //   completed-convoy slots.
    // ============================================================================================
    void StuntOffencesManager::CheckForConvoy(Vehicle::RaceCarPhysics* lpaRaceCarPhysics,
                                              void* lpaRaceCarDrivers,
                                              EActiveRaceCarIndex lePlayerActiveRaceCarIndex,
                                              const CgsContainers::BitArray<8>* lpUsedRaceCars,
                                              f32 lfTimeStep)
    {
        // mutable working copy of the live-car bitset (each linked car is removed as it's consumed).
        CgsContainers::BitArray<8> lWorkingSet = *lpUsedRaceCars;

        s32 laChain[8];
        s32 liChainLen = 1;
        laChain[0] = static_cast<s32>(lePlayerActiveRaceCarIndex);

        // FORWARD: cars the player is tailgating (player is their tailgatee). Walk while the cone holds.
        if (lWorkingSet.IsBitSet(static_cast<u32>(lePlayerActiveRaceCarIndex)))
        {
            s32 liCur = static_cast<s32>(lePlayerActiveRaceCarIndex);
            while (liChainLen < 8)
            {
                lWorkingSet.UnSetBit(static_cast<u32>(liCur));
                s32 liNext = GetTailgateeIndex(static_cast<EActiveRaceCarIndex>(liCur),
                                               lpaRaceCarPhysics, lpaRaceCarDrivers, &lWorkingSet);
                if (liNext == -1) break;
                // prepend (the forward chain grows ahead of the player)
                for (s32 lj = liChainLen; lj > 0; --lj) laChain[lj] = laChain[lj - 1];
                laChain[0] = liNext;
                ++liChainLen;
                liCur = liNext;
            }
        }

        // BACKWARD: cars tailgating the player (player is their tailgater), appended after the chain.
        if (lpUsedRaceCars->IsBitSet(static_cast<u32>(lePlayerActiveRaceCarIndex)) && liChainLen < 8)
        {
            s32 liCur = static_cast<s32>(lePlayerActiveRaceCarIndex);
            while (liChainLen < 8)
            {
                s32 liNext = GetTailgaterIndex(static_cast<EActiveRaceCarIndex>(liCur),
                                               lpaRaceCarPhysics, lpaRaceCarDrivers, &lWorkingSet);
                if (liNext == -1) break;
                laChain[liChainLen] = liNext;
                ++liChainLen;
                liCur = liNext;
            }
        }

        // when the convoy chain SHRANK vs last frame, finalise the dropped links + flag CONVOY complete.
        mbConvoyComplete = false;   // +0x1A8 cleared each frame
        const s32 liPrevLen = miConvoyCount;   // +0x118
        if (liChainLen < liPrevLen)
        {
            for (s32 li = liPrevLen - 1; li > liChainLen - 1; --li)
            {
                // move the live timer/distance into the completed slots, flag the link, clear the live.
                maCompletedTailgateB[li] = maConvoyTimer[li];      // +0x13C[i] <- +0xD8[i]
                maCompletedTailgateC[li] = maConvoyDistance2[li];  // +0x15C[i] <- +0xF8[i]
                maCompletedTailgateFlag[li] = 1;                   // +0x17C[i] = 1
                maConvoyTimer[li]     = 0.0f;
                maConvoyDistance2[li] = 0.0f;
            }
            miCompletedConvoyCount = miConvoyCount;   // +0x184 <- +0x118
            if (liChainLen <= 1) mbConvoyComplete = true;   // convoy fully broken (asm stb 1,+0x1A8)
            muStuntActionComplete |= 0x400u;   // FLAG: convoy-complete bit (not in the shared EStuntActionComplete enum; asm ORs 0x400 into +0x30)
        }

        // commit the new chain length + copy the chain indices into the per-link id block (+0xB8, 32B).
        // The +0xB8 array (maConvoyDistance) doubles as the chain-index store (asm XMemCpy(a1+184,...)).
        std::memcpy(maConvoyDistance, laChain, 32);
        miConvoyCount = liChainLen;

        // when the convoy has >=2 cars, accumulate per-link convoy distance + timer. The asm walks
        // 1-BASED indices (1..count-1) and -- despite the array names -- stores the DISTANCE increment
        // into maConvoyTimer[i] (+0xD8) and dt into maConvoyDistance2[i] (+0xF8). The velocity reg is
        // the PLAYER car's @+0x6C0 (asm _R8 = 5216*playerIndex + raceCarArray + 0x6C0, constant in loop).
        if (liChainLen > 1)
        {
            Vehicle::RaceCarPhysics* lpLinkCar = &lpaRaceCarPhysics[lePlayerActiveRaceCarIndex];
            Vector3 lvVel = lpLinkCar->GetStuntReferenceVelocity();   // +0x6C0
            const f32 lfDistInc = lvVel.x * lfTimeStep;   // FLAG: asm splats x-lane of (vel*unit_83017FE0)*dt; unit not in exports
            for (s32 li = 1; li < liChainLen; ++li)
            {
                maConvoyTimer[li]     += lfDistInc;     // +0xD8[i] += distance increment (asm *(v39-8) += v50)
                maConvoyDistance2[li] += lfTimeStep;    // +0xF8[i] += dt              (asm *v39 += a6)
                muStuntActionInProgress |= 0x80u;       // FLAG: asm ORs 0x80 into +0x2C each convoy link; bit 0x80 not in the shared EStuntActionInProgress enum (convoy in-progress)
            }
        }
    }

    // ============================================================================================
    // @0x8263B278  OutputStuntsInProgress -- mirror live stunt scalars into the RaceCarState + push.
    // ============================================================================================
    void StuntOffencesManager::OutputStuntsInProgress(RaceCarState* lpRaceCarState,
                                                      BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue)
    {
        if (!lpRaceCarState) FireAssert("lpRaceCarState != NULL", 166);

        // copy the 6 live scalars into the RaceCarState (word offsets +0x41C..+0x438). FLAG: written by
        // raw offset because RaceCarState is GameState-side and not homed here; see shared_header_grows.
        u8* lpRcs = reinterpret_cast<u8*>(lpRaceCarState);
        std::memcpy(lpRcs + 0x438, &muStuntActionInProgress,        4);   // a2[270]
        std::memcpy(lpRcs + 0x41C, &mfInProgressBarrelRollAngle,    4);   // a2[263]
        std::memcpy(lpRcs + 0x420, &mfInProgressAirSpinAngle,       4);   // a2[264]
        std::memcpy(lpRcs + 0x424, &mfInProgressHandbreakTurnAngle, 4);   // a2[265]
        std::memcpy(lpRcs + 0x428, &mfInProgressDriftTime,          4);   // a2[266]
        std::memcpy(lpRcs + 0x42C, &mfInProgressDriftDistance,      4);   // a2[267]

        if (!lpGameEventQueue) FireAssert("lpGameEventQueue != NULL", 177);

        if (muStuntActionInProgress != 0)
        {
            // pack the InProgressStuntEvent (148 bytes) and push it. Field offsets are the asm stack
            // layout (event base = &v10 = sp+0x50): the action word, then the THREE 32-byte convoy
            // blobs (timer/dist2/dist) immediately after it (+0x04/+0x24/+0x44), then miConvoyCount and
            // the scalars (+0x64..+0x80), then mbTookOffInReverse at +0x90.
            struct InProgressStuntEvent
            {
                u32 muStuntActionInProgress;            // +0x00  v10  = *(this+11)
                u8  mConvoyTimers[32];                  // +0x04  XMemCpy(this+54  == +0xD8, 32)  [v11]
                u8  mConvoyDist2[32];                   // +0x24  XMemCpy(this+62  == +0xF8, 32)  [v12]
                u8  mConvoyDist[32];                    // +0x44  XMemCpy(this+46  == +0xB8, 32)  [v13]
                s32 miConvoyCount;                      // +0x64  v14  = *(this+70) (+0x118)
                f32 mfBarrelRollAngle;                  // +0x68  v15  = this[108] (+0x1B0)
                f32 mfAirSpinAngle;                     // +0x6C  v16  = this[109] (+0x1B4)
                f32 mfHandbreakTurnAngle;               // +0x70  v17  = this[110] (+0x1B8)
                f32 mfDriftTime;                        // +0x74  v18  = this[111] (+0x1BC)
                f32 mfDriftDistance;                    // +0x78  v19  = this[112] (+0x1C0)
                f32 mfTimeInTheAirSoFar;                // +0x7C  v20  = this[8]   (+0x20)
                f32 mfMaxJumpDistance;                  // +0x80  v21  = fsel(this[41]-this[40], this[41], this[40])
                u8  mReserved84[0x90 - 0x84];           // +0x84  (gap to +0x90)
                u8  mbTookOffInReverse;                 // +0x90  v22  = *(this+175) (+0xAF)
                u8  mReserved91[148 - 0x91];            // +0x91  (pad to 148 bytes)
            } lEvent;
            std::memset(&lEvent, 0, sizeof(lEvent));
            lEvent.muStuntActionInProgress = muStuntActionInProgress;
            std::memcpy(lEvent.mConvoyTimers, maConvoyTimer,     32);   // +0xD8
            std::memcpy(lEvent.mConvoyDist2,  maConvoyDistance2, 32);   // +0xF8
            std::memcpy(lEvent.mConvoyDist,   maConvoyDistance,  32);   // +0xB8
            lEvent.miConvoyCount        = miConvoyCount;            // +0x118
            lEvent.mfBarrelRollAngle    = mfInProgressBarrelRollAngle;
            lEvent.mfAirSpinAngle       = mfInProgressAirSpinAngle;
            lEvent.mfHandbreakTurnAngle = mfInProgressHandbreakTurnAngle;
            lEvent.mfDriftTime          = mfInProgressDriftTime;
            lEvent.mfDriftDistance      = mfInProgressDriftDistance;
            lEvent.mfTimeInTheAirSoFar  = mfTimeInTheAirSoFar;
            // max of the two jump-distance lanes (+0xA4 mfDistanceOfLastJump, +0xA0 mfDistanceInAirSoFar).
            const f32 lfDiff = mfDistanceOfLastJump - mfDistanceInAirSoFar;
            lEvent.mfMaxJumpDistance = (lfDiff > 0.0f) ? mfDistanceOfLastJump : mfDistanceInAirSoFar;   // fsel
            lEvent.mbTookOffInReverse   = mbTookOffInReverse ? 1 : 0;
            reinterpret_cast<CgsModule::VariableEventQueue<1536, 16>*>(lpGameEventQueue)
                ->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent), 120 /*0x78*/, 148 /*0x94*/);
        }

        // clear the in-progress block.
        mfInProgressBarrelRollAngle    = 0.0f;
        muStuntActionInProgress        = 0;
        mfInProgressAirSpinAngle       = 0.0f;
        mfInProgressHandbreakTurnAngle = 0.0f;
        mfInProgressDriftTime          = 0.0f;
        mfInProgressDriftDistance      = 0.0f;
    }


    // ============================================================================================
    // @0x8263B3E8  OutputStuntsCompleted -- pack + push the completed-stunt record, then reset.
    // ============================================================================================
    void StuntOffencesManager::OutputStuntsCompleted(BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue)
    {
        if (!lpGameEventQueue) FireAssert("lpGameEventQueue != NULL", 220);

        if (muStuntActionComplete != 0)
        {
            // pack the CompletedStuntEvent (132 bytes). Field offsets are the asm stack layout
            // (event base = &v9 = sp+0x50): the action word, then the THREE blobs (tailgate B/A/flag)
            // immediately after it (+0x04/+0x24/+0x44), then the convoy/roll/spin counts (+0x4C/+0x50/
            // +0x54), the 7 completed floats (+0x58..+0x70), and the 3 flag bytes (+0x80/+0x81/+0x82).
            struct CompletedStuntEvent
            {
                u32 muStuntActionComplete;     // +0x00  v9   = this[12]   (+0x30)
                u8  mTailgateB[32];            // +0x04  XMemCpy(this+79 == +0x13C, 32) [v10]
                u8  mTailgateA[32];            // +0x24  XMemCpy(this+71 == +0x11C, 32) [v11]
                u8  mTailgateFlag[8];          // +0x44  XMemCpy(this+95 == +0x17C, 8)  [v12]
                s32 miConvoyCount;             // +0x4C  v13  = this[97]   (+0x184)
                s32 miBarrelRolls;             // +0x50  v14  = this[103]  (+0x19C)
                s32 miAirSpinTurns;            // +0x54  v15  = this[107]  (+0x1AC)
                f32 mfBarrelRollAngle;         // +0x58  v16  = *(this+98) (+0x188)
                f32 mfAirSpinAngle;            // +0x5C  v17  = *(this+99) (+0x18C)
                f32 mfHandbreakTurnAngle;      // +0x60  v18  = *(this+100)(+0x190)
                f32 mfDriftTime;               // +0x64  v19  = *(this+101)(+0x194)
                f32 mfDriftDistance;           // +0x68  v20  = *(this+102)(+0x198)
                f32 mfAir;                     // +0x6C  v21  = *(this+104)(+0x1A0)
                f32 mfAirDistance;             // +0x70  v22  = *(this+105)(+0x1A4)
                u8  mReserved74[0x80 - 0x74];  // +0x74  (gap to the flag bytes at +0x80)
                u8  mbSuccessfulLanding;       // +0x80  v23  = *(this+174)(+0xAE)
                u8  mbTookOffInReverse;        // +0x81  v24  = *(this+175)(+0xAF)
                u8  mbConvoyComplete;          // +0x82  v25  = *(this+424)(+0x1A8)
                u8  mReserved83[132 - 0x83];   // +0x83  (pad to 132 bytes)
            } lEvent;
            std::memset(&lEvent, 0, sizeof(lEvent));
            lEvent.muStuntActionComplete = muStuntActionComplete;
            std::memcpy(lEvent.mTailgateB,    maCompletedTailgateB,    32);   // event+0x04 <- +0x13C
            std::memcpy(lEvent.mTailgateA,    maCompletedTailgateA,    32);   // event+0x24 <- +0x11C
            std::memcpy(lEvent.mTailgateFlag, maCompletedTailgateFlag,  8);   // event+0x44 <- +0x17C
            lEvent.miConvoyCount         = miCompletedConvoyCount;      // +0x184
            lEvent.miBarrelRolls         = miCompletedBarrelRolls;      // +0x19C
            lEvent.miAirSpinTurns        = miCompletedAirSpinTurns;     // +0x1AC
            lEvent.mfBarrelRollAngle     = mfCompletedBarrelRollAngle;  // +0x188
            lEvent.mfAirSpinAngle        = mfCompletedAirSpinAngle;     // +0x18C
            lEvent.mfHandbreakTurnAngle  = mfCompletedHandbreakTurnAngle; // +0x190
            lEvent.mfDriftTime           = mfCompletedDriftTime;        // +0x194
            lEvent.mfDriftDistance       = mfCompletedDriftDistance;    // +0x198
            lEvent.mfAir                 = mfCompletedAir;              // +0x1A0
            lEvent.mfAirDistance         = mfCompletedAirDistance;     // +0x1A4
            lEvent.mbSuccessfulLanding   = mbSuccessfulLanding ? 1 : 0;
            lEvent.mbTookOffInReverse    = mbTookOffInReverse ? 1 : 0;
            lEvent.mbConvoyComplete      = mbConvoyComplete ? 1 : 0;     // +0x1A8
            reinterpret_cast<CgsModule::VariableEventQueue<1536, 16>*>(lpGameEventQueue)
                ->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent), 119 /*0x77*/, 132 /*0x84*/);
            ResetCompleteOutputValues();
        }
    }

    // ============================================================================================
    // Layout pin. Never called.
    // ============================================================================================
    void StuntOffencesManager::_AssertLayout()
    {
        static_assert(offsetof(StuntOffencesManager, mvCurrentInAirRotations)  == 0x000, "0x000");
        static_assert(offsetof(StuntOffencesManager, mvStuntRollInProgress)    == 0x010, "0x010");
        static_assert(offsetof(StuntOffencesManager, mfTimeInTheAirSoFar)      == 0x020, "0x020");
        static_assert(offsetof(StuntOffencesManager, mfLastAirTime)            == 0x024, "0x024");
        static_assert(offsetof(StuntOffencesManager, muCurrentRaceCarState)    == 0x028, "0x028");
        static_assert(offsetof(StuntOffencesManager, muStuntActionInProgress)  == 0x02C, "0x02C");
        static_assert(offsetof(StuntOffencesManager, muStuntActionComplete)    == 0x030, "0x030");
        static_assert(offsetof(StuntOffencesManager, mvRaceCarPositionLastFrame) == 0x040, "0x040");
        static_assert(offsetof(StuntOffencesManager, mfBearingLastFrame)       == 0x050, "0x050");
        static_assert(offsetof(StuntOffencesManager, mfHandBreakAngleSoFar)    == 0x054, "0x054");
        static_assert(offsetof(StuntOffencesManager, mfHandBrakeStabiliseTime) == 0x058, "0x058");
        static_assert(offsetof(StuntOffencesManager, mbHandbreakTurnAttempting)== 0x05C, "0x05C");
        static_assert(offsetof(StuntOffencesManager, mvTakeOffVector)          == 0x060, "0x060");
        static_assert(offsetof(StuntOffencesManager, mvLandingVector)          == 0x070, "0x070");
        static_assert(offsetof(StuntOffencesManager, mbKeepCheckingForCleanLanding) == 0x080, "0x080");
        static_assert(offsetof(StuntOffencesManager, mfCleanLandingCheckTimeSoFar)  == 0x084, "0x084");
        static_assert(offsetof(StuntOffencesManager, mvPositionAtTakeoff)      == 0x090, "0x090");
        static_assert(offsetof(StuntOffencesManager, mfDistanceInAirSoFar)     == 0x0A0, "0x0A0");
        static_assert(offsetof(StuntOffencesManager, mfDistanceOfLastJump)     == 0x0A4, "0x0A4");
        static_assert(offsetof(StuntOffencesManager, mfSuccesssfulLandingCheckTimeSoFar) == 0x0A8, "0x0A8");
        static_assert(offsetof(StuntOffencesManager, mbKeepCheckingForSuccessfulLanding) == 0x0AC, "0x0AC");
        static_assert(offsetof(StuntOffencesManager, mbInAirLastFrame)         == 0x0AD, "0x0AD");
        static_assert(offsetof(StuntOffencesManager, mbSuccessfulLanding)      == 0x0AE, "0x0AE");
        static_assert(offsetof(StuntOffencesManager, mbTookOffInReverse)       == 0x0AF, "0x0AF");
        static_assert(offsetof(StuntOffencesManager, mbWasDriftingLastFrame)   == 0x0B0, "0x0B0");
        static_assert(offsetof(StuntOffencesManager, mfTimeDriftingLastFrame)  == 0x0B4, "0x0B4");
        static_assert(offsetof(StuntOffencesManager, maConvoyDistance)         == 0x0B8, "0x0B8");
        static_assert(offsetof(StuntOffencesManager, maConvoyTimer)            == 0x0D8, "0x0D8");
        static_assert(offsetof(StuntOffencesManager, maConvoyDistance2)        == 0x0F8, "0x0F8");
        static_assert(offsetof(StuntOffencesManager, miConvoyCount)            == 0x118, "0x118");
        static_assert(offsetof(StuntOffencesManager, maCompletedTailgateA)     == 0x11C, "0x11C");
        static_assert(offsetof(StuntOffencesManager, maCompletedTailgateB)     == 0x13C, "0x13C");
        static_assert(offsetof(StuntOffencesManager, maCompletedTailgateC)     == 0x15C, "0x15C");
        static_assert(offsetof(StuntOffencesManager, maCompletedTailgateFlag)  == 0x17C, "0x17C");
        static_assert(offsetof(StuntOffencesManager, miCompletedConvoyCount)   == 0x184, "0x184");
        static_assert(offsetof(StuntOffencesManager, mfCompletedBarrelRollAngle) == 0x188, "0x188");
        static_assert(offsetof(StuntOffencesManager, miCompletedBarrelRolls)   == 0x19C, "0x19C");
        static_assert(offsetof(StuntOffencesManager, mfCompletedAir)           == 0x1A0, "0x1A0");
        static_assert(offsetof(StuntOffencesManager, mfCompletedAirDistance)   == 0x1A4, "0x1A4");
        static_assert(offsetof(StuntOffencesManager, miCompletedAirSpinTurns)  == 0x1AC, "0x1AC");
        static_assert(offsetof(StuntOffencesManager, mfInProgressBarrelRollAngle)  == 0x1B0, "0x1B0");
        static_assert(offsetof(StuntOffencesManager, mfInProgressDriftDistance)    == 0x1C0, "0x1C0");
        // X360 last member ends at 0x1C4; the embedded rw::math::vpu::Vector3/Vector2 are alignas(16),
        // so the host ABI rounds sizeof up to the next 16 (0x1D0 == 464). Every recovered offset is
        // pinned by the offsetof asserts below, so the rounded sizeof loses no fidelity.
        static_assert(sizeof(StuntOffencesManager) == 464, "sizeof StuntOffencesManager == 464 (0x1C4 rounded to 16-byte align)");
    }
}
