// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/RaceCarEntityModule/AirTime/BrnAirTimeManager.cpp
//
// BrnWorld::AirTimeManager::Update (X360 0x822F8C88) -- the per-car airborne-time state
// machine, stepped each PostPhysics frame by BrnWorld::RaceCarEntityModule::PostPhysicsUpdate.
//
// The X360 reads three RaceCarState fields by their committed byte offsets:
//   +0x404 mfGas               (airborne indicator: > 0 while the car is in the air)
//   +0x44A mbResetCarTransform (the car was reset -> drop the run)
//   +0x44E mbIsHidden          (the car is hidden -> drop the run)
// and walks { meState, mfTimeSinceLastAir, mfPreviousAirTime, mfTotalAirTime } at +0/+4/+8/+0xC,
// pushing InAir events (type 69 / 'E', payload { mfTotalAirTime, mfDeltaThisFrame }) onto the
// game-event queue via VariableEventQueue<1536,16>::AddEvent.
//
// Reconstructed store-for-store from the X360 asm. The OutputAirEvent helper is inlined by
// the X360 build (the two AddEvent call sites). The default-case Begin/Fire/EndAssert triple
// is reproduced as a house CGS_ASSERT (baked file/line discarded per project convention).
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/AirTime/BrnAirTimeManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                      // CGS_ASSERT
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"                // BrnPhysics::Vehicle::RaceCarState (complete)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                        // VariableEventQueue<1536,16>

namespace BrnWorld
{

// DWARF BrnAirTimeManager.cpp:27 (X360 rodata flt_82004EF4 == 4.0). The "in-between-air"
// grace window, and the value mfTimeSinceLastAir is primed to on a fresh run.
static const f32 KF_TIME_BETWEEN_AIR = 4.0f;

// The InAir event payload (type 69 / 'E', 8 bytes): the run's total air time so far and the
// air-time delta accrued this frame. Read back by BurnoutSkillzManager as InAirEvent.mfTimeInAir.
struct InAirEvent : public CgsModule::Event
{
    f32 mfTotalAirTime;     // +0x0
    f32 mfDeltaThisFrame;   // +0x4
};

// @ 0x822F8C88
void AirTimeManager::Update(const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState,
                            f32 lfSimTimerTimeStep,
                            GameEventQueue* lpEventQueue)
{
    CGS_ASSERT(lpEventQueue != 0, "lpEventQueue");
    CGS_ASSERT(lpRaceCarState != 0, "lpRaceCarState");

    // +0x404 mfGas (airborne indicator), +0x44A mbResetCarTransform, +0x44E mbIsHidden.
    const f32  lfAirborne   = lpRaceCarState->mfGas;
    const bool lbResetOrHidden =
        lpRaceCarState->mbResetCarTransform || lpRaceCarState->mbIsHidden;

    // A reset/hidden car forces the machine back to CRASHING and runs the case-0 logic.
    if (lbResetOrHidden)
        meState = E_AIR_STATE_CRASHING;

    switch (meState)
    {
        case E_AIR_STATE_CRASHING:   // 0
            // Once the car has settled (not reset and off the air indicator), prime a fresh run.
            if (!lpRaceCarState->mbResetCarTransform && lfAirborne == 0.0f)
            {
                mfPreviousAirTime  = 0.0f;            // +0x8
                mfTotalAirTime     = 0.0f;            // +0xC
                mfTimeSinceLastAir = KF_TIME_BETWEEN_AIR; // +0x4
                meState            = E_AIR_STATE_NONE;    // 3
            }
            break;

        case E_AIR_STATE_IN_BETWEEN_AIR:   // 1
        {
            mfTimeSinceLastAir += lfSimTimerTimeStep;   // +0x4 += dt
            if (lfAirborne > 0.0f)
            {
                meState = E_AIR_STATE_IN_AIR;   // 2 -> fall through into the IN_AIR body
                goto label_in_air;
            }
            if (mfTimeSinceLastAir < KF_TIME_BETWEEN_AIR)
            {
                // Still within the grace window: keep the run alive, report it (delta 0).
                InAirEvent lEvent;
                lEvent.mfTotalAirTime   = mfTotalAirTime;   // v7[3]
                lEvent.mfDeltaThisFrame = 0.0f;
                lpEventQueue->AddEvent(&lEvent, 69, 8);
            }
            else
            {
                // Grace window elapsed: end the run.
                mfTotalAirTime = 0.0f;          // +0xC
                meState        = E_AIR_STATE_NONE;  // 3
            }
            break;
        }

        case E_AIR_STATE_IN_AIR:   // 2
        label_in_air:
            if (lfAirborne == 0.0f)
            {
                // Just landed: bank this run's air time and drop to IN_BETWEEN_AIR.
                mfTotalAirTime    = mfPreviousAirTime + mfTotalAirTime; // +0xC = +0x8 + +0xC
                mfTimeSinceLastAir = 0.0f;                              // +0x4
                meState            = E_AIR_STATE_IN_BETWEEN_AIR;        // 1
            }
            else
            {
                // Still airborne: track the latest airborne value.
                mfPreviousAirTime = lfAirborne;   // +0x8 = mfGas
            }
            {
                // Report the run: total + this frame's airborne contribution.
                InAirEvent lEvent;
                lEvent.mfTotalAirTime   = mfTotalAirTime + lfAirborne; // v7[3] + mfGas
                lEvent.mfDeltaThisFrame = lfAirborne;
                lpEventQueue->AddEvent(&lEvent, 69, 8);
            }
            break;

        case E_AIR_STATE_NONE:   // 3
            if (lfAirborne > 0.0f)
                meState = E_AIR_STATE_IN_AIR;   // 2
            break;

        default:
            CGS_ASSERT(false, "Air time manager in unknown state!");
            break;
    }
}

} // namespace BrnWorld
